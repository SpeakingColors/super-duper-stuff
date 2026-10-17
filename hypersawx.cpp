/*
 * HyperSawX : 15-voice supersaw + sub oscillator for KORG minilogue xd
 * Author: SpeakingColors
 *
 * 15 band-limited saws (7 below, 1 center, 7 above) spread with the
 * JP-8000 detune curve, plus a sine sub one octave down.
 *
 *   SHAPE        : Detune (also follows the xd's LFO / mod routing)
 *   SHIFT+SHAPE  : Tone (left = darker, middle = neutral, right = brighter)
 *   Sub          : Sub oscillator level
 *   Drift        : Each voice slowly wanders in pitch, like analog
 *   Vib Depth    : Vibrato depth (up to about 25 cents)
 *   Vib Rate     : Vibrato speed, 0.5 to 8 Hz
 */

#include "userosc.h"

#define NUM_VOICES 15
#define CENTER 7

// Voice spread, -1..+1 (outer voices spaced a bit wider, JP-8000 style)
static const float k_spread[NUM_VOICES] = {
  -1.000f, -0.780f, -0.580f, -0.410f, -0.260f, -0.140f, -0.050f, 0.f,
   0.052f,  0.143f,  0.262f,  0.413f,  0.583f,  0.782f,  1.000f
};
static const float k_max_detune = 0.11f;   // JP-8000 outer voice at full detune
static const float k_drift_max  = 0.004f;  // about 7 cents
static const float k_vib_max    = 0.0145f; // about 25 cents
static const float k_out_gain   = 0.85f;

struct State {
  float phase[NUM_VOICES];
  float sub_phase;
  float drift[NUM_VOICES], drift_t[NUM_VOICES];
  float vib_phase;
  float shape, shiftshape;   // 0..1
  float sub, drift_amt, vib_depth, vib_rate;
  float tone_lp;
  float hp_x1, hp_y1;
  uint32_t tick;
};
static State s;

static inline float rand01(void) {
  return (float)(osc_rand() >> 8) * 5.96046447753906e-008f; // /2^24
}

// JP-8000 detune knob response (Adam Szabo, 2010)
static inline float jp_detune_curve(float x) {
  float y = 10028.7312891634f;
  y = y * x - 50818.8652045924f;
  y = y * x + 111363.4808729368f;
  y = y * x - 138150.6761080548f;
  y = y * x + 106649.6679158292f;
  y = y * x - 53046.9642751875f;
  y = y * x + 17019.9518580080f;
  y = y * x - 3425.0836591318f;
  y = y * x + 404.2703938388f;
  y = y * x - 24.1878824391f;
  y = y * x + 0.6717417634f;
  y = y * x + 0.0030115596f;
  return clipminmaxf(0.f, y, 1.f);
}

static inline float poly_blep(float t, float dt, float inv_dt) {
  if (t < dt) { t *= inv_dt; return t + t - t * t - 1.f; }
  if (t > 1.f - dt) { t = (t - 1.f) * inv_dt; return t * t + t + t + 1.f; }
  return 0.f;
}

void OSC_INIT(uint32_t platform, uint32_t api) {
  (void)platform; (void)api;
  for (int v = 0; v < NUM_VOICES; ++v) { s.phase[v] = rand01(); s.drift[v] = s.drift_t[v] = 0.f; }
  s.sub_phase = 0.f; s.vib_phase = 0.f;
  s.shape = 0.5f; s.shiftshape = 0.5f;
  s.sub = 0.3f; s.drift_amt = 0.2f; s.vib_depth = 0.f; s.vib_rate = 0.3f;
  s.tone_lp = 0.f; s.hp_x1 = s.hp_y1 = 0.f; s.tick = 0;
}

