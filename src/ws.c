#include "ws.h"

#include "util/base64.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* A minimal WebSocket client, shared by the simulator and the board.
   It handles the handshake, framing, ping/pong and reconnection, and
   hands whole text messages to the sketch through ws_poll(). Only the
   transport (ws_io_*) differs per platform. */

enum { WS_MSG_CAP = 24576 };

static void *g_io = NULL;
static volatile WsStatus g_status = WS_OFF;
static int g_started = 0;

/* One-message handoff from the client task to the sketch. The task
   waits if the sketch has not taken the previous message, which keeps
   the socket from buffering without bound. */
static char g_slot[WS_MSG_CAP];
static int g_slot_len = 0;
static volatile int g_slot_full = 0;

/* Receive buffer for the client loop. Static, not on the task stack: it
   is larger than the task stack, and putting it there overflowed. */
static char g_rx[WS_MSG_CAP];

static volatile unsigned long g_msgs = 0;
static volatile unsigned long g_connects = 0;

/* ---- sha1 (only for the handshake) ---- */

typedef struct {
  uint32_t h[5];
  uint64_t len;
  uint8_t buf[64];
  int n;
} Sha1;

static uint32_t rol(uint32_t x, int n) { return (x << n) | (x >> (32 - n)); }

static void sha1_block(Sha1 *s, const uint8_t *p) {
  uint32_t w[80];
  uint32_t a, b, c, d, e;
  int i;

  for (i = 0; i < 16; i++) {
    w[i] = ((uint32_t)p[i * 4] << 24) | ((uint32_t)p[i * 4 + 1] << 16) |
           ((uint32_t)p[i * 4 + 2] << 8) | (uint32_t)p[i * 4 + 3];
  }
  for (i = 16; i < 80; i++) {
    w[i] = rol(w[i - 3] ^ w[i - 8] ^ w[i - 14] ^ w[i - 16], 1);
  }

  a = s->h[0];
  b = s->h[1];
  c = s->h[2];
  d = s->h[3];
  e = s->h[4];

  for (i = 0; i < 80; i++) {
    uint32_t f, k, t;
    if (i < 20) {
      f = (b & c) | (~b & d);
      k = 0x5A827999u;
    } else if (i < 40) {
      f = b ^ c ^ d;
      k = 0x6ED9EBA1u;
    } else if (i < 60) {
      f = (b & c) | (b & d) | (c & d);
      k = 0x8F1BBCDCu;
    } else {
      f = b ^ c ^ d;
      k = 0xCA62C1D6u;
    }
    t = rol(a, 5) + f + e + k + w[i];
    e = d;
    d = c;
    c = rol(b, 30);
    b = a;
    a = t;
  }

  s->h[0] += a;
  s->h[1] += b;
  s->h[2] += c;
  s->h[3] += d;
  s->h[4] += e;
}

static void sha1_init(Sha1 *s) {
  s->h[0] = 0x67452301u;
  s->h[1] = 0xEFCDAB89u;
  s->h[2] = 0x98BADCFEu;
  s->h[3] = 0x10325476u;
  s->h[4] = 0xC3D2E1F0u;
  s->len = 0;
  s->n = 0;
}

static void sha1_update(Sha1 *s, const uint8_t *p, size_t len) {
  s->len += len;
  while (len) {
    size_t take = (size_t)(64 - s->n);
    if (take > len) {
      take = len;
    }
    memcpy(s->buf + s->n, p, take);
    s->n += (int)take;
    p += take;
    len -= take;
    if (s->n == 64) {
      sha1_block(s, s->buf);
      s->n = 0;
    }
  }
}

static void sha1_final(Sha1 *s, uint8_t out[20]) {
  uint64_t bits = s->len * 8;
  uint8_t byte;
  int i;

  byte = 0x80;
  sha1_update(s, &byte, 1);
  byte = 0x00;
  while (s->n != 56) {
    sha1_update(s, &byte, 1);
  }
  for (i = 7; i >= 0; i--) {
    byte = (uint8_t)(bits >> (i * 8));
    sha1_update(s, &byte, 1);
  }
  for (i = 0; i < 5; i++) {
    out[i * 4] = (uint8_t)(s->h[i] >> 24);
    out[i * 4 + 1] = (uint8_t)(s->h[i] >> 16);
    out[i * 4 + 2] = (uint8_t)(s->h[i] >> 8);
    out[i * 4 + 3] = (uint8_t)(s->h[i]);
  }
}

/* ---- url ---- */

static int ws_parse_url(const char *url, char *host, int hostcap, int *port,
                        char *path, int pathcap, int *tls) {
  const char *p = url;
  const char *h;
  int n;

  *tls = 0;
  *port = 80;

  if (strncmp(p, "wss://", 6) == 0) {
    *tls = 1;
    *port = 443;
    p += 6;
  } else if (strncmp(p, "ws://", 5) == 0) {
    p += 5;
  } else {
    return 0;
  }

  h = p;
  while (*p && *p != '/' && *p != ':') {
    p++;
  }
  n = (int)(p - h);
  if (n <= 0 || n >= hostcap) {
    return 0;
  }
  memcpy(host, h, (size_t)n);
  host[n] = '\0';

  if (*p == ':') {
    *port = 0;
    p++;
    while (*p >= '0' && *p <= '9') {
      *port = *port * 10 + (*p - '0');
      p++;
    }
  }

  if (*p == '/') {
    snprintf(path, (size_t)pathcap, "%s", p);
  } else {
    snprintf(path, (size_t)pathcap, "/");
  }
  return 1;
}

