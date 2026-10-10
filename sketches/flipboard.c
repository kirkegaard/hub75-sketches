#include "hub75.h"
#include "util.h"

#include <string.h>

/* One rule: a split-flap board. Every tile the sentence uses churns,
   then the letters settle one at a time in reading order, holding
   through a wrap from the end of one line to the start of the next.
   Then the board flips over into the next sentence.

   The glyph bitmaps are the CC0 "3x5 Microfont" by Ella Jameson
   (github.com/nimaid/microfont). One u16 per ASCII code, bit 14 is the
   top-left pixel and bits run left to right, top to bottom. */

static uint16_t bg;

enum { CELL_W = 4, CELL_H = 6, PAD = 2 };
enum { CHAR_STAGGER = 2, FLIP_FRAMES = 2, HOLD_FRAMES = 90 };
enum { MAX_COLS = 64, MAX_ROWS = 32 };

static const char *const sentences[] = {
    "LOREM IPSUM DOLOR SIT AMET, CONSECTETUR ADIPISCING ELIT. NULLAM QUIS "
    "LIBERO ULTRICES, FACILISIS AUGUE EU, MATTIS ENIM. UT AUCTOR LOREM SED "
    "BIBENDUM LAOREET.",
    "NOW BOARDING FLIGHT 815 TO TOKYO GATE 22",
    "THE QUICK BROWN FOX JUMPS OVER THE LAZY DOG",
    "DELAYED FLIGHT 402 TO BERLIN NEW GATE 7",
    "PACK MY BOX WITH FIVE DOZEN LIQUOR JUGS",
    "LAST CALL FLIGHT 119 TO LISBON DEPARTING",
    "HOW VEXINGLY QUICK DAFT ZEBRAS JUMP",
};

enum { SENTENCE_COUNT = (int)(sizeof(sentences) / sizeof(sentences[0])) };

static const uint16_t font[128] = {
    0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
    0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
    0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
    0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x2482, 0x5a00, 0x5f7d,
    0x3cfa, 0x52a5, 0x3ceb, 0x2400, 0x1491, 0x4494, 0x5540, 0x05d0, 0x0014,
    0x01c0, 0x0002, 0x12a4, 0x7b6f, 0x2c97, 0x62a7, 0x628e, 0x1779, 0x798e,
    0x79ef, 0x72a4, 0x7bef, 0x7bcf, 0x0410, 0x0414, 0x1511, 0x0e38, 0x4454,
    0x72c2, 0x2b63, 0x2bed, 0x6bae, 0x3923, 0x6b6e, 0x79a7, 0x79a4, 0x396b,
    0x5bed, 0x7497, 0x126a, 0x5bad, 0x4927, 0x5fed, 0x5ffd, 0x2b6a, 0x6ba4,
    0x3b73, 0x6bad, 0x388e, 0x7492, 0x5b6f, 0x5b6a, 0x5bfd, 0x5aad, 0x5a92,
    0x72a7, 0x3493, 0x4889, 0x6496, 0x2a00, 0x0007, 0x4400, 0x076b, 0x4d6e,
    0x0723, 0x176b, 0x0573, 0x15d2, 0x07ce, 0x49ad, 0x2092, 0x2094, 0x4bad,
    0x2491, 0x0bed, 0x0d6d, 0x056a, 0x0d74, 0x0759, 0x0564, 0x070e, 0x2691,
    0x0b6b, 0x0b6a, 0x0b7d, 0x0a95, 0x0aca, 0x0e67, 0x3513, 0x2492, 0x6456,
    0x00f0, 0x0000,
};

static const char glyphs[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
                             "abcdefghijklmnopqrstuvwxyz"
                             "0123456789"
                             ".,:;!?-+*/#%&@";

enum { GLYPH_COUNT = (int)(sizeof(glyphs) - 1) };

static unsigned char board[MAX_ROWS][MAX_COLS];
static int max_chars = 1;

static void measure(void) {
  int si;

  max_chars = 1;
  for (si = 0; si < SENTENCE_COUNT; si++) {
    const char *text = sentences[si];
    int count = 0;
    int i;

    for (i = 0; text[i]; i++) {
      if (text[i] != ' ') {
        count++;
      }
    }
    if (count > max_chars) {
      max_chars = count;
    }
  }
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

    while (pos < len && text[pos] == ' ') {
      pos++;
    }
    if (pos >= len) {
      break;
    }
    while (pos + n < len && n < cols) {
      if (text[pos + n] == ' ') {
        last_space = n;
      }
      line[n] = text[pos + n];
      n++;
    }
    if (pos + n < len && last_space >= 0) {
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

void setup(void) {
  bg = color(0, 0, 0);
  measure();
}

void draw(void) {
  int cols = (width - 2 * PAD) / CELL_W;
  int rows = (height - 2 * PAD) / CELL_H;
  int reveal;
  int cycle;
  int tick;
  int cycle_index;
  const char *text;
  int origin_x;
  int origin_y;
  int ord = 0;
  int col;
  int row;

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

  reveal = (max_chars - 1) * CHAR_STAGGER + 1;
  cycle = reveal + HOLD_FRAMES;
  tick = (int)((frameCount - 1) % (unsigned long)cycle);
  cycle_index = (int)((frameCount - 1) / (unsigned long)cycle);

  text = sentences[cycle_index % SENTENCE_COUNT];
  layout(text, cols, rows);

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
        bits = (f % 7u == 0u)
                   ? 0u
                   : font[(unsigned char)glyphs[f % (uint32_t)GLYPH_COUNT]];
        level = 120 + (int)((f >> 8) % 80u);
      } else {
        bits = font[board[row][col]];
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
}
