/* SPDX-FileCopyrightText: 2026 CATEIN */
/* SPDX-License-Identifier: Apache-2.0 */

#include "syscall/syscall.h"
#include "syscall/syscall_internal.h"

#include "kernel/memory_layout.h"
#include "kernel/pebble_tasks.h"
#include "kernel/ui/modals/modal_manager.h"

#ifdef CONFIG_SERVICE_TTS
#include "pbl/services/tts/tts.h"
#endif

#define TTS_ANNOUNCE_MAX 512

DEFINE_SYSCALL(void, sys_tts_announce, const char *text) {
#ifdef CONFIG_SERVICE_TTS
  if (!text) {
    return;
  }
  if (PRIVILEGE_WAS_ELEVATED &&
      !memory_layout_is_cstring_in_region(memory_layout_get_app_region(), text,
                                          TTS_ANNOUNCE_MAX)) {
    // called while an app draws its menus: ignore bad text instead of killing the app
    return;
  }
  // An app keeps drawing its focused row under a modal window such as a
  // notification; only the modal has the user's attention then.
  if (pebble_task_get_current() == PebbleTask_App && modal_manager_get_enabled() &&
      !(modal_manager_get_properties() & ModalProperty_Unfocused)) {
    return;
  }
  tts_announce(text);
#endif
}
