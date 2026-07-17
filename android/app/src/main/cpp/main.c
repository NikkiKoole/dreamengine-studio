// main.c — the dreamengine Android host shell (spike 1).
//
// NativeActivity + native_app_glue own the app; this file:
//   - stands up EGL/GLES2 and blits de_framebuffer() as one flipped fullscreen quad
//     (aspect-fit letterbox),
//   - pulls de_audio_render() from an AAudio callback,
//   - maps touch events (through the letterbox) to de_touch_*.
// The engine itself is the real studio.c + sound.h + a cart, built -DDE_NO_RAYLIB.
// Twin of ios/Sources/{CanvasView,AudioEngine}.swift. See docs/design/android-plan.md.

#include <android_native_app_glue.h>
#include <android/log.h>
#include <android/input.h>
#include <android/window.h>          // AWINDOW_FLAG_* for ANativeActivity_setWindowFlags
#include <EGL/egl.h>
#include <GLES2/gl2.h>
#include <aaudio/AAudio.h>
#include <jni.h>
#include <time.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "engine.h"

#define TAG "dreamengine"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO,  TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, TAG, __VA_ARGS__)

// ---- GLES state ----
static EGLDisplay g_dpy = EGL_NO_DISPLAY;
static EGLSurface g_sfc = EGL_NO_SURFACE;
static EGLContext g_ctx = EGL_NO_CONTEXT;
static GLuint g_tex = 0, g_prog = 0;
static GLint  g_aPos = -1, g_aUV = -1, g_uTex = -1;
static int    g_view_w = 0, g_view_h = 0;     // full surface size (px)
static int    g_sw = 0, g_sh = 0;             // engine framebuffer size
static int    g_ready = 0;

// letterbox rect in surface px (recomputed each frame; touch maps through it)
static int    g_dx = 0, g_dy = 0, g_dw = 0, g_dh = 0;

// ---- timing ----
static double now_seconds(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (double)ts.tv_sec + (double)ts.tv_nsec * 1e-9;
}
static double g_t0 = 0.0;

// ---- audio ----
static AAudioStream *g_audio = NULL;
static volatile int  g_audio_reopen = 0;   // set by the error callback when the route changes

// Called on a SEPARATE (non-audio) thread when the stream is DISCONNECTED — a real route
// change (headphones in/out, device switch), NOT rotation. AAudio requires you reopen from
// another thread, so just flag it; the main loop does the reopen. This is the correct place
// to recover audio — decoupled from the window/orientation lifecycle.
static void audio_error_cb(AAudioStream *s, void *ud, aaudio_result_t err) {
    (void)s; (void)ud;
    if (err == AAUDIO_ERROR_DISCONNECTED) g_audio_reopen = 1;
}

static aaudio_data_callback_result_t audio_cb(
        AAudioStream *s, void *ud, void *audioData, int32_t numFrames) {
    (void)s; (void)ud;
    // de_audio_render fills STEREO INTERLEAVED floats for `numFrames` frames.
    de_audio_render((float *)audioData, numFrames);
    return AAUDIO_CALLBACK_RESULT_CONTINUE;
}

