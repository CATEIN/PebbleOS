/* SPDX-FileCopyrightText: 2026 CATEIN */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <stddef.h>

//! Rewrite UTF-8 text into what SAM's reciter reads well: upper-case ASCII
//! words and SAM's punctuation. Numbers become words ("10:45" -> "TEN FORTY
//! FIVE"), short all-capital words without vowels are spelled ("PM" -> "P M"),
//! common symbols are named and anything else becomes a space.
//! @return the length written to out (always NUL-terminated)
size_t tts_text_normalize(const char *in, char *out, size_t out_size);

//! Length of the next piece of normalized text to give the reciter: at most
//! max_len characters, ending at a word boundary (after a sentence if one ends
//! late enough). Never 0 while text remains.
size_t tts_text_chunk_length(const char *text, size_t max_len);
