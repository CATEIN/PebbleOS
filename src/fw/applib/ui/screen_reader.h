/* SPDX-FileCopyrightText: 2026 CATEIN */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include "applib/ui/layer.h"

//! Speak a menu row when it is the highlighted row of the focused window and
//! text to speech is on. Repeated calls with the same text say nothing, so
//! this can be called on every redraw. Any of the strings may be NULL.
void screen_reader_announce_cell(const Layer *cell_layer, const char *title, const char *value,
                                 const char *subtitle);