static void audio_start(void) {
    AAudioStreamBuilder *b = NULL;
    if (AAudio_createStreamBuilder(&b) != AAUDIO_OK) { LOGE("AAudio builder failed"); return; }
    AAudioStreamBuilder_setFormat(b, AAUDIO_FORMAT_PCM_FLOAT);
    AAudioStreamBuilder_setChannelCount(b, 2);
    AAudioStreamBuilder_setSampleRate(b, 44100);
    // NOTE: LOW_LATENCY (MMAP/exclusive fast path) is SILENT on the macOS emulator's audio
    // HAL — the callback still fires but samples never reach the host speakers. PERFORMANCE_MODE_NONE
    // forces the legacy AudioFlinger mixer path, which routes correctly on the emulator AND real
    // devices. Revisit low-latency once we're testing on hardware.
    AAudioStreamBuilder_setPerformanceMode(b, AAUDIO_PERFORMANCE_MODE_NONE);
    AAudioStreamBuilder_setSharingMode(b, AAUDIO_SHARING_MODE_SHARED);
    AAudioStreamBuilder_setDataCallback(b, audio_cb, NULL);
    AAudioStreamBuilder_setErrorCallback(b, audio_error_cb, NULL);
    // Headroom for a jittery scheduler (the emulator especially). A fixed 512-frame callback
    // (~11.6ms) keeps de_audio_render chunks steady; a large capacity lets the buffer sit deep
    // enough not to under-run (crackle). ~23ms latency — fine for this, tune down on device.
    AAudioStreamBuilder_setFramesPerDataCallback(b, 512);
    AAudioStreamBuilder_setBufferCapacityInFrames(b, 8192);
    aaudio_result_t r = AAudioStreamBuilder_openStream(b, &g_audio);
    AAudioStreamBuilder_delete(b);
    if (r != AAUDIO_OK || !g_audio) { LOGE("AAudio open failed: %d", r); g_audio = NULL; return; }
    // Grow the buffer to several bursts so a jittery scheduler (esp. the emulator, and a
    // busy render frame) doesn't under-run -> crackle. Latency cost is small; tune on device.
    int32_t burst = AAudioStream_getFramesPerBurst(g_audio);
    if (burst > 0) AAudioStream_setBufferSizeInFrames(g_audio, burst * 8);
    AAudioStream_requestStart(g_audio);
    LOGI("AAudio started: %d Hz, %d ch",
         AAudioStream_getSampleRate(g_audio), AAudioStream_getChannelCount(g_audio));
}

static void audio_stop(void) {
    if (!g_audio) return;
    AAudioStream_requestStop(g_audio);
    AAudioStream_close(g_audio);
    g_audio = NULL;
}

// ---- microphone INPUT (mic-and-sampling.md Tier-1) — de_audio_input push ----
// The mirror of audio output: a second AAudio stream in the INPUT direction pushes captured
// mono frames into the engine (which only analyzes them → mic_level/mic_pitch). Opened LAZILY
// so a cart that never calls mic_start() never records and never prompts for RECORD_AUDIO.
static AAudioStream *g_mic = NULL;
static int           g_mic_running = 0;
static int           g_mic_asked   = 0;   // RECORD_AUDIO requested once (the prompt is async)

// Runtime permission (API 23+). Pure NativeActivity has no Java Activity to receive
// onRequestPermissionsResult, so we fire the request once and POLL checkSelfPermission. Returns 1
// once RECORD_AUDIO is granted, 0 while pending/denied. Defensive: any JNI miss just returns 0.
static int mic_permission(struct android_app *app) {
    if (!app->activity || !app->activity->vm) return 0;
    JavaVM *vm = app->activity->vm;
    JNIEnv *env = NULL;
    if ((*vm)->AttachCurrentThread(vm, &env, NULL) != JNI_OK || !env) return 0;
    jobject activity = app->activity->clazz;
    jclass  cls = (*env)->GetObjectClass(env, activity);
    int result = 0;
    jmethodID check = (*env)->GetMethodID(env, cls, "checkSelfPermission", "(Ljava/lang/String;)I");
    if (check) {
        jstring perm = (*env)->NewStringUTF(env, "android.permission.RECORD_AUDIO");
        jint granted = (*env)->CallIntMethod(env, activity, check, perm);   // 0 == PERMISSION_GRANTED
        if (granted == 0) result = 1;
        else if (!g_mic_asked) {                                            // fire the prompt ONCE
            g_mic_asked = 1;
            jmethodID req = (*env)->GetMethodID(env, cls, "requestPermissions", "([Ljava/lang/String;I)V");
            jclass strCls = (*env)->FindClass(env, "java/lang/String");
            if (req && strCls) {
                jobjectArray arr = (*env)->NewObjectArray(env, 1, strCls, perm);
                (*env)->CallVoidMethod(env, activity, req, arr, 1);
            }
        }
        (*env)->DeleteLocalRef(env, perm);
    }
    (*vm)->DetachCurrentThread(vm);
    return result;
}

