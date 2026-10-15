#ifndef HUB75_UTIL_BASE64_H
#define HUB75_UTIL_BASE64_H

#include <stddef.h>
#include <stdint.h>

/* Base64 (RFC 4648). Both directions return the number of bytes/chars
   written, or -1 if the output buffer is too small or the input is not
   valid base64. */

static inline int b64_value(int c) {
  if (c >= 'A' && c <= 'Z') return c - 'A';
  if (c >= 'a' && c <= 'z') return c - 'a' + 26;
  if (c >= '0' && c <= '9') return c - '0' + 52;
  if (c == '+') return 62;
  if (c == '/') return 63;
  return -1;
}

static inline int base64_decode(const char *in, size_t len, uint8_t *out,
                                size_t cap) {
  unsigned buf = 0;
  int bits = 0;
  size_t i;
  size_t o = 0;

  for (i = 0; i < len; i++) {
    int c = (unsigned char)in[i];
    int v;
    if (c == '=' || c == '\n' || c == '\r' || c == ' ' || c == '\t') {
      continue;
    }
    v = b64_value(c);
    if (v < 0) {
      return -1;
    }
    buf = (buf << 6) | (unsigned)v;
    bits += 6;
    if (bits >= 8) {
      bits -= 8;
      if (o >= cap) {
        return -1;
      }
      out[o++] = (uint8_t)((buf >> bits) & 0xff);
    }
  }
  return (int)o;
}

static inline int base64_encode(const uint8_t *in, size_t len, char *out,
                                size_t cap) {
  static const char tab[] =
      "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
  size_t i;
  size_t o = 0;

  for (i = 0; i + 2 < len; i += 3) {
    unsigned v = ((unsigned)in[i] << 16) | ((unsigned)in[i + 1] << 8) | in[i + 2];
    if (o + 4 >= cap) {
      return -1;
    }
    out[o++] = tab[(v >> 18) & 63];
    out[o++] = tab[(v >> 12) & 63];
    out[o++] = tab[(v >> 6) & 63];
    out[o++] = tab[v & 63];
  }
  if (i < len) {
    unsigned v = (unsigned)in[i] << 16;
    size_t rem = len - i;
    if (rem == 2) {
      v |= (unsigned)in[i + 1] << 8;
    }
    if (o + 4 >= cap) {
      return -1;
    }
    out[o++] = tab[(v >> 18) & 63];
    out[o++] = tab[(v >> 12) & 63];
    out[o++] = (rem == 2) ? tab[(v >> 6) & 63] : '=';
    out[o++] = '=';
  }
  out[o] = '\0';
  return (int)o;
}

#endif
