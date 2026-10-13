/* SPDX-FileCopyrightText: 2026 CATEIN */
/* SPDX-License-Identifier: Apache-2.0 */

#include "tts_synth.h"
#include "tts_text.h"

#include <string.h>

#include "sam/sam_fw.h"
#include "sam/reciter.h"
#include "sam/render.h"
#include "sam/sam.h"

// SAM renders at 22050 Hz; output sample k sits at SAM sample k * 441 / 320
#define SAM_STEP_NUM 441
#define SAM_STEP_DEN 320

// Input characters per reciter run. The reciter stops by itself once its
// output passes 120 characters and reports where (reciterResume).
#define RECITER_INPUT_MAX 200

static struct {
  TtsSynthWrite write;
  void *ctx;
  bool stopped;
  uint32_t in_count;
  uint32_t out_count;
  int32_t prev;
  int8_t buf[64];
  size_t buf_len;
} s_synth;

static unsigned char s_phonemes[256];

static void prv_flush(void) {
  if (s_synth.buf_len && !s_synth.stopped &&
      !s_synth.write(s_synth.buf, s_synth.buf_len, s_synth.ctx)) {
    s_synth.stopped = true;
    samAbort = 1;
  }
  s_synth.buf_len = 0;
}

//! SAM's unsigned 8-bit samples in, linearly resampled to 16 kHz
static void prv_sink(const unsigned char *samples, int count) {
  for (int i = 0; i < count; i++) {
    const int32_t x = samples[i];
    // emit the outputs that fall between the previous sample and this one
    while ((uint64_t)s_synth.out_count * SAM_STEP_NUM <
           (uint64_t)s_synth.in_count * SAM_STEP_DEN) {
      const int32_t frac = (int32_t)((uint64_t)s_synth.out_count * SAM_STEP_NUM -
                                     (uint64_t)(s_synth.in_count - 1) * SAM_STEP_DEN);
      const int32_t y = s_synth.prev + (x - s_synth.prev) * frac / SAM_STEP_DEN;
      s_synth.buf[s_synth.buf_len++] = (int8_t)(y - 128);
      s_synth.out_count++;
      if (s_synth.buf_len == sizeof(s_synth.buf)) {
        prv_flush();
      }
    }
    s_synth.prev = x;
    s_synth.in_count++;
  }
}

bool tts_synth_speak(const char *normalized, bool female, TtsSynthWrite write, void *ctx) {
  memset(&s_synth, 0, sizeof(s_synth));
  s_synth.write = write;
  s_synth.ctx = ctx;
  s_synth.prev = 128;

  SetSamSink(prv_sink);
  SetSamFemale(female ? 1 : 0);
  SetSpeed(72);
  SetPitch(64);
  SetMouth(128);
  SetThroat(128);

  const char *p = normalized;
  while (*p && !s_synth.stopped) {
    while (*p == ' ') {
      p++;
    }
    if (!*p) {
      break;
    }
    size_t len = tts_text_chunk_length(p, RECITER_INPUT_MAX);
    memset(s_phonemes, 0, sizeof(s_phonemes));
    memcpy(s_phonemes, p, len);
    s_phonemes[len] = '[';
    if (TextToPhonemes(s_phonemes)) {
      if (reciterResume > 0 && (size_t)reciterResume < len) {
        len = (size_t)reciterResume;
      }
      samAbort = 0;
      SetInput((char *)s_phonemes);
      SAMMain();
      SamStreamFlush();
    }
    p += len;
  }
  prv_flush();
  return !s_synth.stopped;
}
