/* SPDX-FileCopyrightText: 2026 CATEIN */
/* SPDX-License-Identifier: Apache-2.0 */

#include "pbl/services/tts/tts.h"

#include "tts_synth.h"
#include "tts_text.h"

#include "kernel/pebble_tasks.h"
#include "pbl/kernel/mutex.h"
#include "pbl/kernel/sem.h"
#include "pbl/kernel/thread.h"
#include "pbl/services/notifications/alerts_preferences.h"
#include "pbl/services/speaker/speaker_service.h"
#include "pbl/services/system_task.h"
#include <pbl/logging/logging.h>

#include <string.h>

PBL_LOG_MODULE_DEFINE(service_tts, CONFIG_SERVICE_TTS_LOG_LEVEL);

#define TTS_TEXT_MAX       512
#define TTS_NORMALIZED_MAX 1024
#define TTS_VOLUME         100
#define TTS_RETRY_MS       20
// How long to wait for a sound that is already playing (e.g. a notification tone)
#define TTS_BUSY_WAIT_MS   100
#define TTS_BUSY_WAITS     30

// Speech is a thread rather than a PebbleTask, so it tags its speaker stream
// with a task no other client streams from.
#define TTS_SPEAKER_OWNER PebbleTask_KernelBackground

// Same as the system task: synthesis runs in short bursts far ahead of
// playback and must never hold up the UI.
#define TTS_THREAD_PRIORITY (PBL_PRIO_IDLE + 1)

PBL_THREAD_STACK_DEFINE(s_tts_stack, 2048);
static struct pbl_thread s_tts_thread;
static PBL_SEM_DEFINE(s_wake, 0, 1);
static PBL_SEM_DEFINE(s_open_done, 0, 1);
static PBL_MUTEX_DEFINE(s_lock);

// Guarded by s_lock
static char s_pending[TTS_TEXT_MAX];
static bool s_has_pending;
static char s_last_announced[TTS_TEXT_MAX];

// Bumped for every new request; speech in progress stops when it changes
static volatile uint32_t s_generation;

// Speech thread only
static char s_text[TTS_TEXT_MAX];
static char s_normalized[TTS_NORMALIZED_MAX];
static bool s_stream_open;
static bool s_open_result;

// Starting and stopping the speaker reaches the audio driver, so it runs on the
// system task, where the speaker service stops finished streams too. The
// speech thread only writes to, and flushes, a stream that is already open.
static void prv_open_cb(void *data) {
  const uint32_t generation = (uint32_t)(uintptr_t)data;
  s_open_result = s_generation == generation &&
                  speaker_service_stream_open_owned(SpeakerPriorityNotification, TTS_VOLUME,
                                                    SpeakerPcmFormat_16kHz_8bit,
                                                    TTS_SPEAKER_OWNER);
  pbl_sem_give(&s_open_done);
}

static void prv_close_cb(void *data) {
  speaker_service_stream_close_owned(TTS_SPEAKER_OWNER);
}

static void prv_stop_cb(void *data) {
  speaker_service_stop_for_task(TTS_SPEAKER_OWNER);
}

static bool prv_open_stream(uint32_t generation) {
  if (!system_task_add_callback(prv_open_cb, (void *)(uintptr_t)generation)) {
    return false;
  }
  pbl_sem_take(&s_open_done, PBL_FOREVER);
  return s_open_result;
}

typedef struct {
  uint32_t generation;
  int busy_waits;
} SpeakContext;

static bool prv_write(const int8_t *samples, size_t count, void *ctx) {
  SpeakContext *speak = ctx;
  while (count) {
    if (s_generation != speak->generation) {
      return false;
    }
    if (!s_stream_open) {
      if (!prv_open_stream(speak->generation)) {
        // the speaker is playing something as important; wait for it a little
        if (++speak->busy_waits > TTS_BUSY_WAITS) {
          PBL_LOG_DBG("Speaker busy, not speaking");
          return false;
        }
        pbl_thread_sleep(PBL_MSEC(TTS_BUSY_WAIT_MS));
        continue;
      }
      s_stream_open = true;
    }
    const uint32_t written = speaker_service_stream_write_owned(TTS_SPEAKER_OWNER, samples, count);
    samples += written;
    count -= written;
    if (count) {
      if (!speaker_service_stream_is_owned(TTS_SPEAKER_OWNER)) {
        // something more important took the speaker
        s_stream_open = false;
        return false;
      }
      pbl_thread_sleep(PBL_MSEC(TTS_RETRY_MS));
    }
  }
  return true;
}