static aaudio_data_callback_result_t mic_cb(
        AAudioStream *s, void *ud, void *audioData, int32_t numFrames) {
    (void)ud;
    de_audio_input((const float *)audioData, numFrames, AAudioStream_getSampleRate(s));   // mono float in
    return AAUDIO_CALLBACK_RESULT_CONTINUE;
}

static void mic_open(void) {
    AAudioStreamBuilder *b = NULL;
    if (AAudio_createStreamBuilder(&b) != AAUDIO_OK) { LOGE("AAudio mic builder failed"); return; }
    AAudioStreamBuilder_setDirection(b, AAUDIO_DIRECTION_INPUT);
    AAudioStreamBuilder_setFormat(b, AAUDIO_FORMAT_PCM_FLOAT);
    AAudioStreamBuilder_setChannelCount(b, 1);                 // mono take
    AAudioStreamBuilder_setSampleRate(b, 44100);
    AAudioStreamBuilder_setPerformanceMode(b, AAUDIO_PERFORMANCE_MODE_NONE);
    AAudioStreamBuilder_setSharingMode(b, AAUDIO_SHARING_MODE_SHARED);
    AAudioStreamBuilder_setDataCallback(b, mic_cb, NULL);
    AAudioStreamBuilder_setFramesPerDataCallback(b, 512);
    aaudio_result_t r = AAudioStreamBuilder_openStream(b, &g_mic);
    AAudioStreamBuilder_delete(b);
    if (r != AAUDIO_OK || !g_mic) { LOGE("AAudio mic open failed: %d", r); g_mic = NULL; return; }
    AAudioStream_requestStart(g_mic);
    g_mic_running = 1;
    de_mic_set_active(1);
    LOGI("AAudio mic started: %d Hz", AAudioStream_getSampleRate(g_mic));
}

static void mic_close(void) {
    if (g_mic) { AAudioStream_requestStop(g_mic); AAudioStream_close(g_mic); g_mic = NULL; }
    g_mic_running = 0;
    de_mic_set_active(0);
}

// Polled once per frame: open/close capture to match the cart's mic_start()/mic_stop()
// (de_mic_wanted). Opens only after RECORD_AUDIO is granted; the grant is async so this keeps
// re-checking until it lands, then opens on the next poll.
static void mic_poll(struct android_app *app) {
    int want = de_mic_wanted();
    if (want && !g_mic_running) { if (mic_permission(app)) mic_open(); }
    else if (!want && g_mic_running) { g_mic_asked = 0; mic_close(); }
}

