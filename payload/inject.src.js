/*
 * ytps5-sponsorblock payload
 *
 * Injected into the official PS5 video app (Cobalt) by the native module.
 * It removes ad objects from app API responses and skips SponsorBlock
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
  var origParse = JSON.parse;

  var AUTHOR_NAME = 'master_shivnath';
  var CURRENT_VERSION = '1.1.0';
  var UPDATE_CHECK_URL = 'http://127.0.0.1:4040/update';
  var REPO_URL = 'https://github.com/MasterShivnath007/ytps5-sponsorblock';
  var DONATE_URL = 'https://github.com/sponsors/MasterShivnath007';
  var DONATE_QR = 'data:image/png;base64,iVBORw0KGgoAAAANSUhEUgAAALkAAAC5AQAAAABc1qPxAAABpUlEQVR42uVYMXKEMAwUoXDpJ/gn8DFmYIaP4Z/4CZQUDMpqTbqky2yRUGjujmItabUrn/n3z2Z//8VhZqn6XZKfVq75xPfBtw/74fnFF+YVZ0iPrW5T9uZ+ro7fVODINz35tmtwb0h/iUIowdmAei4lVT34YUu5zMZmsxY8el4j8yfjY5X2vLP9rXgEJdv5PDa6zSAcCxEzqAF/Ik5gux/5xpz7VoRlz0Bz3xv1BQGsE5Z9K2Y4gQ3sObQODZBl7o35XgMC+LcICQdw53Qzc9SAWicsu1NeJ1AeASovA3/yZtD2PSBfi7lmncLtHlMWnUb3Q2OTcs6LoeLkGlQ+h8XojGVtoXCY7r5MIPMqlNdmpNl89syFcx6EC3ehqJL84jlPBzcoDF3iWVxpqQjs9N53OOUmM7LnjTV41VY351Q4iIzhBAiLkHDIPHDDWCA3Kah3aTeZgxrbTwBLHaQ73Au+wNBOadm9j5rzvoCkb/neDsIBfGTPhX5O8AjdYmKZEN/V3qsCaz82ncJ99bx1U48lOknllfnC1WBtFnIzS+f8H/4t8gmU1IERk7/eawAAAABJRU5ErkJggg==';

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
  /* 0. Lazy ytps5 settings. Nothing here runs until the settings JSON  */
  /*    is parsed, i.e. after the app has already launched.             */
  /* ---------------------------------------------------------------- */
  var settingsConfig = {
    adBlock: true,
    sponsorBlock: true,
    categories: CATEGORIES.slice()
  };

  function settingsResolve(cmd) {
    for (var key in window._yttv) {
      var item = window._yttv[key];
      if (item && item.instance && item.instance.resolveCommand) {
        return item.instance.resolveCommand(cmd);
      }
    }
    return false;
  }

  function settingsButton(title, icon, commands) {
    if (typeof title === 'string') title = { title: title };
    var button = {
      compactLinkRenderer: {
        serviceEndpoint: {
          commandExecutorCommand: { commands: commands }
        }
      }
    };
    if (title && title.title) {
      button.compactLinkRenderer.title = { simpleText: title.title };
    }
    if (title && title.subtitle) {
      button.compactLinkRenderer.subtitle = { simpleText: title.subtitle };
    }
    if (icon) {
      button.compactLinkRenderer.icon = { iconType: icon.icon };
      if (icon.secondaryIcon) {
        button.compactLinkRenderer.secondaryIcon = {
          iconType: icon.secondaryIcon
        };
      }
    }
    return button;
  }

  function settingsItemList(items) {
    return { overlayPanelItemListRenderer: { items: items, selectedIndex: 0 } };
  }

  function settingsModal(header, content, id, update) {
    var cmd = {
      openPopupAction: {
        popupType: 'MODAL',
        popup: {
          overlaySectionRenderer: {
            overlay: {
              overlayTwoPanelRenderer: {
                actionPanel: {
                  overlayPanelRenderer: {
                    header: {
                      overlayPanelHeaderRenderer: {
                        title: { simpleText: header.title },
                        subtitle: { simpleText: header.subtitle || '' }
                      }
                    },
                    content: content
                  }
                },
                backButton: {
                  buttonRenderer: {
                    command: { signalAction: { signal: 'POPUP_BACK' } }
                  }
                }
              }
            },
            dismissalCommand: { signalAction: { signal: 'POPUP_BACK' } }
          }
        },
        uniqueId: id
      }
    };
    if (update) {
      cmd.openPopupAction.shouldMatchUniqueId = true;
      cmd.openPopupAction.updateAction = true;
    }
    settingsResolve(cmd);
  }

  function settingRow(title, itemId, endpoint, summary) {
    return {
      settingActionRenderer: {
        title: { runs: [{ text: title }] },
        serviceEndpoint: endpoint,
        summary: { runs: [{ text: summary }] },
        trackingParams: 'null',
        actionLabel: { runs: [{ text: title }] },
        itemId: itemId,
        thumbnail: {
          thumbnails: [{
            url: 'https://www.gstatic.com/ytlr/img/parent_code.png'
          }]
        }
      }
    };
  }

  function categoryLabel(name) {
    if (name === 'music_offtopic') return 'Music off-topic';
    if (name === 'selfpromo') return 'Self-promotion';
    return name.charAt(0).toUpperCase() + name.slice(1);
  }

  function settingsMessage(text) {
    return { overlayMessageRenderer: { title: { simpleText: text } } };
  }

  function settingsQr(url) {
    return {
      qrCodeRenderer: {
        qrCodeImage: { thumbnails: [{ url: url }] },
        style: 'QR_CODE_RENDERER_STYLE_ATA_SIDESHEET',
        trackingParams: 'null'
      }
    };
  }

  function showDonatePanel() {
    settingsModal(
      { title: 'Support ' + AUTHOR_NAME, subtitle: 'GitHub Sponsors' },
      settingsItemList([
        settingsMessage('Scan the QR code or visit ' + DONATE_URL),
        settingsQr(DONATE_QR)
      ]),
      'ytps5-donate', false
    );
  }

  function showSettingsPanel(update) {
    var rows = [];
    rows.push(settingsButton(
      { title: 'Support ' + AUTHOR_NAME, subtitle: 'GitHub Sponsors' },
      { icon: 'MONEY_HEART' },
      [{ customAction: { action: 'YTPS5_DONATE', parameters: [] } }]
    ));
    rows.push(settingsButton(
      { title: 'Ad Block', subtitle: settingsConfig.adBlock ? 'On' : 'Off' },
      { icon: 'DOLLAR_SIGN',
        secondaryIcon: settingsConfig.adBlock ? 'CHECK_BOX' : 'CHECK_BOX_OUTLINE_BLANK' },
      [{ customAction: { action: 'YTPS5_TOGGLE_ADBLOCK', parameters: [] } }]
    ));
    rows.push(settingsButton(
      { title: 'SponsorBlock',
        subtitle: settingsConfig.sponsorBlock ? 'On' : 'Off' },
      { icon: 'MONEY_HAND',
        secondaryIcon: settingsConfig.sponsorBlock ? 'CHECK_BOX' : 'CHECK_BOX_OUTLINE_BLANK' },
      [{ customAction: { action: 'YTPS5_TOGGLE_SB', parameters: [] } }]
    ));
    for (var i = 0; i < CATEGORIES.length; i++) {
      var name = CATEGORIES[i];
      var on = settingsConfig.categories.indexOf(name) >= 0;
      rows.push(settingsButton(
        { title: categoryLabel(name), subtitle: on ? 'On' : 'Off' },
        { icon: 'CHECK',
          secondaryIcon: on ? 'CHECK_BOX' : 'CHECK_BOX_OUTLINE_BLANK' },
        [{ customAction: { action: 'YTPS5_TOGGLE_CATEGORY', parameters: name } }]
      ));
    }
    rows.push(settingsButton(
      { title: 'Close' }, { icon: 'CHEVRON_LEFT' },
      [{ signalAction: { signal: 'POPUP_BACK' } }]
    ));
    settingsModal(
      { title: 'ytps5 Settings',
        subtitle: 'by ' + AUTHOR_NAME + ' • Ad Block • SponsorBlock' },
      settingsItemList(rows), 'ytps5-settings', update
    );
  }

  function handleSettingsAction(action, parameters) {
    if (action === 'YTPS5_SETTINGS_SHOW') {
      showSettingsPanel(false);
      return true;
    }
    if (action === 'YTPS5_DONATE') {
      showDonatePanel();
      return true;
    }
    if (action === 'YTPS5_TOGGLE_ADBLOCK') {
      settingsConfig.adBlock = !settingsConfig.adBlock;
      showToast('ytps5', 'Ad Block ' + (settingsConfig.adBlock ? 'On' : 'Off'));
      showSettingsPanel(true);
      return true;
    }
    if (action === 'YTPS5_TOGGLE_SB') {
      settingsConfig.sponsorBlock = !settingsConfig.sponsorBlock;
      if (!settingsConfig.sponsorBlock) {
        segments = [];
        if (skipTimer) {
          clearTimeout(skipTimer);
          skipTimer = null;
        }
      } else if (currentVideoId) {
        loadSponsorBlock(currentVideoId);
      }
      showToast('ytps5',
                'SponsorBlock ' + (settingsConfig.sponsorBlock ? 'On' : 'Off'));
      showSettingsPanel(true);
      return true;
    }
    if (action === 'YTPS5_TOGGLE_CATEGORY') {
      var index = settingsConfig.categories.indexOf(parameters);
      if (index >= 0) settingsConfig.categories.splice(index, 1);
      else settingsConfig.categories.push(parameters);
      if (settingsConfig.sponsorBlock && currentVideoId) {
        loadSponsorBlock(currentVideoId);
      }
      showSettingsPanel(true);
      return true;
    }
    return false;
  }

  function handleSettingsCommands(cmd) {
    if (!cmd || typeof cmd !== 'object') return false;
    if (cmd.customAction &&
        String(cmd.customAction.action).indexOf('YTPS5_') === 0) {
      return handleSettingsAction(cmd.customAction.action,
                                  cmd.customAction.parameters);
    }
    if (cmd.signalAction && cmd.signalAction.customAction &&
        String(cmd.signalAction.customAction.action).indexOf('YTPS5_') === 0) {
      return handleSettingsAction(cmd.signalAction.customAction.action,
                                  cmd.signalAction.customAction.parameters);
    }
    if (cmd.commandExecutorCommand && cmd.commandExecutorCommand.commands) {
      var commands = cmd.commandExecutorCommand.commands;
      for (var i = 0; i < commands.length; i++) {
        if (commands[i] && commands[i].customAction &&
            String(commands[i].customAction.action).indexOf('YTPS5_') === 0) {
          return handleSettingsAction(commands[i].customAction.action,
                                      commands[i].customAction.parameters);
        }
      }
    }
    return false;
  }

  function installSettingsCommandHook() {
    try {
      for (var key in window._yttv) {
        var item = window._yttv[key];
        var instance = item && item.instance;
        if (instance && typeof instance.resolveCommand === 'function' &&
            !instance.__ytps5_settings_hooked) {
          var original = instance.resolveCommand;
          instance.resolveCommand = function (cmd) {
            if (handleSettingsCommands(cmd)) return true;
            return original.apply(this, arguments);
          };
          instance.__ytps5_settings_hooked = true;
          log('settings command hook installed');
        }
      }
    } catch (e) {}
  }

  function versionNewer(remote, local) {
    var a = String(remote).split('.');
    var b = String(local).split('.');
    for (var i = 0; i < 3; i++) {
      var x = parseInt(a[i] || '0', 10);
      var y = parseInt(b[i] || '0', 10);
      if (x > y) return true;
      if (x < y) return false;
    }
    return false;
  }

  function checkForUpdate() {
    try {
      var xhr = new XMLHttpRequest();
      xhr.timeout = 8000;
      xhr.onload = function () {
        if (xhr.status !== 200) return;
        try {
          var data = origParse(xhr.responseText);
          if (data && data.version &&
              versionNewer(data.version, CURRENT_VERSION)) {
            showToast('ytps5 update available',
                      'v' + data.version + '  •  ' + (data.url || REPO_URL));
          }
        } catch (e) {}
      };
      xhr.open('GET', UPDATE_CHECK_URL, true);
      xhr.send();
    } catch (e) {}
  }

  function patchSettings(root) {
    if (!root || typeof root !== 'object') return false;
    if (Array.isArray(root.items)) {
      var changed = false;
      for (var i = root.items.length - 1; i >= 0; i--) {
        var item = root.items[i];
        var action = item && item.settingActionRenderer;
        if (!action) continue;
        var label = textOf(action.title);
        if (/purchases|memberships/i.test(label)) {
          root.items.splice(i, 1);
          changed = true;
          continue;
        }
        if (/premium/i.test(label)) {
          root.items[i] = settingRow(
            'ytps5 Settings', 'ytps5_settings',
            { customAction: { action: 'YTPS5_SETTINGS_SHOW', parameters: [] } },
            'Ad Block, SponsorBlock and more');
          installSettingsCommandHook();
          changed = true;
          log('settings: Premium row replaced');
        }
      }
      if (changed) return true;
    }
    for (var key in root) {
      if (!Object.prototype.hasOwnProperty.call(root, key)) continue;
      var value = root[key];
      if (value && typeof value === 'object' && patchSettings(value)) {
        return true;
      }
    }
    return false;
  }

  /* ---------------------------------------------------------------- */
  /* 1. Ad removal: prune ad objects before the UI renders them.      */
  /* ---------------------------------------------------------------- */
  /* Player-level ad fields plus feed/shelf ad renderers. */
  var AD_KEY_RE =
    /^(adSlotRenderer|promotedSparklesWebRenderer|compactPromotedItemRenderer|promotedVideoRenderer|tvPromotedVideoRenderer|inFeedAdRenderer|displayAdRenderer|bannerPromoRenderer|statementBannerRenderer|brandVideoShelfRenderer|brandVideoSingletonRenderer|adSlot|playerAd|adPlacement|adBreak|adBreakHeartbeatParams|adSlots|adPlacements|playerAds)$/i;
  var AD_KEY_SUBSTR =
    /(promoted|adslot|infeedad|displayad|bannerpromo|brandvideo|adrenderer|adbadge)/i;
  var SPONSOR_LABEL_RE =
    /^(sponsored|ad|promoted|paid promotion|sponsored video)$/i;

  function isAdKey(key) {
    return AD_KEY_RE.test(key) || AD_KEY_SUBSTR.test(key);
  }

  function textOf(value) {
    if (!value) return '';
    if (typeof value === 'string') return value;
    if (typeof value === 'object') {
      if (value.simpleText) return value.simpleText;
      if (value.content) return value.content;
      if (value.label) return value.label;
      if (value.text) return value.text;
      if (value.runs && value.runs.length && value.runs[0].text) {
        return value.runs[0].text;
      }
    }
    return '';
  }

  function containsSponsoredText(node, depth) {
    if (!node || depth > 2) return false;
    if (typeof node === 'string') {
      return /sponsored|paid promotion/i.test(node);
    }
    if (typeof node !== 'object') return false;
    for (var key in node) {
      if (!Object.prototype.hasOwnProperty.call(node, key)) continue;
      if (key === 'title' || key === 'headline') continue;
      if (containsSponsoredText(node[key], depth + 1)) return true;
    }
    return false;
  }

  function hasSponsorMarker(obj) {
    if (!obj || typeof obj !== 'object') return false;
    if (obj.promotedText &&
        SPONSOR_LABEL_RE.test(textOf(obj.promotedText).trim())) {
      return true;
    }
    if (obj.isAd || obj.isPromoted || obj.adBadge) return true;

    var meta = obj.metadata || obj.metadataRows ||
               obj.descriptionSnippet || obj.detailedMetadataSnippets;
    if (meta && containsSponsoredText(meta, 0)) return true;

    var badges = obj.badges || obj.badge || obj.ownerBadges;
    if (badges) {
      var list = Array.isArray(badges) ? badges : [badges];
      for (var i = 0; i < list.length; i++) {
        var badge = list[i];
        if (!badge) continue;
        var inner = badge.metadataBadgeRenderer || badge;
        var label = textOf(inner.label) || textOf(inner);
        var style = inner.style || '';
        if (SPONSOR_LABEL_RE.test(String(label).trim())) return true;
        if (/BADGE_STYLE_TYPE_AD/i.test(String(style))) return true;
      }
    }
    return false;
  }

  function pruneAds(node, depth, budget) {
    if (!node || typeof node !== 'object' || depth > 14 ||
        budget.n++ > 150000) {
      return;
    }

    if (Array.isArray(node)) {
      for (var i = node.length - 1; i >= 0; i--) {
        var item = node[i];
        if (!item || typeof item !== 'object') continue;

        var remove = false;
        if (!Array.isArray(item)) {
          for (var k in item) {
            if (Object.prototype.hasOwnProperty.call(item, k) &&
                isAdKey(k)) {
              remove = true;
              break;
            }
          }
          if (!remove && hasSponsorMarker(item)) remove = true;
        }

        if (remove) {
          node.splice(i, 1);
          continue;
        }
        pruneAds(item, depth + 1, budget);
      }
      return;
    }

    for (var key in node) {
      if (!Object.prototype.hasOwnProperty.call(node, key)) continue;
      if (isAdKey(key)) {
        delete node[key];
        continue;
      }
      var value = node[key];
      if (value && typeof value === 'object') {
        pruneAds(value, depth + 1, budget);
      }
    }
  }

  function cleanResponse(result) {
    if (!result || typeof result !== 'object') return result;
    if (settingsConfig.adBlock) {
      if (result.adPlacements) delete result.adPlacements;
      if (result.playerAds) delete result.playerAds;
      if (result.adSlots) delete result.adSlots;
      if (result.adBreakHeartbeatParams) {
        delete result.adBreakHeartbeatParams;
      }
      pruneAds(result, 0, { n: 0 });
    }
    return result;
  }

  /* Temporary diagnostic: capture the raw feed response containing the
   * sponsored card. Set DEBUG_DUMP to false for release builds. */
  var DEBUG_DUMP = false;

  function dumpRaw(text) {
    if (!DEBUG_DUMP || typeof text !== 'string' ||
        text.length > 2 * 1024 * 1024) {
      return;
    }
    try {
      var xhr = new XMLHttpRequest();
      xhr.open('POST', 'http://127.0.0.1:4040/dump', true);
      xhr.setRequestHeader('Content-Type', 'text/plain');
      xhr.send(text);
    } catch (e) {}
  }

  if (DEBUG_DUMP) {
    try {
      var ping = new XMLHttpRequest();
      ping.open('GET', 'http://127.0.0.1:4040/ping', true);
      ping.send();
    } catch (e) {}
  }

  JSON.parse = function () {
    var text = arguments[0];
    if (typeof text === 'string' &&
        /GoDaddy|Sponsored|promoted|adSlot|adPlacements/i.test(text)) {
      dumpRaw(text);
    }
    var result = cleanResponse(origParse.apply(this, arguments));
    if (typeof text === 'string' &&
        /premium|settingActionRenderer|settingsScreenRenderer/i.test(text)) {
      try { patchSettings(result); } catch (e) {}
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
    segments = (list || []).filter(function (item) {
      return item && settingsConfig.categories.indexOf(item.category) >= 0;
    });
    skipped = {};
    if (segments.length) {
      log('SponsorBlock:', segments.length, 'segment(s) for', videoId);
      showToast('SponsorBlock',
                segments.length + ' segment(s) found' +
                (currentVideo ? ' • video ready' : ' • video not found'));
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
    if (!videoId || !settingsConfig.sponsorBlock) return;
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
    if (!settingsConfig.sponsorBlock) return;
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

  function findVideoDeep(root, depth) {
    if (!root || depth > 10) return null;
    try {
      var direct = root.querySelector ? root.querySelector('video') : null;
      if (direct) return direct;
      var nodes = root.querySelectorAll ? root.querySelectorAll('*') : [];
      for (var i = 0; i < nodes.length; i++) {
        var el = nodes[i];
        if (el.shadowRoot) {
          var found = findVideoDeep(el.shadowRoot, depth + 1);
          if (found) return found;
        }
        if (el.tagName === 'IFRAME') {
          try {
            var doc = el.contentDocument;
            if (doc) {
              var inner = findVideoDeep(doc, depth + 1);
              if (inner) return inner;
            }
          } catch (e) {}
        }
      }
    } catch (e) {}
    return null;
  }

  function dumpDomInfo() {
    try {
      var info = {
        url: window.location.href || '',
        videos: document.querySelectorAll('video').length,
        shadows: [],
        players: []
      };
      var all = document.querySelectorAll('*');
      for (var i = 0; i < all.length && i < 20000; i++) {
        var el = all[i];
        if (el.shadowRoot) info.shadows.push(el.tagName);
        var tag = String(el.tagName || '').toLowerCase();
        if (tag.indexOf('player') >= 0 || tag.indexOf('video') >= 0) {
          info.players.push(tag);
        }
      }
      dumpRaw(JSON.stringify(info));
    } catch (e) {}
  }

  function attachVideo() {
    var video = findVideoDeep(document, 0);
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
    dumpDomInfo();
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

  setTimeout(checkForUpdate, 8000);

  log('payload loaded');
})();
