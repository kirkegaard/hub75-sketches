#include "image.h"

#include "graphics.h"
#include "hub75.h"
#include "util/dither.h"
#include "util/interp.h"

#include <math.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>

/* Only the formats the sketches need, so the decoder stays small. GIF is
   the first frame; stb_image does not expose the rest. */
#define STB_IMAGE_IMPLEMENTATION
#define STBI_ONLY_JPEG
#define STBI_ONLY_PNG
#define STBI_ONLY_GIF
#define STBI_ONLY_BMP
#include "stb_image.h"

static size_t pixel_offset(const Image *img, int x, int y) {
  return ((size_t)y * (size_t)img->w + (size_t)x) * 4u;
}

void image_init(Image *img) {
  if (!img) {
    return;
  }
  img->w = 0;
  img->h = 0;
  img->pixels = NULL;
}

static int adopt(Image *img, stbi_uc *rgba, int w, int h) {
  if (!rgba || w <= 0 || h <= 0) {
    stbi_image_free(rgba);
    return -1;
  }
  free(img->pixels);
  img->pixels = rgba;
  img->w = w;
  img->h = h;
  return 0;
}

int image_load(Image *img, const char *path) {
  int w = 0;
  int h = 0;
  int channels = 0;
  stbi_uc *rgba;

  if (!img || !path) {
    return -1;
  }
  rgba = stbi_load(path, &w, &h, &channels, 4);
  return adopt(img, rgba, w, h);
}

int image_load_memory(Image *img, const void *data, int len) {
  int w = 0;
  int h = 0;
  int channels = 0;
  stbi_uc *rgba;

  if (!img || !data || len <= 0) {
    return -1;
  }
  rgba = stbi_load_from_memory((const stbi_uc *)data, len, &w, &h, &channels, 4);
  return adopt(img, rgba, w, h);
}

void image_free(Image *img) {
  if (!img) {
    return;
  }
  free(img->pixels);
  img->pixels = NULL;
  img->w = 0;
  img->h = 0;
}

int image_copy(Image *dst, const Image *src) {
  uint8_t *pixels;
  size_t bytes;

  if (!dst || !src || !src->pixels || src->w <= 0 || src->h <= 0) {
    return -1;
  }
  if (dst == src) {
    return 0;
  }
  bytes = (size_t)src->w * (size_t)src->h * 4u;
  pixels = (uint8_t *)malloc(bytes);
  if (!pixels) {
    return -1;
  }
  memcpy(pixels, src->pixels, bytes);
  free(dst->pixels);
  dst->pixels = pixels;
  dst->w = src->w;
  dst->h = src->h;
  return 0;
}

uint8_t *image_pixels(Image *img) { return img ? img->pixels : NULL; }

/* --- resize --- */

static void resize_nearest(const Image *src, Image *dst) {
  int x;
  int y;

  for (y = 0; y < dst->h; y++) {
    int sy = (int)((long long)y * src->h / dst->h);

    for (x = 0; x < dst->w; x++) {
      int sx = (int)((long long)x * src->w / dst->w);
      memcpy(dst->pixels + pixel_offset(dst, x, y),
             src->pixels + pixel_offset(src, sx, sy), 4u);
    }
  }
}

