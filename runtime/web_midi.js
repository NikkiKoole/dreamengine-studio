// web_midi.js — Web MIDI → engine bridge, injected into the wasm build via
// emcc `--post-js`. Drives navigator.requestMIDIAccess() and feeds note/bend
// events into the SAME ring the cart drains through midi_get() — the web
// counterpart to the native CoreMIDI backend (see runtime/midi_input.h).
//
// No-op where Web MIDI is unavailable (Safari, iOS — they have no
// requestMIDIAccess). Needs a secure context (https or localhost) + the user
// to allow the MIDI permission prompt. Chrome / Edge / Firefox on desktop.
//
// Exports it calls (EMSCRIPTEN_KEEPALIVE in midi_input.h):
//   de_midi_web_push(type,note,vel) · de_midi_web_bend(v) · de_midi_web_present(n)
//   de_midi_web_name(str)
// Plus a small on-page toast naming each keyboard as it connects.
(function () {
  if (typeof navigator === 'undefined' || !navigator.requestMIDIAccess) {
    console.warn('[web-midi] unsupported (Safari/iOS or insecure context) — MIDI off');
    return;
  }

  function push(type, note, vel) { try { Module.ccall('de_midi_web_push', null, ['number', 'number', 'number'], [type, note, vel]); } catch (e) {} }
  function bend(v)    { try { Module.ccall('de_midi_web_bend',    null, ['number'], [v]); } catch (e) {} }
  function present(n) { try { Module.ccall('de_midi_web_present', null, ['number'], [n]); } catch (e) {} }
  function setname(s) { try { Module.ccall('de_midi_web_name',    null, ['string'], [s]); } catch (e) {} }

  // a brief on-page toast. Returns false if the page isn't ready to show it yet
  // (so the caller can retry rather than mark the device announced prematurely).
  function toast(text) {
    if (typeof document === 'undefined' || !document.body) return false;
    var el = document.getElementById('de-midi-toast');
    if (!el) {
      el = document.createElement('div');
      el.id = 'de-midi-toast';
      el.style.cssText = 'position:fixed;left:50%;bottom:22px;transform:translateX(-50%);' +
        'background:rgba(18,22,38,.92);color:#fff;font:13px/1.4 system-ui,sans-serif;' +
        'padding:8px 14px;border-radius:8px;z-index:99999;pointer-events:none;white-space:nowrap;' +
        'box-shadow:0 2px 14px rgba(0,0,0,.45);opacity:0;transition:opacity .6s';
      document.body.appendChild(el);
    }
    el.textContent = text;
    el.style.opacity = '1';
    clearTimeout(el._t);
    el._t = setTimeout(function () { el.style.opacity = '0'; }, 6000);
    return true;
  }

  function onMessage(ev) {
    var d = ev.data; if (!d || d.length < 1) return;
    var status = d[0] & 0xF0;
    if (status === 0x90 && d.length >= 3) { if (d[2] > 0) push(1, d[1], d[2]); else push(-1, d[1], 0); }
    else if (status === 0x80 && d.length >= 3) push(-1, d[1], 0);
    else if (status === 0xE0 && d.length >= 3) bend(((d[2] << 7) | d[1]) - 8192);
  }

  var announced = {};   // ids whose connect-toast actually rendered (so we don't repeat)
  function refresh(access) {
    var n = 0, first = '', live = {};
    access.inputs.forEach(function (inp) {
      inp.onmidimessage = onMessage;
      if (inp.open) { try { inp.open(); } catch (e) {} }   // open pre-connected ports so they deliver
      var nm = inp.name || 'MIDI device';
      n++; live[inp.id] = 1; if (!first) first = nm;
      // announce once — but only MARK it announced if the toast actually showed,
      // so an early call before the page is ready gets retried, not swallowed.
      if (!announced[inp.id] && toast('🎹 ' + nm + ' connected')) announced[inp.id] = 1;
    });
    for (var k in announced) if (!live[k]) delete announced[k];   // departed → re-announce on replug
    present(n);
    setname(n ? first : '');
    return n;
  }

  function wire(access) {
    access.onstatechange = function () { refresh(access); };   // hot-plug
    // A keyboard already plugged in at page load often isn't in access.inputs yet
    // when access resolves (and Chrome may not fire statechange for it), and the
    // page may not be ready to paint a toast the first instant — so POLL until a
    // device is present AND announced, then stop.
    var tries = 0;
    (function poll() {
      refresh(access);
      var done = access.inputs.size > 0 && Object.keys(announced).length > 0;
      if (!done && tries++ < 30) setTimeout(poll, 300);   // ~9s window
    })();
  }

  function start() {
    navigator.requestMIDIAccess({ sysex: false }).then(wire, function (e) {
      // denied or auto-blocked — DON'T fail silently; tell the user how to allow it
      console.warn('[web-midi] access denied/failed:', e && e.message);
      toast('🎹 MIDI is blocked — click the site icon left of the URL → MIDI → Allow, then reload');
    });
  }

  // Asking on page LOAD is what gets MIDI auto-blocked: an unexpected prompt
  // dismissed a few times makes Chrome block the permission. So if it's already
  // granted, connect now; if already denied, explain; otherwise wait for the first
  // user interaction (a music cart needs a gesture to start audio anyway) so the
  // prompt is intentional.
  function onGesture() {
    window.removeEventListener('pointerdown', onGesture, true);
    window.removeEventListener('keydown', onGesture, true);
    start();
  }
  function armGesture() {
    window.addEventListener('pointerdown', onGesture, true);
    window.addEventListener('keydown', onGesture, true);
  }

  if (navigator.permissions && navigator.permissions.query) {
    navigator.permissions.query({ name: 'midi' }).then(function (st) {
      if (st.state === 'granted')     start();        // already allowed → connect instantly
      else if (st.state === 'denied') toast('🎹 MIDI is blocked — click the site icon left of the URL → MIDI → Allow, then reload');
      else                            armGesture();   // 'prompt' → ask on first interaction
    }, armGesture);   // browser can't query 'midi' (e.g. Firefox) → gesture path
  } else {
    armGesture();
  }
})();