void OSC_CYCLE(const user_osc_param_t * const params, int32_t *yn, const uint32_t frames) {
  const float w0 = osc_w0f_for_note((params->pitch) >> 8, params->pitch & 0xFF);
  const float detune = jp_detune_curve(clipminmaxf(0.f, s.shape + q31_to_f32(params->shape_lfo), 1.f)) * k_max_detune;

  // Vibrato: one value per block is smooth enough (blocks are <= 64 samples)
  const float vib_hz = 0.5f + 7.5f * s.vib_rate;
  s.vib_phase += vib_hz * (float)frames * (1.f / 48000.f);
  if (s.vib_phase >= 1.f) s.vib_phase -= 1.f;
  const float vib = 1.f + s.vib_depth * k_vib_max * osc_sinf(s.vib_phase);

  // Drift: pick new random targets now and then, glide toward them
  if ((s.tick++ & 63) == 0)
    for (int v = 0; v < NUM_VOICES; ++v) s.drift_t[v] = s.drift_amt * k_drift_max * (2.f * rand01() - 1.f);

  // Center voice a bit louder than the sides; normalise the sum
  const float g_center = 1.f, g_side = 0.62f;
  const float norm = 1.f / (g_center + (NUM_VOICES - 1) * g_side);

  float acc[64];  // SDK guarantees frames <= 64
  for (uint32_t i = 0; i < frames; ++i) acc[i] = 0.f;

  for (int v = 0; v < NUM_VOICES; ++v) {
    s.drift[v] += 0.05f * (s.drift_t[v] - s.drift[v]);
    const float dt = clipmaxf(w0 * vib * (1.f + detune * k_spread[v] + s.drift[v]), 0.45f);
    const float inv_dt = 1.f / dt;
    const float g = ((v == CENTER) ? g_center : g_side) * norm;
    float p = s.phase[v];
    for (uint32_t i = 0; i < frames; ++i) {
      acc[i] += g * ((2.f * p - 1.f) - poly_blep(p, dt, inv_dt));
      p += dt; if (p >= 1.f) p -= 1.f;
    }
    s.phase[v] = p;
  }

  // Sub: sine one octave down
  const float sub_dt = 0.5f * w0 * vib;
  float sp = s.sub_phase;
  const float sub_g = 0.7f * s.sub;

  // Tone tilt around a ~1.5 kHz one-pole split
  const float t = s.shiftshape;              // 0 dark, 0.5 neutral, 1 bright
  const float low_g  = (t < 0.5f) ? 1.f : 1.f - 0.5f * (t - 0.5f) * 2.f;
  const float high_g = (t < 0.5f) ? t * 2.f : 1.f + (t - 0.5f) * 2.f;
  const float lp_a = 0.18f;

  const float hp_a = 1.f / (1.f + 6.2831853f * 0.5f * w0);  // remove DC below the sub
  float lp = s.tone_lp, hp_x1 = s.hp_x1, hp_y1 = s.hp_y1;
  q31_t * __restrict y = (q31_t *)yn;
  for (uint32_t i = 0; i < frames; ++i) {
    lp += lp_a * (acc[i] - lp);
    float sig = low_g * lp + high_g * (acc[i] - lp);
    sig += sub_g * osc_sinf(sp);
    sp += sub_dt; if (sp >= 1.f) sp -= 1.f;
    const float hp = hp_a * (hp_y1 + sig - hp_x1);
    hp_x1 = sig; hp_y1 = hp;
    y[i] = f32_to_q31(osc_softclipf(0.05f, hp * k_out_gain));
  }
  s.tone_lp = lp; s.hp_x1 = hp_x1; s.hp_y1 = hp_y1; s.sub_phase = sp;
}

void OSC_NOTEON(const user_osc_param_t * const params) {
  (void)params;
  for (int v = 0; v < NUM_VOICES; ++v) s.phase[v] = rand01();  // free-running feel
}

void OSC_NOTEOFF(const user_osc_param_t * const params) { (void)params; }

void OSC_PARAM(uint16_t index, uint16_t value) {
  switch (index) {
  case k_user_osc_param_id1: s.sub       = clipminmaxf(0.f, value * 0.01f, 1.f); break;
  case k_user_osc_param_id2: s.drift_amt = clipminmaxf(0.f, value * 0.01f, 1.f); break;
  case k_user_osc_param_id3: s.vib_depth = clipminmaxf(0.f, value * 0.01f, 1.f); break;
  case k_user_osc_param_id4: s.vib_rate  = clipminmaxf(0.f, value * 0.01f, 1.f); break;
  case k_user_osc_param_shape:      s.shape = param_val_to_f32(value); break;
  case k_user_osc_param_shiftshape: s.shiftshape = param_val_to_f32(value); break;
  default: break;
  }
}