static void resize_bilinear(const Image *src, Image *dst) {
  int x;
  int y;
  int c;

  for (y = 0; y < dst->h; y++) {
    float fy = ((float)y + 0.5f) * (float)src->h / (float)dst->h - 0.5f;
    int y0 = (int)floorf(fy);
    float ty = fy - (float)y0;

    if (y0 < 0) {
      y0 = 0;
      ty = 0.0f;
    }
    if (y0 >= src->h - 1) {
      y0 = src->h - 1;
      ty = 0.0f;
    }

    for (x = 0; x < dst->w; x++) {
      float fx = ((float)x + 0.5f) * (float)src->w / (float)dst->w - 0.5f;
      int x0 = (int)floorf(fx);
      float tx = fx - (float)x0;
      int y1 = y0 + 1 < src->h ? y0 + 1 : y0;
      int x1;
      const uint8_t *p00;
      const uint8_t *p10;
      const uint8_t *p01;
      const uint8_t *p11;
      uint8_t *out;

      if (x0 < 0) {
        x0 = 0;
        tx = 0.0f;
      }
      if (x0 >= src->w - 1) {
        x0 = src->w - 1;
        tx = 0.0f;
      }
      x1 = x0 + 1 < src->w ? x0 + 1 : x0;

      p00 = src->pixels + pixel_offset(src, x0, y0);
      p10 = src->pixels + pixel_offset(src, x1, y0);
      p01 = src->pixels + pixel_offset(src, x0, y1);
      p11 = src->pixels + pixel_offset(src, x1, y1);
      out = dst->pixels + pixel_offset(dst, x, y);

      for (c = 0; c < 4; c++) {
        float top = (float)p00[c] + ((float)p10[c] - (float)p00[c]) * tx;
        float bot = (float)p01[c] + ((float)p11[c] - (float)p01[c]) * tx;
        float value = top + (bot - top) * ty;

        out[c] = (uint8_t)clamp_byte((int)(value + 0.5f));
      }
    }
  }
}

/* Area average. The destination cell maps to a source rectangle, so a
   big photo loses its noise before it is quantized, not after. */
static void resize_box(const Image *src, Image *dst) {
  int x;
  int y;

  for (y = 0; y < dst->h; y++) {
    int sy0 = (int)((long long)y * src->h / dst->h);
    int sy1 = (int)((long long)(y + 1) * src->h / dst->h);

    if (sy1 <= sy0) {
      sy1 = sy0 + 1;
    }
    if (sy1 > src->h) {
      sy1 = src->h;
    }

    for (x = 0; x < dst->w; x++) {
      int sx0 = (int)((long long)x * src->w / dst->w);
      int sx1 = (int)((long long)(x + 1) * src->w / dst->w);
      long long sum[4] = {0, 0, 0, 0};
      long long count;
      uint8_t *out;
      int c;
      int yy;
      int xx;

      if (sx1 <= sx0) {
        sx1 = sx0 + 1;
      }
      if (sx1 > src->w) {
        sx1 = src->w;
      }
      count = (long long)(sx1 - sx0) * (long long)(sy1 - sy0);

      for (yy = sy0; yy < sy1; yy++) {
        const uint8_t *row = src->pixels + pixel_offset(src, sx0, yy);

        for (xx = sx0; xx < sx1; xx++) {
          sum[0] += row[0];
          sum[1] += row[1];
          sum[2] += row[2];
          sum[3] += row[3];
          row += 4;
        }
      }

      out = dst->pixels + pixel_offset(dst, x, y);
      for (c = 0; c < 4; c++) {
        out[c] = (uint8_t)((sum[c] + count / 2) / count);
      }
    }
  }
}

int image_resize(Image *img, int w, int h, ImageResample how) {
  Image out;

  if (!img || !img->pixels || w <= 0 || h <= 0) {
    return -1;
  }
  if (w == img->w && h == img->h) {
    return 0;
  }

  memset(&out, 0, sizeof out);
  out.w = w;
  out.h = h;
  out.pixels = (uint8_t *)calloc((size_t)w * (size_t)h, 4u);
  if (!out.pixels) {
    return -1;
  }

  if (how == IMAGE_NEAREST) {
    resize_nearest(img, &out);
  } else if (how == IMAGE_BOX) {
    resize_box(img, &out);
  } else {
    resize_bilinear(img, &out);
  }

  free(img->pixels);
  *img = out;
  return 0;
}

static int crop_center(Image *img, int w, int h) {
  Image out;
  int ox;
  int oy;
  int x;
  int y;

  if (img->w < w || img->h < h) {
    return -1;
  }
  if (img->w == w && img->h == h) {
    return 0;
  }

  memset(&out, 0, sizeof out);
  out.w = w;
  out.h = h;
  out.pixels = (uint8_t *)malloc((size_t)w * (size_t)h * 4u);
  if (!out.pixels) {
    return -1;
  }

  ox = (img->w - w) / 2;
  oy = (img->h - h) / 2;
  for (y = 0; y < h; y++) {
    for (x = 0; x < w; x++) {
      memcpy(out.pixels + pixel_offset(&out, x, y),
             img->pixels + pixel_offset(img, x + ox, y + oy), 4u);
    }
  }

  free(img->pixels);
  *img = out;
  return 0;
}

