/* SPDX-FileCopyrightText: 2026 CATEIN */
/* SPDX-License-Identifier: Apache-2.0 */

#include "screen_reader.h"

#include "applib/ui/menu_layer.h"
#include "applib/ui/window_manager.h"
#include "syscall/syscall.h"

#include <stddef.h>

#define SCREEN_READER_TEXT_MAX 200

static void prv_append(char *buf, size_t *len, const char *part) {
  if (!part || !part[0]) {
    return;
  }
  if (*len && *len + 2 < SCREEN_READER_TEXT_MAX) {
    buf[(*len)++] = ',';
    buf[(*len)++] = ' ';
  }
  while (*part && *len + 1 < SCREEN_READER_TEXT_MAX) {
    buf[(*len)++] = *part++;
  }
  buf[*len] = '\0';
}

void screen_reader_announce_cell(const Layer *cell_layer, const char *title, const char *value,
                                 const char *subtitle) {
#ifdef CONFIG_SERVICE_TTS
  if (!cell_layer || !menu_cell_layer_is_highlighted(cell_layer)) {
    return;
  }
  // windows sliding out still draw their highlighted row
  Window *window = layer_get_window(cell_layer);
  if (!window || !window_manager_is_window_focused(window)) {
    return;
  }
  char text[SCREEN_READER_TEXT_MAX];
  size_t len = 0;
  text[0] = '\0';
  prv_append(text, &len, title);
  prv_append(text, &len, value);
  prv_append(text, &len, subtitle);
  if (len) {
    sys_tts_announce(text);
  }
#endif
}
