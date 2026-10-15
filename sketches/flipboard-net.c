#include "hub75.h"
#include "net.h"
#include "util.h"
#include "util/font3x5.h"
#include "yyjson.h"

#include <stdio.h>
#include <string.h>

/* A split-flap board that shows the last.fm "now playing" track. It is
   flipboard.c with one difference: the board only churns when a fetch
   brings back a different track. Between changes it sits still.

   It fetches on an interval, not every frame, because a blocking fetch
   on the board can hold the panel for a second or two. A result is a
   new track when its artist, title, album, or playing flag differs from
   the one already on the board, so the flip does not retrigger on a
   refetch of the same song.

   The credentials (WiFi and the last.fm user and API key) live in a
   gitignored secrets.h; see secrets.example.h. */

static uint16_t bg;

enum { CELL_W = 4, CELL_H = 6, PAD = 2 };
enum { CHAR_STAGGER = 2, FLIP_FRAMES = 2 };
enum { MAX_COLS = 64, MAX_ROWS = 32 };
enum { REFRESH_MS = 15000, TEXT_CAP = 512 };

enum { GLYPH_COUNT = (int)(sizeof(font3x5_glyphs) - 1) };

static unsigned char board[MAX_ROWS][MAX_COLS];
static char display[TEXT_CAP];
static char last_key[TEXT_CAP];
static int have_data = 0;
static int restart = 0;
static unsigned long anim_start = 0;
static unsigned long last_fetch_ms = 0;
static NetResponse g_res;

/* Fold external text to the font's ASCII: uppercase, collapse runs of
   space, and drop the bytes of a UTF-8 sequence the 3x5 table cannot
   show. A byte above 127 would index past font3x5[128], so nothing
   non-ASCII is allowed through. */
static void fold_ascii(const char *in, char *out, size_t cap) {
  size_t n = 0;
  int pending_space = 0;

  for (; *in; in++) {
    unsigned char c = (unsigned char)*in;

    if (c >= 0x80 && c < 0xC0) {
      continue;
    }
    if (c >= 0x80) {
      c = '?';
    }
    if (c >= 'a' && c <= 'z') {
      c = (unsigned char)(c - 'a' + 'A');
    }
    if (c == '\n') {
      /* Keep explicit line breaks; layout() turns them into new rows. */
      if (n + 1 < cap) {
        out[n++] = '\n';
      }
      pending_space = 0;
      continue;
    }
    if (c < 32 || c == 127) {
      c = ' ';
    }

    if (c == ' ') {
      if (n == 0) {
        continue;
      }
      pending_space = 1;
      continue;
    }

    if (pending_space) {
      if (n + 1 < cap) {
        out[n++] = ' ';
      }
      pending_space = 0;
    }
    if (n + 1 < cap) {
      out[n++] = (char)c;
    }
  }
  out[n] = '\0';
}

/* Set the board text, retriggering the flip only when it actually
   changes, so a steady status does not churn. */
static void set_text(const char *raw) {
  char folded[TEXT_CAP];

  fold_ascii(raw, folded, sizeof folded);
  if (strcmp(folded, display) != 0) {
    snprintf(display, sizeof display, "%s", folded);
    restart = 1;
  }
}

/* The string at a JSON Pointer path (RFC 6901), or NULL. The pointer is
   slash-separated and an array index is just a segment, so last.fm's
   ".recenttracks.track[0].artist.#text" reads as
   "/recenttracks/track/0/artist/#text". */
static const char *json_str(yyjson_val *root, const char *path) {
  return yyjson_get_str(yyjson_ptr_get(root, path));
}

static void build_url(char *buf, size_t cap) {
  snprintf(buf, cap,
           "https://ws.audioscrobbler.com/2.0/?method=user.getRecentTracks"
           "&user=%s&api_key=%s&format=json&limit=1",
           HUB75_LASTFM_USER, HUB75_LASTFM_TOKEN);
}

/* Parse a completed response and, if it carries a different track, put it
   on the board. A failure only reaches the panel before the first result,
   so a blip does not wipe out the current track. */
static void handle_response(NetResponse *res) {
  char artist[128] = "";
  char track[128] = "";
  char album[128] = "";
  char key[TEXT_CAP];
  char raw[TEXT_CAP];
  int nowplaying = 0;
  yyjson_doc *doc;
  yyjson_val *root;

  if (!res->ok || !res->body) {
    if (!have_data) {
      if (res->status > 0) {
        snprintf(raw, sizeof raw, "HTTP %d", res->status);
      } else {
        snprintf(raw, sizeof raw, "FETCH FAILED");
      }
      set_text(raw);
    }
    return;
  }

  doc = yyjson_read(res->body, (size_t)res->len, 0);
  if (!doc) {
    if (!have_data) {
      set_text("BAD JSON");
    }
    return;
  }

  root = yyjson_doc_get_root(doc);
  if (root) {
    const char *s;

    s = json_str(root, "/recenttracks/track/0/artist/#text");
    if (s) {
      snprintf(artist, sizeof artist, "%s", s);
    }
    s = json_str(root, "/recenttracks/track/0/name");
    if (s) {
      snprintf(track, sizeof track, "%s", s);
    }
    s = json_str(root, "/recenttracks/track/0/album/#text");
    if (s) {
      snprintf(album, sizeof album, "%s", s);
    }
    s = json_str(root, "/recenttracks/track/0/@attr/nowplaying");
    if (s && strcmp(s, "true") == 0) {
      nowplaying = 1;
    }
  }
  yyjson_doc_free(doc);

  if (artist[0] == '\0' && track[0] == '\0') {
    if (!have_data) {
      set_text("NO DATA");
    }
    return;
  }

  snprintf(key, sizeof key, "%d|%s|%s|%s", nowplaying, artist, track, album);
  if (have_data && strcmp(key, last_key) == 0) {
    return;
  }
  snprintf(last_key, sizeof last_key, "%s", key);
  have_data = 1;

  snprintf(raw, sizeof raw, "%s:\n%s\n%s",
           nowplaying ? "NOW PLAYING" : "LAST PLAYED", artist, track);
  set_text(raw);
}