static int pad_center(Image *img, int w, int h) {
  Image out;
  int ox;
  int oy;
  int x;
  int y;
  int i;

  if (img->w > w || img->h > h) {
    return -1;
  }
  if (img->w == w && img->h == h) {
    return 0;
  }

  memset(&out, 0, sizeof out);
  out.w = w;
  out.h = h;
  out.pixels = (uint8_t *)malloc((size_t)w * (size_t)h * 4u);
  if (!out.pixels) {
    return -1;
  }

  /* Opaque black bars, not transparent ones: CONTAIN is a finished
     frame, and the panel starts dark anyway. */
  memset(out.pixels, 0, (size_t)w * (size_t)h * 4u);
  for (i = 0; i < w * h; i++) {
    out.pixels[(size_t)i * 4u + 3u] = 255;
  }

  ox = (w - img->w) / 2;
  oy = (h - img->h) / 2;
  for (y = 0; y < img->h; y++) {
    for (x = 0; x < img->w; x++) {
      memcpy(out.pixels + pixel_offset(&out, x + ox, y + oy),
             img->pixels + pixel_offset(img, x, y), 4u);
    }
  }

  free(img->pixels);
  *img = out;
  return 0;
}

int image_fit(Image *img, int w, int h, ImageFit fit, ImageResample how) {
  float scale;
  int fit_w;
  int fit_h;

  if (!img || !img->pixels || w <= 0 || h <= 0) {
    return -1;
  }

  if (fit == IMAGE_STRETCH) {
    return image_resize(img, w, h, how);
  }

  if (fit == IMAGE_COVER) {
    float sx = (float)w / (float)img->w;
    float sy = (float)h / (float)img->h;

    scale = sx > sy ? sx : sy;
    fit_w = (int)ceilf((float)img->w * scale);
    fit_h = (int)ceilf((float)img->h * scale);
    if (fit_w < w) {
      fit_w = w;
    }
    if (fit_h < h) {
      fit_h = h;
    }
    if (image_resize(img, fit_w, fit_h, how) != 0) {
      return -1;
    }
    return crop_center(img, w, h);
  }

  {
    float sx = (float)w / (float)img->w;
    float sy = (float)h / (float)img->h;

    scale = sx < sy ? sx : sy;
    fit_w = (int)floorf((float)img->w * scale);
    fit_h = (int)floorf((float)img->h * scale);
    if (fit_w < 1) {
      fit_w = 1;
    }
    if (fit_h < 1) {
      fit_h = 1;
    }
    if (fit_w > w) {
      fit_w = w;
    }
    if (fit_h > h) {
      fit_h = h;
    }
    if (image_resize(img, fit_w, fit_h, how) != 0) {
      return -1;
    }
    return pad_center(img, w, h);
  }
}

/* --- filters --- */

static int luminance(const uint8_t *p) {
  return (p[0] * 77 + p[1] * 150 + p[2] * 29 + 128) >> 8;
}

static int level_to_byte(int level, int levels) {
  int max = levels - 1;

  if (max <= 0) {
    return 0;
  }
  return (level * 255 + max / 2) / max;
}

static int quantize_nearest(int v, int levels) {
  int max;

  if (levels < 2) {
    levels = 2;
  }
  max = levels - 1;
  v = clamp_int(v, 0, 255);
  return clamp_int((v * max + 127) / 255, 0, max);
}

static void filter_grayscale(Image *img) {
  int i;
  int n = img->w * img->h;

  for (i = 0; i < n; i++) {
    uint8_t *p = img->pixels + (size_t)i * 4u;
    int y = luminance(p);

    p[0] = (uint8_t)y;
    p[1] = (uint8_t)y;
    p[2] = (uint8_t)y;
  }
}

