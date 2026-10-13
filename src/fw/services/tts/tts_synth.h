/* SPDX-FileCopyrightText: 2026 CATEIN */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define TTS_SYNTH_SAMPLE_RATE 16000

//! Receives signed 8-bit samples at TTS_SYNTH_SAMPLE_RATE.
//! @return false to stop speaking
typedef bool (*TtsSynthWrite)(const int8_t *samples, size_t count, void *ctx);

//! Speak normalized text (see tts_text_normalize()) with SAM's 1982 renderer.
//! Not reentrant: SAM keeps its state in globals.
//! @param female true for Sadie '82, false for the original SAM voice
//! @return false if write() asked to stop
bool tts_synth_speak(const char *normalized, bool female, TtsSynthWrite write, void *ctx);
