/*
 * Cookie-free, aggregate-only counter for the ytps5 site.
 *
 * What it records:
 *   pv-YYYY-MM-DD   one increment per page load
 *   dl-YYYY-MM-DD   one increment per click on a release/download link
 *   ref-<host>      the referring hostname only (no path, no query string)
 *
 * What it does not record: cookies, localStorage IDs, IP addresses, paths,
 * query strings, screen size, or anything that could identify a visitor.
 * The counter service only stores the integer totals above.
 */
(function () {
  "use strict";

  var NS = "ytps5-sponsorblock";
  var API = "https://abacus.jasoncameron.dev";

  // Do not pollute the totals from local previews.
  if (/^(localhost|127\.0\.0\.1|\[::1\]|0\.0\.0\.0)$/.test(location.hostname)) return;

  function hit(key) {
    try {
      fetch(API + "/hit/" + NS + "/" + encodeURIComponent(key), {
        mode: "cors",
        credentials: "omit",
        keepalive: true,
        cache: "no-store"
      }).catch(function () {});
    } catch (e) {
      /* counting must never break the page */
    }
  }

  function day() {
    var d = new Date();
    var m = String(d.getMonth() + 1);
    var dd = String(d.getDate());
    return d.getFullYear() + "-" + (m.length < 2 ? "0" + m : m) + "-" + (dd.length < 2 ? "0" + dd : dd);
  }

  var today = day();

  // Page view, once per document (not on bfcache restores).
  if (!window.__ytps5Counted) {
    window.__ytps5Counted = true;
    hit("pv-" + today);
  }

  // Referring hostname only. Full URLs can carry search terms or tokens,
  // so they are reduced to the registrable host before anything is sent.
  var ref = "direct";
  try {
    if (document.referrer) {
      var host = new URL(document.referrer).hostname.toLowerCase().replace(/^www\./, "");
      ref = host === location.hostname.toLowerCase().replace(/^www\./, "") ? "internal" : host;
    }
  } catch (e) {
    ref = "other";
  }
  if (!/^[a-z0-9.-]{1,60}$/.test(ref)) ref = "other";
  hit("ref-" + ref);

  // Download intent: clicks on a release asset or any .elf link.
  document.addEventListener("click", function (ev) {
    var a = ev.target && ev.target.closest ? ev.target.closest("a[href]") : null;
    if (!a) return;
    var href = a.getAttribute("href") || "";
    if (/\/releases\/(latest\/)?download\//i.test(href) || /\.elf(\?|#|$)/i.test(href)) {
      hit("dl-" + today);
    }
  }, true);
})();
