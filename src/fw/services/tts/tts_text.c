/* SPDX-FileCopyrightText: 2026 CATEIN */
/* SPDX-License-Identifier: Apache-2.0 */

#include "tts_text.h"

#include <stdbool.h>
#include <stdint.h>
#include <string.h>

typedef struct {
  char *buf;
  size_t size;
  size_t len;
} Out;

static bool prv_is_digit(char c) {
  return c >= '0' && c <= '9';
}

static bool prv_is_upper(char c) {
  return c >= 'A' && c <= 'Z';
}

static bool prv_is_alpha(char c) {
  return prv_is_upper(c) || (c >= 'a' && c <= 'z');
}

static char prv_upper(char c) {
  return (c >= 'a' && c <= 'z') ? (char)(c - 'a' + 'A') : c;
}

static char prv_last(const Out *o) {
  return o->len ? o->buf[o->len - 1] : ' ';
}

static void prv_put(Out *o, char c) {
  if (c == ' ' && prv_last(o) == ' ') {
    return;
  }
  if (o->len + 1 < o->size) {
    o->buf[o->len++] = c;
  }
}

static void prv_puts(Out *o, const char *s) {
  while (*s) {
    prv_put(o, *s++);
  }
}

static const char *const s_ones[20] = {
  "ZERO",    "ONE",     "TWO",       "THREE",    "FOUR",     "FIVE",    "SIX",
  "SEVEN",   "EIGHT",   "NINE",      "TEN",      "ELEVEN",   "TWELVE",  "THIRTEEN",
  "FOURTEEN", "FIFTEEN", "SIXTEEN",  "SEVENTEEN", "EIGHTEEN", "NINETEEN",
};

static const char *const s_tens[10] = {
  "", "", "TWENTY", "THIRTY", "FORTY", "FIFTY", "SIXTY", "SEVENTY", "EIGHTY", "NINETY",
};

static void prv_put_word(Out *o, const char *word) {
  prv_put(o, ' ');
  prv_puts(o, word);
  prv_put(o, ' ');
}

static void prv_put_below_1000(Out *o, uint32_t n) {
  if (n >= 100) {
    prv_put_word(o, s_ones[n / 100]);
    prv_put_word(o, "HUNDRED");
    n %= 100;
    if (n == 0) {
      return;
    }
  }
  if (n >= 20) {
    prv_put_word(o, s_tens[n / 10]);
    n %= 10;
    if (n == 0) {
      return;
    }
  }
  prv_put_word(o, s_ones[n]);
}

static void prv_put_number(Out *o, uint32_t n) {
  if (n == 0) {
    prv_put_word(o, "ZERO");
    return;
  }
  if (n >= 1000000) {
    prv_put_below_1000(o, n / 1000000);
    prv_put_word(o, "MILLION");
    n %= 1000000;
  }
  if (n >= 1000) {
    prv_put_below_1000(o, n / 1000);
    prv_put_word(o, "THOUSAND");
    n %= 1000;
  }
  if (n) {
    prv_put_below_1000(o, n);
  }
}

static void prv_put_digits(Out *o, const char *digits, size_t count) {
  for (size_t i = 0; i < count; i++) {
    prv_put_word(o, s_ones[digits[i] - '0']);
  }
}

//! A run of digits, with thousands separators ("1,000"). @return its end
static const char *prv_number(Out *o, const char *in, bool after_colon, bool after_point) {
  char digits[12];
  size_t count = 0;
  bool too_long = false;
  const char *p = in;
  while (true) {
    if (prv_is_digit(*p)) {
      if (count < sizeof(digits)) {
        digits[count++] = *p;
      } else {
        too_long = true;
      }
      p++;
    } else if (*p == ',' && count > 0 && prv_is_digit(p[1]) && prv_is_digit(p[2]) &&
               prv_is_digit(p[3]) && !prv_is_digit(p[4])) {
      p++;
    } else {
      break;
    }
  }

  if (too_long) {
    // read long digit strings (codes, phone numbers) digit by digit
    for (const char *d = in; d < p; d++) {
      if (prv_is_digit(*d)) {
        prv_put_word(o, s_ones[*d - '0']);
      }
    }
    return p;
  }
  if (after_point || (count > 1 && digits[0] == '0' && !after_colon) || count > 9) {
    prv_put_digits(o, digits, count);
    return p;
  }
  if (after_colon && count == 2) {
    // minutes: "10:05" -> "TEN OH FIVE", "10:00" -> "TEN"
    if (digits[0] == '0' && digits[1] == '0') {
      return p;
    }
    if (digits[0] == '0') {
      prv_put_word(o, "OH");
      prv_put_word(o, s_ones[digits[1] - '0']);
      return p;
    }
  }
  uint32_t value = 0;
  for (size_t i = 0; i < count; i++) {
    value = value * 10 + (uint32_t)(digits[i] - '0');
  }
  prv_put_number(o, value);
  return p;
}

//! Two-letter words that are more often words than abbreviations in capitals
static bool prv_is_common_pair(const char *w) {
  static const char s_words[] = "ANASATBEBYDOGOHEIFINISITMEMYNOOFOHOKONORSOTOUPUSWE";
  for (const char *p = s_words; *p; p += 2) {
    if (p[0] == w[0] && p[1] == w[1]) {
      return true;
    }
  }
  return false;
}

