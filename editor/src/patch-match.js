// patch-match.js — editor drop UI for the shipped `pm` CLI (option A).
//
// Drop a WAV on the window → Electron spawns build/pm (same spirit as
// studio:run) → this panel shows stage progress, then the candidates you
// can audition and paste into the open cart buffer. EIGHT of them on the
// default path and SIX on a shift-drop --quick (pm caps at want = 8 over
// 3 engines x 3 seeds; --quick refines 3 x 2), which is why nothing here
// hardcodes a count: renderResults reports what actually arrived. No engine change;
// ADR-0006 / docs/design/patch-matching-cart.md.
//
import { view, flashRange } from './main.js'

let toastFn = (msg) => console.warn(msg)
let running = false
let audioEl = null
let playingN = -1
let wired = false

function $(id) { return document.getElementById(id) }

function showModal() { const m = $('pm-modal'); if (m) m.hidden = false }
function hideModal() { const m = $('pm-modal'); if (m) m.hidden = true }

function setPanel(which) {
  for (const id of ['pm-progress', 'pm-error', 'pm-results']) {
    const el = $(id)
    if (el) el.hidden = id !== which
  }
}

function setProgress(pct, label) {
  const fill = $('pm-bar-fill')
  const lab = $('pm-progress-label')
  if (fill) fill.style.width = `${Math.round(Math.max(0, Math.min(1, pct)) * 100)}%`
  if (lab) lab.textContent = label || ''
}

function appendLog(s) {
  const log = $('pm-log')
  if (!log) return
  log.textContent += s
  log.scrollTop = log.scrollHeight
}

function stopAudition() {
  if (audioEl) { audioEl.pause(); audioEl.removeAttribute('src'); audioEl.load() }
  playingN = -1
  document.querySelectorAll('#pm-cands .pm-cand').forEach(el => el.classList.remove('playing'))
  document.querySelectorAll('#pm-cands button').forEach(b => {
    if (b.textContent === '■ stop')
      b.textContent = b.closest('.pm-target') ? '▶ hear target' : '▶ audition'
  })
}

function insertSnippet(text) {
  const insert = text.endsWith('\n') ? text : text + '\n'
  const { from, to } = view.state.selection.main
  view.dispatch({
    changes: { from, to, insert },
    selection: { anchor: from + insert.length },
  })
  flashRange(view, from, from + insert.length)
  document.querySelector('#tabs .tab[data-tab="code"]')?.click()
}

function renderResults(res, name) {
  setPanel('pm-results')
  const host = $('pm-cands')
  if (!host) return
  host.innerHTML = ''

  if (res.targetWav) {
    const tgt = document.createElement('div')
    tgt.className = 'pm-cand pm-target'
    tgt.innerHTML = `<div class="pm-cand-h"><span>target</span><span class="pm-dim">what the search scored</span></div>`
    const play = document.createElement('button')
    play.textContent = '▶ hear target'
    play.addEventListener('click', () => playUrl(res.targetWav, play, -1))
    tgt.appendChild(play)
    host.appendChild(tgt)
  }

  for (const c of res.candidates) {
    const row = document.createElement('div')
    row.className = 'pm-cand'
    row.dataset.n = String(c.n)
    const loss = Number.isFinite(c.fx) ? c.fx.toFixed(5) : '?'
    row.innerHTML = `<div class="pm-cand-h">
      <span>${c.n}. ${c.engine}</span>
      <span class="pm-dim">fx ${loss}</span>
    </div>`
    const actions = document.createElement('div')
    actions.className = 'pm-cand-actions'
    const play = document.createElement('button')
    play.textContent = c.wavDataUrl ? '▶ audition' : 'no wav'
    play.disabled = !c.wavDataUrl
    play.addEventListener('click', () => playUrl(c.wavDataUrl, play, c.n))
    const paste = document.createElement('button')
    paste.textContent = 'paste into cart'
    paste.title = 'insert this instrument() block at the cursor'
    paste.addEventListener('click', () => {
      insertSnippet(c.snippet)
      toastFn(`pasted candidate ${c.n} (${c.engine})`, 2500)
    })
    actions.appendChild(play)
    actions.appendChild(paste)
    row.appendChild(actions)
    host.appendChild(row)
  }

  const note = document.createElement('div')
  note.className = 'kw-hint'
  note.textContent = `${res.candidates.length} candidates from ${name} · paste sits at the cursor in the open buffer`
  host.appendChild(note)
}