// ---- immersive fullscreen (hide status + navigation bars; draw under the cutout) ----
// The manifest's Fullscreen theme only drops the STATUS bar. On gesture-nav phones the
// NAVIGATION bar stays, and an edge-swipe pulls the system bars back in — the "cruft" seen
// mid-game. Android CLEARS immersive whenever the window regains focus, so this must be
// re-applied on APP_CMD_GAINED_FOCUS (and after a rotation), not just once at startup.
// targetSdk 35 ignores the legacy setSystemUiVisibility flags, so we prefer
// WindowInsetsController (API 30+) and fall back to setSystemUiVisibility for API 26-29.
// Twin of the iOS edge-gesture deferral (ios/…UIKit-lifecycle root). All JNI is
// exception-guarded: a wrong-thread throw degrades to a no-op, never a crash.
static void android_immersive(struct android_app *app) {
    if (!app || !app->activity || !app->activity->vm) return;

    // thread-safe baseline (glue marshals to the UI thread): fullscreen + edge-to-edge layout
    ANativeActivity_setWindowFlags(app->activity,
        AWINDOW_FLAG_FULLSCREEN | AWINDOW_FLAG_LAYOUT_NO_LIMITS, 0);

    JavaVM *vm = app->activity->vm;
    JNIEnv *env = NULL;
    if ((*vm)->AttachCurrentThread(vm, &env, NULL) != JNI_OK || !env) return;
    jobject activity = app->activity->clazz;
    jclass  acls     = (*env)->GetObjectClass(env, activity);

    int sdk = 26;                                   // Build.VERSION.SDK_INT
    jclass vcls = (*env)->FindClass(env, "android/os/Build$VERSION");
    if (vcls) {
        jfieldID fid = (*env)->GetStaticFieldID(env, vcls, "SDK_INT", "I");
        if (fid) sdk = (*env)->GetStaticIntField(env, vcls, fid);
    }

    jmethodID getWindow = (*env)->GetMethodID(env, acls, "getWindow", "()Landroid/view/Window;");
    jobject window = getWindow ? (*env)->CallObjectMethod(env, activity, getWindow) : NULL;
    jclass  wcls   = window ? (*env)->GetObjectClass(env, window) : NULL;

    if (window && wcls && sdk >= 30) {
        // window.setDecorFitsSystemWindows(false) → content draws edge-to-edge
        jmethodID setDecor = (*env)->GetMethodID(env, wcls, "setDecorFitsSystemWindows", "(Z)V");
        if (setDecor) (*env)->CallVoidMethod(env, window, setDecor, JNI_FALSE);
        // WindowInsetsController c = window.getInsetsController();
        jmethodID getCtl = (*env)->GetMethodID(env, wcls, "getInsetsController",
                                               "()Landroid/view/WindowInsetsController;");
        jobject ctl = getCtl ? (*env)->CallObjectMethod(env, window, getCtl) : NULL;
        if (ctl) {
            jclass ccls = (*env)->GetObjectClass(env, ctl);
            int bars = 0;                            // WindowInsets.Type.systemBars()
            jclass tcls = (*env)->FindClass(env, "android/view/WindowInsets$Type");
            if (tcls) {
                jmethodID sysBars = (*env)->GetStaticMethodID(env, tcls, "systemBars", "()I");
                if (sysBars) bars = (*env)->CallStaticIntMethod(env, tcls, sysBars);
            }
            jmethodID hide = (*env)->GetMethodID(env, ccls, "hide", "(I)V");
            if (hide && bars) (*env)->CallVoidMethod(env, ctl, hide, bars);
            // setSystemBarsBehavior(BEHAVIOR_SHOW_TRANSIENT_BARS_BY_SWIPE = 2): a swipe shows
            // the bars transiently, then they auto-hide — so nav never sticks around mid-game
            jmethodID setBehav = (*env)->GetMethodID(env, ccls, "setSystemBarsBehavior", "(I)V");
            if (setBehav) (*env)->CallVoidMethod(env, ctl, setBehav, 2);
        }
    } else if (window && wcls) {
        // API 26-29: window.getDecorView().setSystemUiVisibility(immersive-sticky flags)
        jmethodID getDecor = (*env)->GetMethodID(env, wcls, "getDecorView", "()Landroid/view/View;");
        jobject decor = getDecor ? (*env)->CallObjectMethod(env, window, getDecor) : NULL;
        if (decor) {
            jclass dcls = (*env)->GetObjectClass(env, decor);
            jmethodID setVis = (*env)->GetMethodID(env, dcls, "setSystemUiVisibility", "(I)V");
            // LAYOUT_STABLE|LAYOUT_HIDE_NAVIGATION|LAYOUT_FULLSCREEN|HIDE_NAVIGATION|FULLSCREEN|IMMERSIVE_STICKY
            if (setVis) (*env)->CallVoidMethod(env, decor, setVis, 0x100|0x200|0x400|0x2|0x4|0x1000);
        }
    }

    if ((*env)->ExceptionCheck(env)) (*env)->ExceptionClear(env);   // e.g. wrong-thread → no-op
    (*vm)->DetachCurrentThread(vm);
}

