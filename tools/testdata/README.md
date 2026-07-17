# tools/testdata — fixed input assets for headless cart testing

Real-world inputs a cart can be driven by in the harness, so a run is reproducible without a live
device (a mic, a camera, …). Deterministic: same asset + same script + same seed → same output.

## `vocal-8s.wav` — a real solo singing voice (the mic-input test source)

8 s, mono, 44100 Hz, 16-bit, −6 dBFS. A clean-ish monophonic sung melody — the honest test input for
the auto-tune / vocoder work (real breath, vibrato, and note-to-note motion that a synthetic 'ah'
never exposes).

**Provenance:** trimmed (5–13 s) + gain-reduced from *"Jana gana mana vocal.ogg"*
(https://commons.wikimedia.org/wiki/File:Jana_gana_mana_vocal.ogg), **public domain**. Reduced to mono
44.1 k 16-bit WAV via ffmpeg.

## Feeding it as the microphone — `DE_MIC_WAV`

Any harness run reads `DE_MIC_WAV=<path>` and feeds that WAV through the real mic seam
(`de_audio_input`) one frame's worth of samples per frame, *before* the synchronous `--wav` audio
render — so the live mic path (`autotune_mic`, `vocoder_mic`, `mic_pitch`, …) is exercised with a real
recording, deterministically, no device. The injected WAV also forces `mic_active()` true and skips
the real mic. (16-bit PCM WAV only; the runtime hook lives in `studio.c`.)

```bash
# render livetune, fed by the vocal, live auto-tune on at frame 5 → out.wav (the corrected voice)
printf 'tap 5 SPACE\n' > /tmp/space.script
DE_MIC_WAV=tools/testdata/vocal-8s.wav \
  node tools/play.js livetune script /tmp/space.script --headless --frames 480 --wav /tmp/out.wav
# then analyze: node tools/formant-check.js /tmp/out.wav 2 3   (pitch snapped? formants held?)
```
