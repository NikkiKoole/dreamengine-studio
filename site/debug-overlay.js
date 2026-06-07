// debug-overlay.js — cable-free on-device debug panel for wasm carts.
// (docs/design/mobile-web-notes.md §6d, STATUS open item 27)
//
// Lives at the SITE ROOT (copied there by build-site.js) and is loaded on
// demand by a ~20-line loader baked into web_shell.html — so improving this
// file never requires rebuilding carts: republish this one file.
//
// Open with ?debug=1 or triple-tap/triple-click the top-left corner; the same
// gesture (or ✕) closes it. Shows:
//   • live touch contacts — a ring per finger straight from Module.deTouches
//     (the engine's touch truth), with id + count readout. A ring that stays
//     after you lift = phantom; rings that appear while the game ignores you
//     = the bug is past the touch layer (mouse path / cart logic).
//   • console mirror — last lines of console.log/warn/error (printh lands
//     here on web) without needing a Mac + cable.
//   • JS errors — window.onerror / unhandledrejection as red lines instead
//     of a silently frozen canvas.
//   • fps measured from the browser's frame loop.
(function () {
  if (window.deDebug) { window.deDebug.toggle(); return; }

  var LOG_MAX = 14;
  var logs = [];
  var visible = false;

  // ── console hook (installed once, even while hidden — catches early logs) ──
  function hook(kind) {
    var orig = console[kind];
    console[kind] = function () {
      var line = Array.prototype.map.call(arguments, function (a) {
        try { return typeof a === 'string' ? a : JSON.stringify(a); }
        catch (e) { return String(a); }
      }).join(' ');
      push(kind, line);
      return orig.apply(console, arguments);
    };
  }
  function push(kind, line) {
    logs.push({ kind: kind, line: line, t: new Date().toTimeString().slice(0, 8) });
    if (logs.length > LOG_MAX) logs.shift();
    renderLog();
  }
  hook('log'); hook('warn'); hook('error');
  window.addEventListener('error', function (e) {
    push('error', (e.message || 'script error') + ' @' + (e.filename || '').split('/').pop() + ':' + e.lineno);
  });
  window.addEventListener('unhandledrejection', function (e) {
    push('error', 'unhandled rejection: ' + (e.reason && e.reason.message || e.reason));
  });

  // ── panel DOM ──
  var css = 'position:fixed;z-index:9999;font:10px/1.5 ui-monospace,Menlo,monospace;';
  var panel = document.createElement('div');
  panel.style.cssText = css + 'top:0;left:0;right:0;max-height:45%;overflow:auto;' +
    'background:rgba(20,21,24,.92);color:#e8e6e3;padding:6px 8px;display:none;' +
    'border-bottom:1px solid #ff6c24;pointer-events:auto;-webkit-user-select:text;user-select:text;';
  var head = document.createElement('div');
  head.style.cssText = 'display:flex;gap:12px;align-items:center;color:#ffa300;';
  var stat = document.createElement('span');
  var close = document.createElement('span');
  close.textContent = '✕';
  close.style.cssText = 'margin-left:auto;cursor:pointer;padding:0 8px;color:#9a948c;';
  close.addEventListener('click', function () { setVisible(false); });
  head.appendChild(stat); head.appendChild(close);
  var logEl = document.createElement('div');
  logEl.style.cssText = 'margin-top:4px;white-space:pre-wrap;word-break:break-all;';
  panel.appendChild(head); panel.appendChild(logEl);
  document.body.appendChild(panel);

  var dotsLayer = document.createElement('div');
  dotsLayer.style.cssText = css + 'inset:0;pointer-events:none;display:none;';
  document.body.appendChild(dotsLayer);
  var dots = [];   // pooled ring divs

  var KINDCOL = { log: '#e8e6e3', warn: '#ffa300', error: '#ff3355' };
  function renderLog() {
    if (!visible) return;
    logEl.innerHTML = logs.map(function (l) {
      return '<span style="color:#5f5a54">' + l.t + '</span> <span style="color:' +
        (KINDCOL[l.kind] || '#e8e6e3') + '">' + l.line.replace(/&/g, '&amp;').replace(/</g, '&lt;') + '</span>';
    }).join('\n') || '<span style="color:#5f5a54">(no console output yet — printh() lands here)</span>';
  }

  // ── per-frame: fps + touch rings ──
  var frames = 0, fps = 0, fpsT = performance.now();
  function tick() {
    frames++;
    var now = performance.now();
    if (now - fpsT >= 1000) { fps = frames; frames = 0; fpsT = now; }
    if (visible) {
      var t = window.Module && Module.deTouches || [];
      var n = (t.length / 3) | 0;
      var c = window.Module && Module.canvas;
      var ids = [];
      if (c) {
        var rect = c.getBoundingClientRect();
        for (var i = 0; i < n; i++) {
          ids.push(t[i * 3]);
          var d = dots[i];
          if (!d) {
            d = document.createElement('div');
            d.style.cssText = 'position:absolute;width:44px;height:44px;margin:-22px 0 0 -22px;' +
              'border:3px solid #00e436;border-radius:50%;color:#00e436;font:10px ui-monospace;' +
              'text-align:center;line-height:58px;';
            dotsLayer.appendChild(d); dots[i] = d;
          }
          d.style.display = 'block';
          d.style.left = (rect.left + (t[i * 3 + 1] / c.width)  * rect.width)  + 'px';
          d.style.top  = (rect.top  + (t[i * 3 + 2] / c.height) * rect.height) + 'px';
          d.textContent = t[i * 3];
        }
      }
      for (var j = n; j < dots.length; j++) if (dots[j]) dots[j].style.display = 'none';
      stat.textContent = 'fps ' + fps + ' · touches ' + n + (ids.length ? ' [' + ids.join(',') + ']' : '') +
        (Module.deTouchCeiling ? ' · ceiling ' + Module.deTouchCeiling : '');
    }
    requestAnimationFrame(tick);
  }
  requestAnimationFrame(tick);

  function setVisible(v) {
    visible = v;
    panel.style.display = v ? 'block' : 'none';
    dotsLayer.style.display = v ? 'block' : 'none';
    if (v) renderLog();
  }

  window.deDebug = { toggle: function () { setVisible(!visible); }, show: function () { setVisible(true); } };
  setVisible(true);
  push('log', 'debug overlay open — triple-tap top-left corner (or ✕) to close');
})();