static int layout(const char *text, int cols, int rows) {
  char line[MAX_COLS + 1];
  int len = (int)strlen(text);
  int pos = 0;
  int used = 0;
  int row;
  int col;

  for (row = 0; row < rows; row++) {
    for (col = 0; col < cols; col++) {
      board[row][col] = ' ';
    }
  }

  while (pos < len && used < rows) {
    int n = 0;
    int last_space = -1;
    int i;
    int forced;

    while (pos < len && text[pos] == ' ') {
      pos++;
    }
    if (pos >= len) {
      break;
    }
    if (text[pos] == '\n') {
      pos++;
      continue;
    }
    while (pos + n < len && n < cols && text[pos + n] != '\n') {
      if (text[pos + n] == ' ') {
        last_space = n;
      }
      line[n] = text[pos + n];
      n++;
    }

    /* A newline ends the row without wrapping back to the last space. */
    forced = (pos + n < len && text[pos + n] == '\n');
    if (!forced && pos + n < len && last_space >= 0) {
      n = last_space;
      pos += last_space + 1;
    } else {
      pos += n;
    }

    for (i = 0; i < n; i++) {
      board[used][i] = (unsigned char)line[i];
    }
    used++;
  }
  return used;
}

/* Advance the network state machine. Runs in draw() so the panel keeps
   refreshing while the link is up and a fetch is in flight. */
static void update(void) {
  NetStatus st = net_status();
  char line[TEXT_CAP];

  if (st != NET_READY) {
    /* Before the first result, show what the link is doing. After that,
       keep the track up through a blip instead of blanking the panel. */
    if (!have_data) {
      const char *ip = net_ip_text();
      if (ip[0]) {
        snprintf(line, sizeof line, "%s %s", net_status_text(), ip);
      } else {
        snprintf(line, sizeof line, "%s", net_status_text());
      }
      set_text(line);
    }
    return;
  }

  if (fetch_poll(&g_res)) {
    handle_response(&g_res);
    net_free(&g_res);
  }

  if (!fetch_busy() && (unsigned long)(millis() - last_fetch_ms) >= REFRESH_MS) {
    char url[256];

    last_fetch_ms = millis();
    build_url(url, sizeof url);
    fetch_start(url);
  }
}

void setup(void) {
  bg = color(0, 0, 0);
  net_begin();
  set_text(net_status_text());
  anim_start = 0;
  last_fetch_ms = millis() - REFRESH_MS;
}

void draw(void) {
  int cols = (width - 2 * PAD) / CELL_W;
  int rows = (height - 2 * PAD) / CELL_H;
  int tick;
  int origin_x;
  int origin_y;
  int ord = 0;
  int col;
  int row;

  update();

  if (restart) {
    anim_start = frameCount;
    restart = 0;
  }

  if (cols < 1) {
    cols = 1;
  }
  if (rows < 1) {
    rows = 1;
  }
  if (cols > MAX_COLS) {
    cols = MAX_COLS;
  }
  if (rows > MAX_ROWS) {
    rows = MAX_ROWS;
  }

  tick = (int)(frameCount - anim_start);

  layout(display, cols, rows);

  origin_x = PAD + (width - 2 * PAD - (cols * CELL_W - 1)) / 2;
  origin_y = PAD;

  background(bg);

  for (row = 0; row < rows; row++) {
    for (col = 0; col < cols; col++) {
      uint32_t seed = hash_mix(hash_combine2(col, row));
      int settle;
      uint16_t bits;
      int level;
      int gx = origin_x + col * CELL_W;
      int gy = origin_y + row * CELL_H;
      int r;
      int c;

      if (board[row][col] == ' ') {
        continue;
      }
      settle = ord * CHAR_STAGGER;
      ord++;

      if (tick < settle) {
        uint32_t f =
            hash_mix(seed ^ ((uint32_t)tick / FLIP_FRAMES) * 2654435761u);
        bits =
            (f % 7u == 0u)
                ? 0u
                : font3x5[(
                      unsigned char)font3x5_glyphs[f % (uint32_t)GLYPH_COUNT]];
        level = 120 + (int)((f >> 8) % 80u);
      } else {
        bits = font3x5[board[row][col]];
        level = 255;
      }

      stroke(color((uint8_t)level, (uint8_t)level, (uint8_t)level));
      for (r = 0; r < 5; r++) {
        for (c = 0; c < 3; c++) {
          if (bits & (1u << (14 - (r * 3 + c)))) {
            point(gx + c, gy + r);
          }
        }
      }
    }
  }

  /* A 3x3 spinner in the bottom-right corner while a fetch is running.
     The fetch happens on its own task, so this keeps turning. */
  if (fetch_busy()) {
    static const signed char sx[8] = {0, 1, 2, 2, 2, 1, 0, 0};
    static const signed char sy[8] = {0, 0, 0, 1, 2, 2, 2, 1};
    int phase = (int)((frameCount / 3u) % 8u);
    int bx = width - 4;
    int by = height - 4;

    stroke(color(255, 255, 255));
    point(bx + sx[phase], by + sy[phase]);
  }
}
