/* SPDX-FileCopyrightText: 2026 CATEIN */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <stdbool.h>
#include <stdint.h>

//! Text-to-speech voices. The values are stored in the alerts preferences.
typedef enum {
  TtsVoiceOff = 0,
  TtsVoiceSadie,  //!< Sadie '82: SAM's 1982 renderer with female data
  TtsVoiceSam,    //!< the original 1982 S.A.M. voice
  TtsVoiceCount,
} TtsVoice;

//! Start the speech thread. Called once at boot.
void tts_init(void);

TtsVoice tts_get_voice(void);
void tts_set_voice(TtsVoice voice);

//! @return The name of a voice for menus, e.g. "On - Sadie".
const char *tts_voice_name(TtsVoice voice);

//! Speak English text with the current voice, cutting off anything still being said.
//! Does nothing while the voice is off. Safe to call from any kernel task.
void tts_speak(const char *text);

//! Like tts_speak(), but ignores text identical to the last announcement, so the
//! screen reader can report the focused item on every redraw.
void tts_announce(const char *text);

//! Stop speaking.
void tts_stop(void);