static void filter_invert(Image *img) {
  int i;
  int n = img->w * img->h;

  for (i = 0; i < n; i++) {
    uint8_t *p = img->pixels + (size_t)i * 4u;

    p[0] = (uint8_t)(255 - p[0]);
    p[1] = (uint8_t)(255 - p[1]);
    p[2] = (uint8_t)(255 - p[2]);
  }
}

static void filter_threshold(Image *img, int level) {
  int i;
  int n = img->w * img->h;

  level = clamp_int(level, 0, 255);
  for (i = 0; i < n; i++) {
    uint8_t *p = img->pixels + (size_t)i * 4u;
    int v = luminance(p) >= level ? 255 : 0;

    p[0] = (uint8_t)v;
    p[1] = (uint8_t)v;
    p[2] = (uint8_t)v;
  }
}

static void filter_brightness(Image *img, int amount) {
  int i;
  int n = img->w * img->h;

  for (i = 0; i < n; i++) {
    uint8_t *p = img->pixels + (size_t)i * 4u;

    p[0] = (uint8_t)clamp_byte(p[0] + amount);
    p[1] = (uint8_t)clamp_byte(p[1] + amount);
    p[2] = (uint8_t)clamp_byte(p[2] + amount);
  }
}

static void filter_contrast(Image *img, int amount) {
  int i;
  int n = img->w * img->h;
  float factor = (100.0f + (float)clamp_int(amount, -100, 100)) / 100.0f;

  for (i = 0; i < n; i++) {
    uint8_t *p = img->pixels + (size_t)i * 4u;
    int c;

    for (c = 0; c < 3; c++) {
      float v = ((float)p[c] - 128.0f) * factor + 128.0f;

      p[c] = (uint8_t)clamp_byte((int)lroundf(v));
    }
  }
}

static void filter_posterize(Image *img, int levels) {
  int i;
  int n = img->w * img->h;
  int c;

  levels = clamp_int(levels, 2, 256);
  for (i = 0; i < n; i++) {
    uint8_t *p = img->pixels + (size_t)i * 4u;

    for (c = 0; c < 3; c++) {
      p[c] = (uint8_t)level_to_byte(quantize_nearest(p[c], levels), levels);
    }
  }
}

static void unpack565(uint16_t color, int *r, int *g, int *b) {
  *r = ((color >> 11) & 0x1Fu) * 255 / 31;
  *g = ((color >> 5) & 0x3Fu) * 255 / 63;
  *b = (color & 0x1Fu) * 255 / 31;
}

static void filter_tint(Image *img, uint16_t color, int amount) {
  int i;
  int n = img->w * img->h;
  int tr;
  int tg;
  int tb;
  int a = clamp_int(amount, 0, 255);

  unpack565(color, &tr, &tg, &tb);
  for (i = 0; i < n; i++) {
    uint8_t *p = img->pixels + (size_t)i * 4u;

    p[0] = (uint8_t)((p[0] * (255 - a) + tr * a) / 255);
    p[1] = (uint8_t)((p[1] * (255 - a) + tg * a) / 255);
    p[2] = (uint8_t)((p[2] * (255 - a) + tb * a) / 255);
  }
}

/* Map luminance between two palette colours. A one-bit dither before
   this gives a clean two-colour palette, e.g. Game Boy green. */
static void filter_duotone(Image *img, uint16_t dark, uint16_t light) {
  int i;
  int n = img->w * img->h;
  int dr;
  int dg;
  int db;
  int lr;
  int lg;
  int lb;

  unpack565(dark, &dr, &dg, &db);
  unpack565(light, &lr, &lg, &lb);
  for (i = 0; i < n; i++) {
    uint8_t *p = img->pixels + (size_t)i * 4u;
    int t = luminance(p);

    p[0] = (uint8_t)(dr + (lr - dr) * t / 255);
    p[1] = (uint8_t)(dg + (lg - dg) * t / 255);
    p[2] = (uint8_t)(db + (lb - db) * t / 255);
  }
}

