/*
    BSD 3-Clause License

    Copyright (c) 2018, KORG INC.
    All rights reserved.

    Redistribution and use in source and binary forms, with or without
    modification, are permitted provided that the following conditions are met:

    * Redistributions of source code must retain the above copyright notice, this
      list of conditions and the following disclaimer.

    * Redistributions in binary form must reproduce the above copyright notice,
      this list of conditions and the following disclaimer in the documentation
      and/or other materials provided with the distribution.

    * Neither the name of the copyright holder nor the names of its
      contributors may be used to endorse or promote products derived from
      this software without specific prior written permission.

    THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
    AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
    IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
    DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
    FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
    DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
    SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
    CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
    OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
    OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

//*/

/**
 * @file    _unit.c
 * @brief   HyperSawX
 *
 * @addtogroup api
 * @{
 */

#include "userosc.h"

#define NUM_VOICES 7
#define TOTAL_OSC (NUM_VOICES * 2 + 1)
#define TWO_PI 6.28318530718f

typedef struct State {
  float phase[TOTAL_OSC + 1];     // +1 = sub oscillator
  float drift_phase[TOTAL_OSC];
  float detune, tone, submix, driftdepth;
  float det_factors[TOTAL_OSC];
  float panL[TOTAL_OSC];
  float panR[TOTAL_OSC];
} State;

static State s;

void OSC_INIT(uint32_t platform, uint32_t api) {
  for (int i = 0; i < TOTAL_OSC + 1; i++) {
    s.phase[i] = (float)rand() / (float)RAND_MAX;
    if (i < TOTAL_OSC)
      s.drift_phase[i] = (float)rand() / (float)RAND_MAX;
  }

  // Precompute stereo panning and detune multipliers
  int idx = 0;
  for (int i = 0; i < NUM_VOICES; i++)
    s.det_factors[idx++] = -((float)(i + 1) / (float)NUM_VOICES); // below
  s.det_factors[idx++] = 0.f;  // center
  for (int i = 0; i < NUM_VOICES; i++)
    s.det_factors[idx++] = ((float)(i + 1) / (float)NUM_VOICES);  // above

  // Simple alternating pan law (0.2L to 0.8R spread)
  for (int v = 0; v < TOTAL_OSC; v++) {
    float p = (v % 2 == 0) ? 0.3f : 0.7f; // alternate
    s.panL[v] = cosf(p * 1.5708f); // equal power
    s.panR[v] = sinf(p * 1.5708f);
  }

  s.detune = 0.3f;
  s.tone = 0.5f;
  s.submix = 0.3f;
  s.driftdepth = 0.3f;
}

void OSC_CYCLE(const user_osc_param_t * const params, int32_t *yn, const uint32_t frames) {
  const float base_w0 = osc_w0f_for_note(params->pitch);
  const float det_amt = s.detune * 0.04f;
  const float norm = 1.0f / TOTAL_OSC;
  const float tone_gain = s.tone * 1.6f + 0.4f;
  const float drift_speed = 0.00005f;
  const float drift_scale = s.driftdepth * 0.01f;
  const float submix = s.submix;

  q31_t * __restrict y = (q31_t *)yn;

  for (uint32_t i = 0; i < frames; i++) {
    float sumL = 0.f, sumR = 0.f;

    // unrolled main loop
    for (int v = 0; v < TOTAL_OSC; v++) {
      // per-voice drift
      s.drift_phase[v] += drift_speed;
      if (s.drift_phase[v] >= 1.f) s.drift_phase[v] -= 1.f;
      float drift = sinf(TWO_PI * s.drift_phase[v]) * drift_scale;

      // phase advance
      s.phase[v] += base_w0 * (1.f + s.det_factors[v] * det_amt + drift);
      s.phase[v] -= (uint32_t)s.phase[v];

      float saw = (2.f * s.phase[v]) - 1.f;
      saw *= tone_gain; // tone tilt via gain
      saw = fmaxf(fminf(saw, 1.f), -1.f); // soft clip

      sumL += saw * s.panL[v];
      sumR += saw * s.panR[v];
    }

    // sub oscillator (octave down, mono)
    s.phase[TOTAL_OSC] += base_w0 * 0.5f;
    s.phase[TOTAL_OSC] -= (uint32_t)s.phase[TOTAL_OSC];
    float sub = ((2.f * s.phase[TOTAL_OSC]) - 1.f) * submix;

    float outL = (sumL * norm) + sub;
    float outR = (sumR * norm) + sub;

    // final soft limiter
    outL = fmaxf(fminf(outL * 1.2f, 1.f), -1.f);
    outR = fmaxf(fminf(outR * 1.2f, 1.f), -1.f);

    *y++ = f32_to_q31(outL);
    *y++ = f32_to_q31(outR);
  }
}

void OSC_NOTEON(const user_osc_param_t * const params) {
  for (int i = 0; i < TOTAL_OSC + 1; i++) {
    s.phase[i] = (float)rand() / (float)RAND_MAX;
    if (i < TOTAL_OSC)
      s.drift_phase[i] = (float)rand() / (float)RAND_MAX;
  }
}

void OSC_PARAM(uint16_t index, uint16_t value) {
  switch (index) {
    case k_user_osc_param_id1: s.detune = clipminmaxf(0.f, param_val_to_f32(value), 1.f); break;
    case k_user_osc_param_id2: s.tone = clipminmaxf(0.f, param_val_to_f32(value), 1.f); break;
    case k_user_osc_param_id3: s.submix = clipminmaxf(0.f, param_val_to_f32(value), 1.f); break;
    case k_user_osc_param_id4: s.driftdepth = clipminmaxf(0.f, param_val_to_f32(value), 1.f); break;
    default: break;
  }
}

/** @} */


/** @} */
