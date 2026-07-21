// ratios.js — the ONE ratio/size vocabulary for the editor's video exports.
//
// Everything that offers an output size reads from here — the Promote "bake at" picker, the trailer
// builder's output picker, the dress-modal ratio picker, and the friendly chip labels. Before this
// module those lists were hand-copied in three places (two <select>s + a RATIO_LABEL map) and drifted.
// Add or change a preset ONCE, here.
//
// Values are "WxH" strings (what dress-clip.js --w/--h, compose-clips `# size`, and make-gif --screen
// all consume); '' = native (the cart/clip's own dims). Social presets are standard 1080-class canvases
// so 9:16 is a single 1080x1920 everywhere (matching dress-clip.js + youtube-push). The App Store
// entries are app-PREVIEW-video aspect sizes (deliberately smaller than store-shots.js's DEVICES
// screenshot pixels — the preview-video spec differs; verify vs App Store Connect). See
// docs/design/export-ratios.md.

export const RATIO_PRESETS = [
  { value: '',          label: 'native',                  chip: 'native' },
  { value: '1920x1080', label: '16:9',                    chip: '16:9',     group: 'social' },
  { value: '1080x1920', label: '9:16 (vertical)',         chip: '9:16',     group: 'social' },
  { value: '1080x1080', label: '1:1 (square)',            chip: '1:1',      group: 'social' },
  { value: '1080x1350', label: '4:5 (portrait)',          chip: '4:5',      group: 'social' },
  { value: '886x1920',  label: 'iPhone 6.9″ (portrait)',  chip: 'iPhone',   group: 'appstore' },
  { value: '1920x886',  label: 'iPhone 6.9″ (landscape)', chip: 'iPhone L', group: 'appstore' },
  { value: '1200x1600', label: 'iPad 13″ (portrait)',     chip: 'iPad',     group: 'appstore' },
  { value: '1600x1200', label: 'iPad 13″ (landscape)',    chip: 'iPad L',   group: 'appstore' },
]

const GROUP_LABEL = { social: 'social', appstore: 'App Store (verify vs ASC)' }

// the friendly short label for a "WxH" value (used for variant chips + toasts); falls back to the raw value
export const ratioLabel = v => (RATIO_PRESETS.find(p => p.value === v)?.chip) || v || 'native'

// populate a <select> with the presets, grouped into <optgroup>s. Preserves the current value.
export function buildRatioSelect(sel) {
  if (!sel) return
  const cur = sel.value
  const bits = []
  let g
  for (const p of RATIO_PRESETS) {
    const grp = p.group || null
    if (grp !== g) { if (g) bits.push('</optgroup>'); g = grp; if (g) bits.push(`<optgroup label="${GROUP_LABEL[g] || g}">`) }
    bits.push(`<option value="${p.value}">${p.label}</option>`)
  }
  if (g) bits.push('</optgroup>')
  sel.innerHTML = bits.join('')
  setRatioValue(sel, cur)
}

// set a select's value, tolerating a saved size that isn't a preset (e.g. an old reel's 1280x720) —
// adds a one-off option so it still displays + round-trips instead of silently snapping to the first.
export function setRatioValue(sel, value) {
  if (!sel) return
  const v = value || ''
  if (v && !RATIO_PRESETS.some(p => p.value === v) && !sel.querySelector(`option[value="${v}"]`)) {
    const o = document.createElement('option'); o.value = v; o.textContent = `${v} (saved)`; sel.appendChild(o)
  }
  sel.value = v
}
