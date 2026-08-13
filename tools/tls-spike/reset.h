/*
 * reset.h — the starting state, written ONCE against the bare names and included by all three
 * variants' SETUP. Every trial must begin from identical state or the comparison is meaningless.
 *
 * This is not a detail: the first run of this benchmark had the plain-statics variant keeping its
 * echo buffer across trials while the context variants got a fresh zeroed struct each time. It
 * accumulated feedback into denormals and measured 3x SLOWER than the refactor it was supposed to
 * be the baseline for. The output-equality assertion in main.c is what caught it.
 */
memset(echo_buf, 0, sizeof echo_buf);
memset(vphase,   0, sizeof vphase);
f_z1 = f_z2 = 0.0f; echo_z = 0.0f; echo_pos = 0; cho_ph = 0.0f; sc_env = 0.0f;
f_cut = 0.15f; f_res = 0.3f; echo_fb = 0.35f; echo_tone = 0.55f;
cho_depth = 0.2f; drive_amt = 0.4f; drive_tone = 0.9f; sc_ratio = 0.3f;
master_gain = 1.0f; pan_law_x = 0.5f;
for (int v_ = 0; v_ < 8; v_++) vfreq[v_] = 0.01f + 0.003f * v_;