/* --- dithering --- */

static int bayer2(int x, int y) {
  static const uint8_t matrix[4] = {0, 2, 3, 1};

  return matrix[((y & 1) << 1) | (x & 1)];
}

static int bayer8(int x, int y) {
  static const uint8_t matrix[64] = {
      0,  32, 8,  40, 2,  34, 10, 42, 48, 16, 56, 24, 50, 18, 58, 26,
      12, 44, 4,  36, 14, 46, 6,  38, 60, 28, 52, 20, 62, 30, 54, 22,
      3,  35, 11, 43, 1,  33, 9,  41, 51, 19, 59, 27, 49, 17, 57, 25,
      15, 47, 7,  39, 13, 45, 5,  37, 63, 31, 55, 23, 61, 29, 53, 21};

  return matrix[((y & 7) << 3) | (x & 7)];
}

/* Clustered-dot matrix: the low thresholds sit together, so the dither
   becomes a grid of growing dots instead of Bayer's dispersed points. */
static int halftone4(int x, int y) {
  static const uint8_t matrix[16] = {12, 5, 6, 13, 4, 0, 1, 7,
                                     11, 3, 2, 8,  15, 10, 9, 14};

  return matrix[((y & 3) << 2) | (x & 3)];
}

/* A 16x16 void-and-cluster mask. It is random-looking but has no low
   frequencies, so it avoids both Bayer's visible grid and white noise's
   clumps. Generated for this repo; any mask with the same properties
   would do. */
static int blue_noise16(int x, int y) {
  static const uint8_t matrix[256] = {
      220, 111, 197, 29,  126, 150, 183, 103, 135, 223, 110, 11,  48,  200,
      19,  85,  67,  170, 146, 75,  231, 16,  84,  233, 8,   78,  60,  230,
      139, 92,  251, 137, 192, 46,  1,   213, 106, 191, 57,  159, 202, 143,
      172, 118, 186, 63,  163, 35,  122, 244, 95,  175, 136, 41,  242, 120,
      24,  252, 52,  31,  237, 4,   112, 226, 80,  157, 58,  227, 26,  166,
      208, 74,  107, 181, 96,  216, 81,  169, 205, 25,  210, 12,  187, 114,
      69,  94,  6,   142, 38,  206, 10,  153, 131, 42,  99,  145, 235, 130,
      36,  253, 152, 222, 180, 246, 161, 129, 240, 68,  199, 249, 178, 54,
      73,  182, 88,  198, 14,  125, 50,  101, 59,  86,  21,  113, 33,  90,
      7,   116, 20,  164, 45,  140, 65,  204, 28,  228, 185, 211, 171, 229,
      188, 160, 225, 196, 248, 97,  214, 239, 108, 82,  156, 134, 0,   43,
      144, 61,  127, 47,  79,  147, 66,  124, 2,   167, 37,  217, 176, 250,
      76,  109, 236, 89,  22,  212, 133, 32,  201, 158, 56,  189, 119, 15,
      91,  51,  123, 190, 13,  151, 254, 102, 173, 232, 23,  104, 245, 77,
      224, 149, 234, 27,  207, 162, 221, 64,  193, 3,   53,  87,  179, 218,
      18,  132, 44,  100, 194, 71,  138, 98,  40,  128, 168, 115, 209, 141,
      121, 72,  154, 203, 174, 5,   117, 165, 247, 17,  83,  243, 30,  70,
      238, 39,  9,   241, 49,  93,  255, 62,  219, 34,  55,  195, 177, 148,
      215, 105, 155, 184};

  return matrix[((y & 15) << 4) | (x & 15)];
}

static int ordered_threshold(DitherAlgorithm algo, int x, int y,
                             int *thresholds) {
  if (algo == DITHER_BAYER_2) {
    *thresholds = 4;
    return bayer2(x, y);
  }
  if (algo == DITHER_BAYER_8) {
    *thresholds = 64;
    return bayer8(x, y);
  }
  if (algo == DITHER_HALFTONE_4) {
    *thresholds = 16;
    return halftone4(x, y);
  }
  if (algo == DITHER_BLUE_NOISE_16) {
    *thresholds = 256;
    return blue_noise16(x, y);
  }
  *thresholds = 16;
  return dither_threshold4(x, y);
}