// ---- GLES helpers ----
static GLuint compile_shader(GLenum type, const char *src) {
    GLuint s = glCreateShader(type);
    glShaderSource(s, 1, &src, NULL);
    glCompileShader(s);
    GLint ok = 0; glGetShaderiv(s, GL_COMPILE_STATUS, &ok);
    if (!ok) { char log[512]; glGetShaderInfoLog(s, sizeof log, NULL, log); LOGE("shader: %s", log); }
    return s;
}

static const char *VS =
    // sw_cbuf is BOTTOM-UP and GLES texel (0,0) is uv (0,0) -> no Y flip needed:
    // screen bottom-left (NDC -1,-1 / uv 0,0) samples the image's bottom-left texel.
    "attribute vec2 aPos; attribute vec2 aUV; varying vec2 vUV;\n"
    "void main(){ vUV = aUV; gl_Position = vec4(aPos, 0.0, 1.0); }\n";
static const char *FS =
    "precision mediump float; varying vec2 vUV; uniform sampler2D uTex;\n"
    "void main(){ gl_FragColor = texture2D(uTex, vUV); }\n";

static void gl_setup(void) {
    g_prog = glCreateProgram();
    GLuint vs = compile_shader(GL_VERTEX_SHADER, VS);
    GLuint fs = compile_shader(GL_FRAGMENT_SHADER, FS);
    glAttachShader(g_prog, vs);
    glAttachShader(g_prog, fs);
    glLinkProgram(g_prog);
    g_aPos = glGetAttribLocation(g_prog, "aPos");
    g_aUV  = glGetAttribLocation(g_prog, "aUV");
    g_uTex = glGetUniformLocation(g_prog, "uTex");

    glGenTextures(1, &g_tex);
    glBindTexture(GL_TEXTURE_2D, g_tex);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);   // chunky lo-fi
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    // allocate storage once at the framebuffer size (RGBA8888 = bytes R,G,B,A)
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, g_sw, g_sh, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 4);
}

static int egl_init(struct android_app *app) {
    g_dpy = eglGetDisplay(EGL_DEFAULT_DISPLAY);
    eglInitialize(g_dpy, NULL, NULL);

    const EGLint cfg_attr[] = {
        EGL_RENDERABLE_TYPE, EGL_OPENGL_ES2_BIT,
        EGL_SURFACE_TYPE, EGL_WINDOW_BIT,
        EGL_BLUE_SIZE, 8, EGL_GREEN_SIZE, 8, EGL_RED_SIZE, 8, EGL_ALPHA_SIZE, 8,
        EGL_NONE
    };
    EGLConfig cfg; EGLint n = 0;
    eglChooseConfig(g_dpy, cfg_attr, &cfg, 1, &n);
    if (n < 1) { LOGE("no EGL config"); return 0; }

    g_sfc = eglCreateWindowSurface(g_dpy, cfg, app->window, NULL);
    const EGLint ctx_attr[] = { EGL_CONTEXT_CLIENT_VERSION, 2, EGL_NONE };
    g_ctx = eglCreateContext(g_dpy, cfg, EGL_NO_CONTEXT, ctx_attr);
    if (!eglMakeCurrent(g_dpy, g_sfc, g_sfc, g_ctx)) { LOGE("eglMakeCurrent failed"); return 0; }
    eglSwapInterval(g_dpy, 1);   // vsync — throttle the render loop instead of busy-spinning the CPU

    eglQuerySurface(g_dpy, g_sfc, EGL_WIDTH,  &g_view_w);
    eglQuerySurface(g_dpy, g_sfc, EGL_HEIGHT, &g_view_h);
    gl_setup();
    LOGI("EGL up: surface %dx%d, framebuffer %dx%d", g_view_w, g_view_h, g_sw, g_sh);
    return 1;
}

