/* de:meta
{
  "title": "export demo",
  "slug": "exportdemo",
  "kind": ["tech-demo"],
  "teaches": ["save-load-persistence"],
  "created": "2026-08-19",
  "description": {
    "summary": "The reference wiring for export_audio(): record what you are hearing to a real stereo file, with the free/Pro split that ADR-0035 puts on it.",
    "detail": "A four-bar loop you can hear, and two buttons that carry it OUT of the app. SHARE writes a compressed take anyone can post; WAV writes the lossless one, and that button is the paid unlock. The wall is deliberately on the FORMAT, not on the feature: a free player can still share what they made, which keeps the social loop open, and only a workflow that needs lossless audio has to pay. Watch the REC bar fill while the audio thread captures, then the path appear. The export is STEREO, so the autopan on the pad survives into the file, which a mono capture would have thrown away.",
    "controls": "SPACE toggles the loop. Tap SHARE for a compressed take, WAV for the lossless one (Pro). ESC closes the Pro sheet."
  }
}
de:meta */

// The worked example for runtime/pro.h + export_audio(), the two halves of ADR-0035's
// "Pro is the paths that carry audio OUT of the app". Copy this wiring, not a hand-rolled one.
//
// THE SHAPE WORTH COPYING, and it is a product decision as much as a code one:
//   · the free player can ALWAYS export something (EXPORT_M4A). The wall is on the FORMAT.
//   · only the lossless take asks for Pro, and it opens the SHARED sheet from pro.h rather than
//     a paywall this cart drew itself.
//   · export_busy()/export_progress() are polled, because the capture happens on the audio thread
//     over real seconds — there is no synchronous "save file" call to await.

#include "studio.h"
#include "ui.h"
#include "pro.h"

#define TAKE_SECONDS 4.0f

static ProSheet sheet;          // the cart owns the Pro sheet's state (pro.h is stateless)
static bool     playing = true;
static int      beat_i = 0;   // NOT `step`: spec.h declares step(int), and the clash only breaks the -DDE_SPEC build
static float    next_step_at = 0;
static char     status[160] = "";

static void start_export(int format) {
    // one name per format so a WAV take never silently overwrites the shared one
    const char *name = (format == EXPORT_WAV) ? "take.wav" : "take-share.wav";
    if (export_audio(name, TAKE_SECONDS, format))
        snprintf(status, sizeof status, "recording %.0fs...", TAKE_SECONDS);
    else
        snprintf(status, sizeof status, "busy - one export at a time");
}

void init(void) {
    instrument(5, INSTR_TRI,  12, 140, 2, 200);    // pluck. TRI, not SAW: a saw steps a full cycle
                                                   // every period by construction, so click-check
                                                   // reads a clean saw loop as 150x splices
    instrument(6, INSTR_SINE, 18, 70, 0,  90);     // bass. 18ms attack, NOT 4: at 4ms a sine
                                                   // starts as a step and click-check reads 109% of peak
    // a stereo mover, so the export has something a MONO capture would destroy
    autopan(0.8f, 0.9f, LFO_SHAPE_SINE);
    bpm(110);
}

void update(void) {
    if (keyp(KEY_SPACE)) playing = !playing;
    if (playing && now() >= next_step_at) {
        static const int notes[8] = { 45, 52, 48, 55, 45, 57, 48, 52 };
        hit(notes[beat_i & 7], 5, 4, 260);
        if ((beat_i & 1) == 0) hit(33, 6, 5, 90);
        beat_i++;
        next_step_at = now() + 0.27f;
    }
}

void draw(void) {
    cls(CLR_DARK_BLUE);
    ui_begin();                     // presses are RECORDED here and resolved in ui_end(); without
    print("EXPORT DEMO", 8, 6, CLR_YELLOW);
    print(playing ? "loop: playing  (space)" : "loop: stopped  (space)", 8, 18, CLR_LIGHT_GREY);

    // ── the two doors out of the app ────────────────────────────────────────────────────────
    // SHARE is always available. That is the point: a free player who cannot post what they made
    // is a marketing channel switched off, and it also means the paywall never takes away a
    // feature somebody already had.
    if (ui_button(8, 34, 76, 18, "share")) start_export(EXPORT_M4A);

    // WAV is the paid one. Note it does NOT hide when locked — a wall you cannot see is a feature
    // nobody knows exists. It opens the shared sheet instead.
    if (ui_button(92, 34, 76, 18, pro_unlocked() ? "wav" : "wav (pro)")) {
        if (pro_unlocked()) start_export(EXPORT_WAV);
        else                pro_sheet_open(&sheet);
    }

    // ── the capture is asynchronous, so it has to be watched rather than awaited ─────────────
    if (export_busy()) {
        int w = (int)(152 * export_progress());
        rect(8, 60, 152, 8, CLR_LIGHT_GREY);
        rectfill(8, 60, w, 8, CLR_RED);
        print("REC", 166, 60, CLR_RED);
    } else if (export_last()[0]) {
        snprintf(status, sizeof status, "saved: %s", export_last());
    }
    print(status, 8, 76, CLR_WHITE);

    if (!pro_for_sale())
        print("(no store here - everything unlocked)", 8, 90, CLR_MEDIUM_GREY);

    pro_sheet(&sheet, "Lossless WAV, MIDI and the plug-in");   // LAST, like cursor_draw()
    ui_end();                       // …the pair, no widget ever clicks (the engine warns, loudly)
}

#ifdef DE_SPEC
#include "spec.h"
void spec(void) {
    // The export is asynchronous and driven by the audio thread, so a spec cannot wait for the
    // file. What it CAN pin is the contract around it, which is where the mistakes live.
    expect(export_busy() == 0, "idle before anything is asked for");
    expect(export_progress() == 0.0f, "no progress while idle");
    expect(export_audio("spec-take.wav", 1.0f, EXPORT_WAV) == 1, "a first export starts");
    expect(export_busy() == 1, "…and reports busy");
    // ONE AT A TIME, and this is the assertion that matters: the capture buffer is process-global
    // (tools/ctx-classification.json), so a second start must be REFUSED rather than interleaving
    // two takes into one file.
    expect(export_audio("other.wav", 1.0f, EXPORT_WAV) == 0, "a second export is refused while busy");
    expect(export_audio(NULL, -5.0f, EXPORT_WAV) == 0, "…including a nonsense one");
}
#endif