function playUrl(url, btn, n) {
  if (!audioEl || !url) return
  if (playingN === n && !audioEl.paused) { stopAudition(); if (btn) btn.textContent = n < 0 ? '▶ hear target' : '▶ audition'; return }
  stopAudition()
  playingN = n
  audioEl.src = url
  audioEl.play().catch(() => toastFn('could not play that WAV', 2500))
  document.querySelectorAll('#pm-cands .pm-cand').forEach(el => {
    el.classList.toggle('playing', n >= 0 && el.dataset.n === String(n))
  })
  if (btn) btn.textContent = '■ stop'
  audioEl.onended = () => {
    if (btn) btn.textContent = n < 0 ? '▶ hear target' : '▶ audition'
    stopAudition()
  }
}

function showError(msg) {
  setPanel('pm-error')
  const el = $('pm-error')
  if (el) el.textContent = msg
}

function wireOnce() {
  if (wired) return
  wired = true
  audioEl = $('pm-audio')
  $('pm-close')?.addEventListener('click', () => {
    hideModal()
    stopAudition()
  })
  $('pm-cancel')?.addEventListener('click', async () => {
    if (!window.studio?.patchMatchCancel) return
    $('pm-cancel').disabled = true
    await window.studio.patchMatchCancel()
    appendLog('\ncancelled.\n')
  })
  if (window.studio?.onPatchMatchLog) {
    window.studio.onPatchMatchLog(s => appendLog(s))
  }
  if (window.studio?.onPatchMatchProgress) {
    window.studio.onPatchMatchProgress(info => {
      if (info && typeof info.pct === 'number') setProgress(info.pct, info.label)
    })
  }
}

export function initPatchMatch({ showToast } = {}) {
  if (showToast) toastFn = showToast
  wireOnce()
}

export async function handleWavDrop(file, { quick = false } = {}) {
  wireOnce()
  const name = file?.name || 'sample.wav'

  // ⚠ REFUSE BEFORE TOUCHING THE PANEL. Every line below this guard is destructive to a search that
  // is ALREADY RUNNING: it blanks the log, resets the bar to zero and swaps the panel. So when the
  // guard sat under them, a stray second drop threw away the running search's whole trail and then
  // politely said it would not start a second one. The refusal has to come first.
  if (running) {
    showModal()
    showError('a match is already running. cancel it, or wait.')
    return
  }

  showModal()
  $('pm-subject').textContent = name + (quick ? '  ·  quick' : '')
  $('pm-log').textContent = ''
  setProgress(0, 'starting…')
  setPanel('pm-progress')
  const cancel = $('pm-cancel')
  if (cancel) cancel.disabled = false
  stopAudition()

  if (!window.studio?.patchMatch) {
    showError('matching a sample needs the desktop app (npm start) — the editor spawns the patch-match CLI, it is not an in-cart render.')
    return
  }

  let wavPath = ''
  try { wavPath = window.studio.getFilePath?.(file) || '' } catch {}
  if (!wavPath) {
    showError('could not see a path for that drop. drop the WAV from Finder / Explorer (the desktop app needs a real file).')
    return
  }

  running = true
  try {
    const res = await window.studio.patchMatch({ wavPath, quick })
    if (res?.ok && res.candidates?.length) {
      renderResults(res, name)
      if ($('pm-modal')?.hidden) toastFn(`${res.candidates.length} patches ready — click to review`, 8000, () => { showModal(); renderResults(res, name) })
    } else if (res?.cancelled) {
      showError('cancelled.')
    } else {
      showError(res?.error || 'patch-match failed')
    }
  } catch (e) {
    showError(e?.message || String(e))
  } finally {
    running = false
    if (cancel) cancel.disabled = false
  }
}

// used by the self-contained UI check (Vite, no Electron) so we can exercise
// audition + paste without spawning pm.
export function showPatchMatchFixture(res, name = 'fixture.wav') {
  wireOnce()
  showModal()
  $('pm-subject').textContent = name
  renderResults(res, name)
}