static void egl_teardown(void) {
    if (g_dpy != EGL_NO_DISPLAY) {
        eglMakeCurrent(g_dpy, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
        if (g_ctx != EGL_NO_CONTEXT) eglDestroyContext(g_dpy, g_ctx);
        if (g_sfc != EGL_NO_SURFACE) eglDestroySurface(g_dpy, g_sfc);
        eglTerminate(g_dpy);
    }
    g_dpy = EGL_NO_DISPLAY; g_sfc = EGL_NO_SURFACE; g_ctx = EGL_NO_CONTEXT;
    g_tex = 0; g_prog = 0; g_ready = 0;
}

static void compute_letterbox(void) {
    float sa = (float)g_sw / (float)g_sh;
    float va = (float)g_view_w / (float)g_view_h;
    if (va > sa) { g_dh = g_view_h; g_dw = (int)(g_dh * sa); }
    else         { g_dw = g_view_w; g_dh = (int)(g_dw / sa); }
    g_dx = (g_view_w - g_dw) / 2;
    g_dy = (g_view_h - g_dh) / 2;
}

static void draw_frame(void) {
    if (!g_ready) return;
    de_frame(now_seconds() - g_t0);            // engine draws into sw_cbuf

    const uint32_t *fb = de_framebuffer();
    glBindTexture(GL_TEXTURE_2D, g_tex);
    glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, g_sw, g_sh, GL_RGBA, GL_UNSIGNED_BYTE, fb);

    // black letterbox bars, then the aspect-fit quad
    glViewport(0, 0, g_view_w, g_view_h);
    glClearColor(0, 0, 0, 1);
    glClear(GL_COLOR_BUFFER_BIT);

    compute_letterbox();
    glViewport(g_dx, g_dy, g_dw, g_dh);

    glUseProgram(g_prog);
    // fullscreen quad (pos in NDC, uv 0..1); Y flip happens in the vertex shader
    const GLfloat verts[] = {
        //  x     y     u    v
        -1.f, -1.f,  0.f, 0.f,
         1.f, -1.f,  1.f, 0.f,
        -1.f,  1.f,  0.f, 1.f,
         1.f,  1.f,  1.f, 1.f,
    };
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, g_tex);
    glUniform1i(g_uTex, 0);
    glEnableVertexAttribArray(g_aPos);
    glVertexAttribPointer(g_aPos, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(GLfloat), verts);
    glEnableVertexAttribArray(g_aUV);
    glVertexAttribPointer(g_aUV, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(GLfloat), verts + 2);
    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);

    eglSwapBuffers(g_dpy, g_sfc);
}

// ---- input: motion -> de_touch_* (mapped through the letterbox to framebuffer px) ----
static void map_touch(float px, float py, float *fx, float *fy) {
    float x = (px - g_dx) / (float)g_dw * (float)g_sw;
    float y = (py - g_dy) / (float)g_dh * (float)g_sh;   // top-down, matches de_touch
    if (x < 0) x = 0; if (x > g_sw - 1) x = g_sw - 1;
    if (y < 0) y = 0; if (y > g_sh - 1) y = g_sh - 1;
    *fx = x; *fy = y;
}

static int32_t handle_input(struct android_app *app, AInputEvent *ev) {
    (void)app;
    if (AInputEvent_getType(ev) != AINPUT_EVENT_TYPE_MOTION) return 0;

    int32_t action = AMotionEvent_getAction(ev);
    int32_t flag   = action & AMOTION_EVENT_ACTION_MASK;
    int32_t idx    = (action & AMOTION_EVENT_ACTION_POINTER_INDEX_MASK)
                        >> AMOTION_EVENT_ACTION_POINTER_INDEX_SHIFT;
    float fx, fy;

    switch (flag) {
        case AMOTION_EVENT_ACTION_DOWN:
        case AMOTION_EVENT_ACTION_POINTER_DOWN: {
            int id = AMotionEvent_getPointerId(ev, idx);
            map_touch(AMotionEvent_getX(ev, idx), AMotionEvent_getY(ev, idx), &fx, &fy);
            de_touch_begin(id, fx, fy);
            break;
        }
        case AMOTION_EVENT_ACTION_MOVE: {
            size_t n = AMotionEvent_getPointerCount(ev);
            for (size_t i = 0; i < n; i++) {
                int id = AMotionEvent_getPointerId(ev, i);
                map_touch(AMotionEvent_getX(ev, i), AMotionEvent_getY(ev, i), &fx, &fy);
                de_touch_moved(id, fx, fy);
            }
            break;
        }
        case AMOTION_EVENT_ACTION_UP:
        case AMOTION_EVENT_ACTION_POINTER_UP:
        case AMOTION_EVENT_ACTION_CANCEL: {
            int id = AMotionEvent_getPointerId(ev, idx);
            map_touch(AMotionEvent_getX(ev, idx), AMotionEvent_getY(ev, idx), &fx, &fy);
            de_touch_ended(id, fx, fy);
            break;
        }
        default: return 0;
    }
    return 1;
}

