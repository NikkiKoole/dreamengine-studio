# False positives the RECALL work introduced, and the discriminators that killed them

Widening the patterns (2026-07-31) cost precision twice on the live corpus before these
two rules landed. Both are asserted here so the next widening cannot bring them back.

Discriminator 7, a quoted claim is being DISCUSSED, not asserted. This sentence closes a
wait, which is the opposite of a denial, but it must name the wait to close it. Verbatim
from audio-notes.md, and kept on ONE line as it is there. KNOWN LIMIT: matching is
per-line, so a quote WRAPPED across two lines is not seen as closed and will still fire.

**Two things this settles.** (1) It closed the "waiting on the sidechain path" wait that items 13 + 14 both reference: the plumbing was already right.

Discriminator 3 covering `wah`, where a bare "no wah" is a TEST CONDITION, not a claim we
lack the pedal. From the real debug-harness worked example:

An FFT of both (middle C, no wah) showed our noise floor 28 dB higher (-90 vs -118 dB)
plus spurious inharmonic partials.
