/*
 * ytps5-sponsorblock payload
 *
 * Injected into the official PS5 YouTube (Cobalt) app by the native module.
 * It removes ad objects from YouTube API responses and skips SponsorBlock
 * segments in the <video> element.
 */
(function () {
  'use strict';

  if (window.__YTPS5_SPONSORBLOCK_LOADED__) return;
  window.__YTPS5_SPONSORBLOCK_LOADED__ = true;

  var LOG = '[ytps5]';
  var API = 'https://sponsor.ajay.app/api/skipSegments';
  var CATEGORIES = [
    'sponsor',
    'intro',
    'outro',
    'interaction',
    'selfpromo',
    'preview',
    'filler',
    'music_offtopic'
  ];

  function log() {
    try {
      var args = Array.prototype.slice.call(arguments);
      args.unshift(LOG);
      console.log.apply(console, args);
    } catch (e) {}
  }

  function showToast(title, subtitle) {
    try {
      var popupAction = {
        openPopupAction: {
          popupType: 'TOAST',
          popup: {
            overlayToastRenderer: {
              title: { simpleText: title },
              subtitle: { simpleText: subtitle || '' }
            }
          }
        }
      };
      for (var key in window._yttv) {
        var item = window._yttv[key];
        if (item && item.instance && item.instance.resolveCommand) {
          item.instance.resolveCommand(popupAction);
          return;
        }
      }
    } catch (e) {}
  }

  /* ---------------------------------------------------------------- */
  /* 1. Ad removal: prune ad objects before the UI renders them.      */
  /* ---------------------------------------------------------------- */
  var origParse = JSON.parse;
  JSON.parse = function () {
    var result = origParse.apply(this, arguments);
    if (result && typeof result === 'object' && !Array.isArray(result)) {
      if (result.adPlacements) delete result.adPlacements;
      if (result.playerAds) delete result.playerAds;
      if (result.adSlots) delete result.adSlots;
      if (result.adBreakHeartbeatParams) delete result.adBreakHeartbeatParams;
    }
    return result;
  };

  window.JSON.parse = JSON.parse;
  try {
    for (var key in window._yttv) {
      var item = window._yttv[key];
      if (item && item.JSON && item.JSON.parse) {
        item.JSON.parse = JSON.parse;
      }
    }
  } catch (e) {}

  /* ---------------------------------------------------------------- */
  /* 2. SponsorBlock state.                                            */
  /* ---------------------------------------------------------------- */
  var segments = [];
  var currentVideo = null;
  var currentVideoId = null;
  var skipTimer = null;
  var skipped = {};
  var fetchSeq = 0;

  function getVideoId() {
    try {
      var href = window.location.href || '';
      var hash = window.location.hash || '';
      var match = (href + hash).match(/[?&#]v=([A-Za-z0-9_-]{6,})/);
      if (match) return match[1];
      match = hash.match(/(?:^|[?&#])v=([A-Za-z0-9_-]{6,})/);
      return match ? match[1] : null;
    } catch (e) {
      return null;
    }
  }

  function apiUrl(videoId) {
    return (
      API +
      '?videoID=' +
      encodeURIComponent(videoId) +
      '&categories=' +
      encodeURIComponent(JSON.stringify(CATEGORIES))
    );
  }

  function parseSegments(text) {
    var data = origParse(text);
    if (!Array.isArray(data)) return [];
    return data.filter(function (item) {
      return (
        item &&
        Array.isArray(item.segment) &&
        item.segment.length >= 2 &&
        isFinite(item.segment[0]) &&
        isFinite(item.segment[1]) &&
        item.segment[1] > item.segment[0]
      );
    });
  }

  function applySegments(videoId, list) {
    if (videoId !== currentVideoId) return;
    segments = list || [];
    skipped = {};
    if (segments.length) {
      log('SponsorBlock:', segments.length, 'segment(s) for', videoId);
      showToast('SponsorBlock', segments.length + ' segment(s) found');
      scheduleSkip();
    } else {
      log('SponsorBlock: no segments for', videoId);
    }
  }

  function tryLocalProxy(videoId, seq) {
    try {
      var xhr = new XMLHttpRequest();
      xhr.timeout = 2500;
      xhr.onload = function () {
        if (seq !== fetchSeq) return;
        if (xhr.status === 200) {
          try {
            applySegments(videoId, parseSegments(xhr.responseText));
          } catch (e) {
            log('proxy parse failed', e);
          }
        }
      };
      xhr.onerror = xhr.ontimeout = function () {
        log('SponsorBlock proxy unavailable');
      };
      xhr.open('GET', 'http://127.0.0.1:4040/' + encodeURIComponent(videoId), true);
      xhr.send();
    } catch (e) {
      log('proxy exception', e);
    }
  }

  function loadSponsorBlock(videoId) {
    if (!videoId) return;
    var seq = ++fetchSeq;

    log('SponsorBlock: fetching', videoId);
    if (typeof fetch === 'function') {
      fetch(apiUrl(videoId), { credentials: 'omit', mode: 'cors' })
        .then(function (response) {
          if (response.status === 404) return [];
          if (!response.ok) throw new Error('HTTP ' + response.status);
          return response.json();
        })
        .then(function (data) {
          if (seq !== fetchSeq) return;
          applySegments(videoId, Array.isArray(data) ? data : []);
        })
        .catch(function (error) {
          log('direct SponsorBlock fetch failed:', String(error));
          if (seq === fetchSeq) tryLocalProxy(videoId, seq);
        });
    } else {
      tryLocalProxy(videoId, seq);
    }
  }

  /* ---------------------------------------------------------------- */
  /* 3. Segment skipping.                                              */
  /* ---------------------------------------------------------------- */
  function scheduleSkip() {
    if (skipTimer) {
      clearTimeout(skipTimer);
      skipTimer = null;
    }
    if (!currentVideo || currentVideo.paused || !segments.length) return;

    var now = currentVideo.currentTime;
    var next = null;
    for (var i = 0; i < segments.length; i++) {
      var seg = segments[i].segment;
      if (seg[1] > now + 0.2) {
        if (!next || seg[0] < next.segment[0]) next = segments[i];
      }
    }
    if (!next) return;

    var start = next.segment[0];
    var end = next.segment[1];
    var delay = Math.max(0, (start - now) * 1000);

    skipTimer = setTimeout(function () {
      skipTimer = null;
      if (!currentVideo || currentVideo.paused) return;
      if (currentVideo.currentTime < start - 1.5) {
        scheduleSkip();
        return;
      }

      var id =
        next.UUID ||
        next.category + '_' + start + '_' + end;
      var seen = skipped[id];
      if (seen) {
        seen.count++;
        if (Date.now() - seen.first < 1200) return;
      } else {
        skipped[id] = { count: 1, first: Date.now() };
      }

      log('skipping', next.category, start, '->', end);
      showToast(
        'Segment Skipped',
        next.category + ' (' + Math.floor(end - start) + 's)'
      );

      if (currentVideo.duration && currentVideo.duration - end < 1) {
        currentVideo.currentTime = Math.max(start, end - 1);
      } else {
        currentVideo.currentTime = end;
      }
      scheduleSkip();
    }, delay);
  }

  function detachVideo() {
    if (currentVideo) {
      currentVideo.removeEventListener('play', scheduleSkip);
      currentVideo.removeEventListener('pause', scheduleSkip);
      currentVideo.removeEventListener('timeupdate', scheduleSkip);
      currentVideo.removeEventListener('durationchange', scheduleSkip);
      currentVideo = null;
    }
    if (skipTimer) {
      clearTimeout(skipTimer);
      skipTimer = null;
    }
  }

  function attachVideo() {
    var video = document.querySelector('video');
    if (!video) {
      setTimeout(attachVideo, 500);
      return;
    }
    if (video === currentVideo) return;
    detachVideo();
    currentVideo = video;
    currentVideo.addEventListener('play', scheduleSkip);
    currentVideo.addEventListener('pause', scheduleSkip);
    currentVideo.addEventListener('timeupdate', scheduleSkip);
    currentVideo.addEventListener('durationchange', scheduleSkip);
    log('video element attached');
    scheduleSkip();
  }

  function onVideoChange() {
    var videoId = getVideoId();
    if (!videoId || videoId === currentVideoId) return;
    currentVideoId = videoId;
    segments = [];
    skipped = {};
    attachVideo();
    loadSponsorBlock(videoId);
  }

  window.addEventListener('hashchange', onVideoChange, false);
  window.addEventListener('popstate', onVideoChange, false);
  document.addEventListener('yt-navigate-finish', onVideoChange, true);

  /* The TV app is a SPA; poll the URL and the video element. */
  setInterval(onVideoChange, 1500);
  setInterval(attachVideo, 1500);

  onVideoChange();
  attachVideo();

  setTimeout(function () {
    showToast('Ad Block + SponsorBlock Enabled!', 'ytps5-sponsorblock');
  }, 2500);

  log('payload loaded');
})();