static bool prv_has_pending(void) {
  pbl_mutex_lock(&s_lock, PBL_FOREVER);
  const bool pending = s_has_pending;
  pbl_mutex_unlock(&s_lock);
  return pending;
}

static void prv_speak(const char *text, uint32_t generation) {
  // cut off the previous speech but keep its stream, so the speaker isn't restarted
  s_stream_open = speaker_service_stream_flush_owned(TTS_SPEAKER_OWNER);

  const TtsVoice voice = tts_get_voice();
  tts_text_normalize(text, s_normalized, sizeof(s_normalized));
  if (voice != TtsVoiceOff && s_normalized[0]) {
    SpeakContext ctx = {.generation = generation};
    tts_synth_speak(s_normalized, voice == TtsVoiceSadie, prv_write, &ctx);
  }

  // When more speech is waiting it reuses the stream; otherwise let the queued
  // audio play out and the speaker stop by itself.
  if (s_stream_open && !prv_has_pending()) {
    system_task_add_callback(prv_close_cb, NULL);
    s_stream_open = false;
  }
}

static void prv_thread_main(void *arg) {
  while (true) {
    pbl_sem_take(&s_wake, PBL_FOREVER);

    pbl_mutex_lock(&s_lock, PBL_FOREVER);
    const bool has_text = s_has_pending;
    if (has_text) {
      strncpy(s_text, s_pending, sizeof(s_text));
      s_has_pending = false;
    }
    const uint32_t generation = s_generation;
    pbl_mutex_unlock(&s_lock);

    if (has_text) {
      prv_speak(s_text, generation);
    }
  }
}

void tts_init(void) {
  struct pbl_thread_attr attr = {
    .name = "TTS",
    .entry = prv_thread_main,
    .prio = TTS_THREAD_PRIORITY,
    .privileged = true,
    .stack = s_tts_stack,
    .stack_size = sizeof(s_tts_stack),
  };
  if (pbl_thread_create(&s_tts_thread, &attr) != 0) {
    PBL_LOG_ERR("Could not start the speech thread");
  }
}

TtsVoice tts_get_voice(void) {
  const uint8_t voice = alerts_preferences_get_tts_voice();
  return voice < TtsVoiceCount ? (TtsVoice)voice : TtsVoiceOff;
}

void tts_set_voice(TtsVoice voice) {
  if (voice >= TtsVoiceCount) {
    voice = TtsVoiceOff;
  }
  alerts_preferences_set_tts_voice(voice);
  if (voice == TtsVoiceOff) {
    tts_stop();
  }
}

const char *tts_voice_name(TtsVoice voice) {
  switch (voice) {
    case TtsVoiceSadie:
      return "On - Sadie";
    case TtsVoiceSam:
      return "On - SAM";
    default:
      return "Off";
  }
}

static void prv_request(const char *text) {
  pbl_mutex_lock(&s_lock, PBL_FOREVER);
  strncpy(s_pending, text, sizeof(s_pending) - 1);
  s_pending[sizeof(s_pending) - 1] = '\0';
  s_has_pending = true;
  s_generation++;
  pbl_mutex_unlock(&s_lock);
  pbl_sem_give(&s_wake);
}

void tts_speak(const char *text) {
  if (!text || tts_get_voice() == TtsVoiceOff) {
    return;
  }
  pbl_mutex_lock(&s_lock, PBL_FOREVER);
  strncpy(s_last_announced, text, sizeof(s_last_announced) - 1);
  s_last_announced[sizeof(s_last_announced) - 1] = '\0';
  pbl_mutex_unlock(&s_lock);
  prv_request(text);
}

void tts_announce(const char *text) {
  if (!text || !text[0] || tts_get_voice() == TtsVoiceOff) {
    return;
  }
  pbl_mutex_lock(&s_lock, PBL_FOREVER);
  const bool repeat = strncmp(s_last_announced, text, sizeof(s_last_announced) - 1) == 0;
  if (!repeat) {
    strncpy(s_last_announced, text, sizeof(s_last_announced) - 1);
    s_last_announced[sizeof(s_last_announced) - 1] = '\0';
  }
  pbl_mutex_unlock(&s_lock);
  if (!repeat) {
    prv_request(text);
  }
}

void tts_stop(void) {
  pbl_mutex_lock(&s_lock, PBL_FOREVER);
  s_has_pending = false;
  s_generation++;
  s_last_announced[0] = '\0';
  pbl_mutex_unlock(&s_lock);
  system_task_add_callback(prv_stop_cb, NULL);
}