static int is_ordered(DitherAlgorithm algo) {
  return algo == DITHER_BAYER_2 || algo == DITHER_BAYER_4 ||
         algo == DITHER_BAYER_8 || algo == DITHER_HALFTONE_4 ||
         algo == DITHER_BLUE_NOISE_16;
}

static void dither_ordered(Image *img, DitherAlgorithm algo, int levels,
                           int mono) {
  int x;
  int y;

  for (y = 0; y < img->h; y++) {
    for (x = 0; x < img->w; x++) {
      uint8_t *p = img->pixels + pixel_offset(img, x, y);
      int thresholds = 16;
      int t = ordered_threshold(algo, x, y, &thresholds);

      if (mono) {
        int nlevels = levels > 1 ? levels : 2;
        int v = dither_levels_ex(luminance(p), nlevels, t, thresholds);
        int gray = level_to_byte(v, nlevels);

        p[0] = (uint8_t)gray;
        p[1] = (uint8_t)gray;
        p[2] = (uint8_t)gray;
      } else {
        static const int bits[3] = {5, 6, 5};
        int c;

        for (c = 0; c < 3; c++) {
          int nlevels = levels > 1 ? levels : (1 << bits[c]);
          int v = dither_levels_ex(p[c], nlevels, t, thresholds);

          p[c] = (uint8_t)level_to_byte(v, nlevels);
        }
      }
    }
  }
}

typedef struct DiffusionTerm {
  int dx;
  int dy;
  int weight;
} DiffusionTerm;

typedef struct DiffusionKernel {
  const DiffusionTerm *terms;
  int count;
  int divisor;
} DiffusionKernel;

static const DiffusionTerm kFloydTerms[] = {
    {1, 0, 7}, {-1, 1, 3}, {0, 1, 5}, {1, 1, 1}};

static const DiffusionTerm kAtkinsonTerms[] = {
    {1, 0, 1}, {2, 0, 1}, {-1, 1, 1}, {0, 1, 1}, {1, 1, 1}, {0, 2, 1}};

static const DiffusionTerm kStuckiTerms[] = {
    {1, 0, 8},  {2, 0, 4},  {-2, 1, 2}, {-1, 1, 4}, {0, 1, 8},
    {1, 1, 4},  {2, 1, 2},  {-2, 2, 1}, {-1, 2, 2}, {0, 2, 4},
    {1, 2, 2},  {2, 2, 1}};

static const DiffusionTerm kBurkesTerms[] = {
    {1, 0, 8}, {2, 0, 4}, {-2, 1, 2}, {-1, 1, 4},
    {0, 1, 8}, {1, 1, 4}, {2, 1, 2}};

static const DiffusionTerm kJarvisTerms[] = {
    {1, 0, 7},  {2, 0, 5},  {-2, 1, 3}, {-1, 1, 5}, {0, 1, 7},
    {1, 1, 5},  {2, 1, 3},  {-2, 2, 1}, {-1, 2, 3}, {0, 2, 5},
    {1, 2, 3},  {2, 2, 1}};

static const DiffusionTerm kSierraLiteTerms[] = {
    {1, 0, 2}, {-1, 1, 1}, {0, 1, 1}};

static const DiffusionKernel kFloyd = {kFloydTerms, 4, 16};
static const DiffusionKernel kAtkinson = {kAtkinsonTerms, 6, 8};
static const DiffusionKernel kStucki = {kStuckiTerms, 12, 42};
static const DiffusionKernel kBurkes = {kBurkesTerms, 7, 32};
static const DiffusionKernel kJarvis = {kJarvisTerms, 12, 48};
static const DiffusionKernel kSierraLite = {kSierraLiteTerms, 3, 4};