//! A run of letters (with inner apostrophes). @return its end
static const char *prv_word(Out *o, const char *in) {
  const char *p = in;
  bool all_upper = true;
  bool has_vowel = false;
  while (prv_is_alpha(*p) || (*p == '\'' && prv_is_alpha(p[1]) && p > in)) {
    const char c = prv_upper(*p);
    if (prv_is_alpha(*p) && !prv_is_upper(*p)) {
      all_upper = false;
    }
    if (c == 'A' || c == 'E' || c == 'I' || c == 'O' || c == 'U' || c == 'Y') {
      has_vowel = true;
    }
    p++;
  }
  const size_t len = (size_t)(p - in);
  if (prv_is_digit(prv_last(o))) {
    prv_put(o, ' ');
  }
  if (all_upper && ((len >= 2 && len <= 4 && !has_vowel) ||
                    (len == 2 && !prv_is_common_pair(in)))) {
    // an abbreviation: spell it ("PM" -> "P M", "AM" -> "A M")
    for (const char *c = in; c < p; c++) {
      prv_put_word(o, (char[]){*c, '\0'});
    }
  } else {
    for (const char *c = in; c < p; c++) {
      prv_put(o, prv_upper(*c));
    }
  }
  return p;
}

static void prv_trim_space(Out *o) {
  while (o->len && o->buf[o->len - 1] == ' ') {
    o->len--;
  }
}

static bool prv_is_stop(char c) {
  return c == '.' || c == '?' || c == '!';
}

//! A pause: SAM's comma, unless a pause or the start is already there
static void prv_put_pause(Out *o) {
  prv_trim_space(o);
  if (o->len && prv_last(o) != ',' && !prv_is_stop(prv_last(o))) {
    prv_put(o, ',');
  }
  prv_put(o, ' ');
}

//! Skip one UTF-8 sequence, replacing the few common ones that matter. @return its end
static const char *prv_utf8(Out *o, const char *in) {
  const uint8_t *p = (const uint8_t *)in;
  if (p[0] == 0xE2 && p[1] == 0x80) {
    switch (p[2]) {
      case 0x98:  // left single quote
      case 0x99:  // right single quote / apostrophe
        if (prv_is_alpha(prv_last(o)) && prv_is_alpha((char)p[3])) {
          prv_put(o, '\'');
        } else {
          prv_put(o, ' ');
        }
        return in + 3;
      case 0x93:  // en dash
      case 0x94:  // em dash
        prv_put_pause(o);
        return in + 3;
      case 0xA6:  // ellipsis
        prv_put(o, '.');
        prv_put(o, ' ');
        return in + 3;
      default:
        break;
    }
  }
  p++;
  while ((*p & 0xC0) == 0x80) {
    p++;
  }
  prv_put(o, ' ');
  return (const char *)p;
}

static const char *prv_symbol_name(char c) {
  switch (c) {
    case '%':
      return "PERCENT";
    case '&':
      return "AND";
    case '+':
      return "PLUS";
    case '@':
      return "AT";
    case '#':
      return "NUMBER";
    case '=':
      return "EQUALS";
    default:
      return NULL;
  }
}

size_t tts_text_normalize(const char *in, char *out, size_t out_size) {
  Out o = {.buf = out, .size = out_size, .len = 0};
  if (out_size == 0) {
    return 0;
  }
  bool after_colon = false;  // a number follows "10:"
  bool after_point = false;  // a number follows "3."
  bool dollars = false;      // say "DOLLARS" after the next number
  while (*in) {
    const char c = *in;
    if (prv_is_digit(c)) {
      in = prv_number(&o, in, after_colon, after_point);
      if (dollars) {
        prv_put_word(&o, "DOLLARS");
        dollars = false;
      }
      after_colon = after_point = false;
      if (*in == ':' && prv_is_digit(in[1])) {
        after_colon = true;
        in++;
      } else if (*in == '.' && prv_is_digit(in[1])) {
        prv_put_word(&o, "POINT");
        after_point = true;
        in++;
      }
      continue;
    }
    after_colon = after_point = false;
    if (prv_is_alpha(c)) {
      in = prv_word(&o, in);
      continue;
    }
    if ((uint8_t)c >= 0x80) {
      in = prv_utf8(&o, in);
      continue;
    }
    const char *name = prv_symbol_name(c);
    if (name) {
      prv_put_word(&o, name);
    } else if (c == '$' && prv_is_digit(in[1])) {
      dollars = true;
    } else if (prv_is_stop(c) || c == '\n') {
      // SAM's sentence ends; "..." becomes one, and a line break ends a sentence
      prv_trim_space(&o);
      if (o.len && !prv_is_stop(prv_last(&o))) {
        if (prv_last(&o) == ',') {
          o.len--;
        }
        prv_put(&o, c == '\n' ? '.' : c);
      }
      prv_put(&o, ' ');
    } else if (c == ',' || c == ';' || c == ':' || c == '(' || c == ')' ||
               (c == '-' && prv_last(&o) == ' ' && (in[1] == ' ' || in[1] == '\0'))) {
      prv_put_pause(&o);
    } else {
      // dashes, quotes, brackets (SAM's '[' ends its input), control characters
      prv_put(&o, ' ');
    }
    in++;
  }
  while (o.len && (o.buf[o.len - 1] == ' ' || o.buf[o.len - 1] == ',')) {
    o.len--;
  }
  o.buf[o.len] = '\0';
  return o.len;
}

size_t tts_text_chunk_length(const char *text, size_t max_len) {
  const size_t len = strlen(text);
  if (len <= max_len) {
    return len;
  }
  for (size_t i = max_len; i > max_len / 2; i--) {
    const char c = text[i - 1];
    if ((c == '.' || c == '?' || c == '!') && text[i] == ' ') {
      return i;
    }
  }
  for (size_t i = max_len; i > 0; i--) {
    if (text[i] == ' ') {
      return i;
    }
  }
  return max_len;
}