/* ---- io helpers ---- */

static int io_read_full(void *io, uint8_t *buf, size_t len) {
  size_t got = 0;
  while (got < len) {
    int r = ws_io_read(io, buf + got, len - got, 15000);
    if (r <= 0) {
      return -1;
    }
    got += (size_t)r;
  }
  return 0;
}

static int ws_send_control(void *io, int opcode, const uint8_t *payload,
                           int len) {
  uint8_t frame[6 + 125];
  uint8_t mask[4];
  int i;

  frame[0] = (uint8_t)(0x80 | opcode);
  frame[1] = (uint8_t)(0x80 | (len & 0x7f));
  ws_io_random(mask, 4);
  memcpy(frame + 2, mask, 4);
  for (i = 0; i < len; i++) {
    frame[6 + i] = (uint8_t)(payload[i] ^ mask[i & 3]);
  }
  return ws_io_write(io, frame, (size_t)(6 + len));
}

/* ---- handshake ---- */

/* The server must echo base64(sha1(key + GUID)); check it when present. */
static int ws_accept_ok(const char *resp, const char *keyb64) {
  static const char GUID[] = "258EAFA5-E914-47DA-95CA-C5AB0DC85B11";
  char combined[64];
  char expect[32];
  uint8_t digest[20];
  Sha1 s;
  const char *p;

  snprintf(combined, sizeof combined, "%s%s", keyb64, GUID);
  sha1_init(&s);
  sha1_update(&s, (const uint8_t *)combined, strlen(combined));
  sha1_final(&s, digest);
  base64_encode(digest, sizeof digest, expect, sizeof expect);

  p = strstr(resp, "Sec-WebSocket-Accept:");
  if (!p) {
    return 1; /* header absent: stay lenient */
  }
  p += strlen("Sec-WebSocket-Accept:");
  while (*p == ' ') {
    p++;
  }
  return strncmp(p, expect, strlen(expect)) == 0;
}

static int ws_handshake(void *io, const char *host, int port, const char *path,
                        int tls) {
  uint8_t key[16];
  char keyb64[32];
  char hosthdr[160];
  char req[640];
  char resp[1024];
  int n = 0;
  int status = 0;
  const char *sp;

  ws_io_random(key, sizeof key);
  base64_encode(key, sizeof key, keyb64, sizeof keyb64);

  if (port == (tls ? 443 : 80)) {
    snprintf(hosthdr, sizeof hosthdr, "%s", host);
  } else {
    snprintf(hosthdr, sizeof hosthdr, "%s:%d", host, port);
  }

  snprintf(req, sizeof req,
           "GET %s HTTP/1.1\r\n"
           "Host: %s\r\n"
           "Upgrade: websocket\r\n"
           "Connection: Upgrade\r\n"
           "Sec-WebSocket-Key: %s\r\n"
           "Sec-WebSocket-Version: 13\r\n"
           "\r\n",
           path, hosthdr, keyb64);

  if (ws_io_write(io, req, strlen(req)) < 0) {
    return 0;
  }

  while (n < (int)sizeof resp - 1) {
    int r = ws_io_read(io, resp + n, 1, 5000);
    if (r <= 0) {
      return 0;
    }
    n += r;
    if (n >= 4 && memcmp(resp + n - 4, "\r\n\r\n", 4) == 0) {
      break;
    }
  }
  resp[n] = '\0';

  if (strncmp(resp, "HTTP/", 5) == 0) {
    sp = strchr(resp, ' ');
    if (sp) {
      status = atoi(sp + 1);
    }
  }
  return status == 101 && ws_accept_ok(resp, keyb64);
}

/* ---- frames ---- */

static int ws_read_message(void *io, char *buf, size_t cap) {
  uint8_t hdr[2];
  uint8_t mask[4];
  uint64_t len;
  int opcode;
  int masked;
  int i;

  if (io_read_full(io, hdr, 2) < 0) {
    return -1;
  }
  opcode = hdr[0] & 0x0f;
  masked = (hdr[1] & 0x80) != 0;
  len = hdr[1] & 0x7f;

  if (len == 126) {
    uint8_t e[2];
    if (io_read_full(io, e, 2) < 0) {
      return -1;
    }
    len = ((uint64_t)e[0] << 8) | e[1];
  } else if (len == 127) {
    uint8_t e[8];
    if (io_read_full(io, e, 8) < 0) {
      return -1;
    }
    len = 0;
    for (i = 0; i < 8; i++) {
      len = (len << 8) | e[i];
    }
  }

  if (masked && io_read_full(io, mask, 4) < 0) {
    return -1;
  }

  if (len > cap - 1) {
    uint8_t sink[256];
    uint64_t left = len;
    while (left) {
      size_t take = left < sizeof sink ? (size_t)left : sizeof sink;
      if (io_read_full(io, sink, take) < 0) {
        return -1;
      }
      left -= take;
    }
    return 0;
  }

  if (len && io_read_full(io, (uint8_t *)buf, (size_t)len) < 0) {
    return -1;
  }
  if (masked) {
    for (i = 0; i < (int)len; i++) {
      buf[i] = (char)(buf[i] ^ mask[i & 3]);
    }
  }
  buf[len] = '\0';

  if (opcode == 0x8) {
    return -1; /* close */
  }
  if (opcode == 0x9) {
    ws_send_control(io, 0xA, (const uint8_t *)buf, (int)len); /* ping -> pong */
    return 0;
  }
  if (opcode == 0xA) {
    return 0; /* pong */
  }
  if (opcode == 0x1 || opcode == 0x2) {
    return (int)len;
  }
  return 0;
}