static const DiffusionKernel *diffusion_kernel(DitherAlgorithm algo) {
  switch (algo) {
  case DITHER_ATKINSON:
    return &kAtkinson;
  case DITHER_STUCKI:
    return &kStucki;
  case DITHER_BURKES:
    return &kBurkes;
  case DITHER_JARVIS:
    return &kJarvis;
  case DITHER_SIERRA_LITE:
    return &kSierraLite;
  case DITHER_FLOYD_STEINBERG:
  default:
    return &kFloyd;
  }
}

static void spread_error(int32_t *work, int w, int h, int x, int y, int c,
                         int err, const DiffusionKernel *kernel) {
  int i;

  for (i = 0; i < kernel->count; i++) {
    const DiffusionTerm *term = &kernel->terms[i];
    int nx = x + term->dx;
    int ny = y + term->dy;

    if ((unsigned)nx >= (unsigned)w || (unsigned)ny >= (unsigned)h) {
      continue;
    }
    work[((size_t)ny * (size_t)w + (size_t)nx) * 3u + (size_t)c] +=
        err * term->weight / kernel->divisor;
  }
}

static void dither_diffuse_color(Image *img, const DiffusionKernel *kernel,
                                 int levels) {
  size_t n = (size_t)img->w * (size_t)img->h;
  int32_t *work = (int32_t *)malloc(n * 3u * sizeof(int32_t));
  int x;
  int y;
  int c;

  if (!work) {
    dither_ordered(img, DITHER_BAYER_4, levels, 0);
    return;
  }

  for (size_t i = 0; i < n; i++) {
    const uint8_t *p = img->pixels + i * 4u;

    work[i * 3u + 0u] = p[0];
    work[i * 3u + 1u] = p[1];
    work[i * 3u + 2u] = p[2];
  }

  for (y = 0; y < img->h; y++) {
    for (x = 0; x < img->w; x++) {
      size_t index = (size_t)y * (size_t)img->w + (size_t)x;
      uint8_t *p = img->pixels + index * 4u;

      for (c = 0; c < 3; c++) {
        int nlevels = levels > 1 ? levels : (c == 1 ? 64 : 32);
        int v = clamp_int(work[index * 3u + (size_t)c], 0, 255);
        int level = quantize_nearest(v, nlevels);
        int q = level_to_byte(level, nlevels);
        int err = v - q;

        p[c] = (uint8_t)q;
        spread_error(work, img->w, img->h, x, y, c, err, kernel);
      }
    }
  }

  free(work);
}

static void dither_diffuse_mono(Image *img, const DiffusionKernel *kernel,
                                int levels) {
  size_t n = (size_t)img->w * (size_t)img->h;
  int32_t *work = (int32_t *)malloc(n * 3u * sizeof(int32_t));
  int x;
  int y;

  if (!work) {
    dither_ordered(img, DITHER_BAYER_4, levels, 1);
    return;
  }

  for (size_t i = 0; i < n; i++) {
    work[i * 3u] = luminance(img->pixels + i * 4u);
  }

  for (y = 0; y < img->h; y++) {
    for (x = 0; x < img->w; x++) {
      size_t index = (size_t)y * (size_t)img->w + (size_t)x;
      uint8_t *p = img->pixels + index * 4u;
      int v = clamp_int(work[index * 3u], 0, 255);
      int level = quantize_nearest(v, levels);
      int q = level_to_byte(level, levels);
      int err = v - q;

      p[0] = (uint8_t)q;
      p[1] = (uint8_t)q;
      p[2] = (uint8_t)q;
      spread_error(work, img->w, img->h, x, y, 0, err, kernel);
    }
  }

  free(work);
}

static void filter_dither(Image *img, FilterOptions options) {
  if (options.mono) {
    int levels = options.levels > 1 ? options.levels : 2;

    if (is_ordered(options.dither)) {
      dither_ordered(img, options.dither, levels, 1);
    } else {
      dither_diffuse_mono(img, diffusion_kernel(options.dither), levels);
    }
  } else {
    if (is_ordered(options.dither)) {
      dither_ordered(img, options.dither, options.levels, 0);
    } else {
      dither_diffuse_color(img, diffusion_kernel(options.dither),
                           options.levels);
    }
  }
}