// ---- lifecycle ----
static void handle_cmd(struct android_app *app, int32_t cmd) {
    switch (cmd) {
        // The RENDER SURFACE (EGL) follows the window/orientation lifecycle. Audio does NOT —
        // it starts once (android_main) and plays straight through rotation, recovered only via
        // audio_error_cb on a GENUINE route change (headphones, device switch). Do not reopen on
        // rotation: on a real device it's unnecessary, and the churn breaks the emulator's fragile
        // audio route. The emulator's rotation-silence is a known emulator limitation, not ours.
        case APP_CMD_INIT_WINDOW:
            if (app->window && egl_init(app)) { g_ready = 1; g_t0 = now_seconds(); }
            android_immersive(app);          // hide the system bars for this window
            break;
        case APP_CMD_TERM_WINDOW:
            egl_teardown();
            break;
        // Android drops immersive mode on every focus regain and after a rotation — re-assert it
        // or the nav bar creeps back the moment the player interacts. This is the piece that was
        // missing: fullscreen was set once at startup and never restored.
        case APP_CMD_GAINED_FOCUS:
            android_immersive(app);
            break;
        case APP_CMD_WINDOW_RESIZED:
        case APP_CMD_CONFIG_CHANGED:
            if (g_dpy != EGL_NO_DISPLAY && g_sfc != EGL_NO_SURFACE) {
                eglQuerySurface(g_dpy, g_sfc, EGL_WIDTH,  &g_view_w);
                eglQuerySurface(g_dpy, g_sfc, EGL_HEIGHT, &g_view_h);
            }
            android_immersive(app);          // rotation can reset the bars — re-hide
            break;
        default: break;
    }
}

void android_main(struct android_app *app) {
    app->onAppCmd     = handle_cmd;
    app->onInputEvent = handle_input;

    // Persistence: point saves at this app's private internal dir (survives restart, wiped on
    // uninstall). MUST precede de_init — the cart's init() may load_int(). internalDataPath can be
    // NULL very early on some devices; if so we skip it and saves no-op (same as desktop with cwd).
    if (app->activity && app->activity->internalDataPath) {
        char sdir[1024];
        snprintf(sdir, sizeof sdir, "%s/saves", app->activity->internalDataPath);
        de_set_save_dir(sdir);
        LOGI("save dir: %s", sdir);
    } else {
        LOGE("no internalDataPath — saves disabled this run");
    }

    de_init(DE_RENDERER_SOFTWARE);
    g_sw = de_screen_w();
    g_sh = de_screen_h();
    LOGI("engine init: framebuffer %dx%d", g_sw, g_sh);

    audio_start();               // once — independent of the window; survives rotation

    while (!app->destroyRequested) {
        int events;
        struct android_poll_source *src;
        // don't block while we have a window to animate; block (-1) when we don't
        while (ALooper_pollOnce(g_ready ? 0 : -1, NULL, &events, (void **)&src) >= 0) {
            if (src) src->process(app, src);
            if (app->destroyRequested) { mic_close(); audio_stop(); egl_teardown(); return; }
        }
        if (g_audio_reopen) {    // a real route change disconnected the stream — reopen it
            g_audio_reopen = 0;
            audio_stop();
            audio_start();
        }
        mic_poll(app);           // open/close the mic to match the cart's mic_start()/mic_stop()
        draw_frame();
    }
    mic_close();
    audio_stop();
    egl_teardown();
}