/* ---- handoff ---- */

static void ws_enqueue(const char *msg, int len) {
  while (g_slot_full) {
    ws_io_delay(5);
  }
  if (len >= WS_MSG_CAP) {
    len = WS_MSG_CAP - 1;
  }
  memcpy(g_slot, msg, (size_t)len);
  g_slot[len] = '\0';
  g_slot_len = len;
  g_slot_full = 1;
  g_msgs++;
}

int ws_poll(char *buf, size_t cap) {
  int len;

  if (!g_slot_full) {
    return 0;
  }
  len = g_slot_len;
  if ((size_t)len >= cap) {
    len = (int)cap - 1;
  }
  memcpy(buf, g_slot, (size_t)len);
  buf[len] = '\0';
  g_slot_full = 0;
  return len;
}

int ws_send(const char *text) {
  void *io = g_io;
  uint8_t header[14];
  uint8_t mask[4];
  uint8_t *out;
  size_t len;
  size_t hlen;
  size_t i;
  int ok;

  if (!io || g_status != WS_OPEN) {
    return 0;
  }

  len = strlen(text);
  header[0] = 0x81;
  if (len < 126) {
    header[1] = (uint8_t)(0x80 | len);
    hlen = 2;
  } else if (len < 65536) {
    header[1] = 0x80 | 126;
    header[2] = (uint8_t)(len >> 8);
    header[3] = (uint8_t)(len & 0xff);
    hlen = 4;
  } else {
    header[1] = 0x80 | 127;
    for (i = 0; i < 8; i++) {
      header[2 + i] = (uint8_t)(len >> (56 - i * 8));
    }
    hlen = 10;
  }

  ws_io_random(mask, 4);
  memcpy(header + hlen, mask, 4);

  out = (uint8_t *)malloc(len ? len : 1);
  if (!out) {
    return 0;
  }
  for (i = 0; i < len; i++) {
    out[i] = (uint8_t)(text[i] ^ mask[i & 3]);
  }

  ok = ws_io_write(io, header, hlen + 4) >= 0 && ws_io_write(io, out, len) >= 0;
  free(out);
  return ok;
}

WsStatus ws_status(void) { return g_status; }

const char *ws_status_text(void) {
  switch (g_status) {
  case WS_OFF:
    return "OFF";
  case WS_CONNECTING:
    return "CONNECTING";
  case WS_OPEN:
    return "OPEN";
  case WS_CLOSED:
    return "CLOSED";
  default:
    return "?";
  }
}

unsigned long ws_messages(void) { return g_msgs; }
unsigned long ws_connects(void) { return g_connects; }

void ws_close(void) {
  g_started = 0;
  g_status = WS_OFF;
  g_slot_full = 0;
}

/* ---- loop ---- */

void ws_client_loop(void *arg) {
  const char *url = (const char *)arg;
  char host[128];
  char path[128];
  int port;
  int tls;

  for (;;) {
    if (!ws_parse_url(url, host, (int)sizeof host, &port, path,
                      (int)sizeof path, &tls)) {
      ws_io_log("ws: bad url");
    } else {
      void *io;

      g_status = WS_CONNECTING;
      ws_io_log("ws: connecting");
      io = ws_io_connect(host, port, tls);
      if (!io) {
        ws_io_log("ws: connect failed");
      } else if (ws_handshake(io, host, port, path, tls)) {
        g_io = io;
        g_status = WS_OPEN;
        g_connects++;
        ws_io_log("ws: open");

        for (;;) {
          int n = ws_read_message(io, g_rx, sizeof g_rx);
          if (n < 0) {
            break;
          }
          if (n > 0) {
            ws_enqueue(g_rx, n);
          }
        }

        g_io = NULL;
        ws_io_log("ws: closed by peer");
      } else {
        ws_io_log("ws: handshake failed");
      }
      if (io) {
        ws_io_close(io);
      }
    }
    g_status = WS_CLOSED;
    ws_io_delay(1000);
  }
}

void ws_open(const char *url) {
  static char stored[256];

  if (g_started) {
    return;
  }
  g_started = 1;
  snprintf(stored, sizeof stored, "%s", url);
  ws_task_create(ws_client_loop, stored);
}