void filter(Image *img, FilterOptions options) {
  if (!img || !img->pixels || img->w <= 0 || img->h <= 0) {
    return;
  }

  switch (options.kind) {
  case FILTER_GRAYSCALE:
    filter_grayscale(img);
    break;
  case FILTER_INVERT:
    filter_invert(img);
    break;
  case FILTER_THRESHOLD:
    filter_threshold(img, options.amount);
    break;
  case FILTER_BRIGHTNESS:
    filter_brightness(img, options.amount);
    break;
  case FILTER_CONTRAST:
    filter_contrast(img, options.amount);
    break;
  case FILTER_POSTERIZE:
    filter_posterize(img, options.levels);
    break;
  case FILTER_TINT:
    filter_tint(img, options.color, options.amount);
    break;
  case FILTER_DUOTONE:
    filter_duotone(img, options.color, options.color2);
    break;
  case FILTER_DITHER:
    filter_dither(img, options);
    break;
  }
}

void image_grayscale(Image *img) {
  filter(img, (FilterOptions){.kind = FILTER_GRAYSCALE});
}

void image_invert(Image *img) {
  filter(img, (FilterOptions){.kind = FILTER_INVERT});
}

void image_threshold(Image *img, int level) {
  filter(img, (FilterOptions){.kind = FILTER_THRESHOLD, .amount = level});
}

void image_brightness(Image *img, int amount) {
  filter(img, (FilterOptions){.kind = FILTER_BRIGHTNESS, .amount = amount});
}

void image_contrast(Image *img, int amount) {
  filter(img, (FilterOptions){.kind = FILTER_CONTRAST, .amount = amount});
}

void image_posterize(Image *img, int levels) {
  filter(img, (FilterOptions){.kind = FILTER_POSTERIZE, .levels = levels});
}

void image_tint(Image *img, uint16_t color, int amount) {
  FilterOptions options = {
      .kind = FILTER_TINT, .color = color, .amount = amount};

  filter(img, options);
}

void image_duotone(Image *img, uint16_t dark, uint16_t light) {
  FilterOptions options = {
      .kind = FILTER_DUOTONE, .color = dark, .color2 = light};

  filter(img, options);
}

void image_dither(Image *img, DitherAlgorithm algorithm, int levels,
                  int mono) {
  FilterOptions options = {.kind = FILTER_DITHER,
                           .dither = algorithm,
                           .levels = levels,
                           .mono = mono};

  filter(img, options);
}

/* --- draw --- */

static uint16_t blend565(uint16_t dst, const uint8_t *src, int alpha) {
  int dr;
  int dg;
  int db;
  int r;
  int g;
  int b;

  unpack565(dst, &dr, &dg, &db);
  r = (src[0] * alpha + dr * (255 - alpha)) / 255;
  g = (src[1] * alpha + dg * (255 - alpha)) / 255;
  b = (src[2] * alpha + db * (255 - alpha)) / 255;

  return color((uint8_t)r, (uint8_t)g, (uint8_t)b);
}

void image_draw(const Image *img, int x, int y) {
  uint16_t *fb = hub75_pixels();
  int ix;
  int iy;

  if (!img || !img->pixels || !fb) {
    return;
  }

  for (iy = 0; iy < img->h; iy++) {
    int py = y + iy;

    if ((unsigned)py >= (unsigned)height) {
      continue;
    }
    for (ix = 0; ix < img->w; ix++) {
      int px = x + ix;
      const uint8_t *p;
      uint16_t *dst;

      if ((unsigned)px >= (unsigned)width) {
        continue;
      }
      p = img->pixels + pixel_offset(img, ix, iy);
      dst = fb + (size_t)py * (size_t)width + (size_t)px;

      if (p[3] == 255) {
        *dst = color(p[0], p[1], p[2]);
      } else if (p[3] != 0) {
        *dst = blend565(*dst, p, p[3]);
      }
    }
  }
}
