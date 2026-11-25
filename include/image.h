#ifndef HUB75_IMAGE_H
#define HUB75_IMAGE_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Decoded images are RGBA8888, row-major: r, g, b, a per pixel. A photo
   has far more levels than the panel, so the last step is usually a
   dither (FILTER_DITHER), not a plain color() conversion. */

typedef struct Image {
  int w;
  int h;
  uint8_t *pixels;
} Image;

/* How a resize samples the source. BOX averages every source pixel that
   falls in a destination cell, which is what a large photo needs when
   shrinking to 128x64. BILINEAR is the smooth choice for small changes.
   NEAREST keeps hard pixel edges. */
typedef enum ImageResample {
  IMAGE_NEAREST = 0,
  IMAGE_BILINEAR,
  IMAGE_BOX
} ImageResample;

/* STRETCH ignores the aspect ratio. COVER fills the target and crops
   the overflow. CONTAIN fits inside and fills the rest with black. */
typedef enum ImageFit {
  IMAGE_STRETCH = 0,
  IMAGE_COVER,
  IMAGE_CONTAIN
} ImageFit;

/* Ordered patterns use a fixed threshold matrix; error diffusion pushes
   the quantization error into neighbors. Bayer tiles cleanly, blue noise
   hides the pattern, halftone makes clustered print dots, and the
   diffusion kernels trade sharpness for texture. */
typedef enum DitherAlgorithm {
  DITHER_BAYER_2 = 0,
  DITHER_BAYER_4,
  DITHER_BAYER_8,
  DITHER_HALFTONE_4,
  DITHER_BLUE_NOISE_16,
  DITHER_FLOYD_STEINBERG,
  DITHER_ATKINSON,
  DITHER_STUCKI,
  DITHER_BURKES,
  DITHER_JARVIS,
  DITHER_SIERRA_LITE
} DitherAlgorithm;

typedef enum FilterKind {
  FILTER_GRAYSCALE = 0,
  FILTER_INVERT,
  FILTER_THRESHOLD,
  FILTER_BRIGHTNESS,
  FILTER_CONTRAST,
  FILTER_POSTERIZE,
  FILTER_TINT,
  FILTER_DUOTONE,
  FILTER_DITHER
} FilterKind;

/* One options struct for every filter, so a call reads like Processing's
   filter('dither', {options}). The fields each kind actually reads:
     FILTER_GRAYSCALE   (none)
     FILTER_INVERT      (none)
     FILTER_THRESHOLD   amount (0..255)
     FILTER_BRIGHTNESS  amount (-255..255)
     FILTER_CONTRAST    amount (-100..100)
     FILTER_POSTERIZE   levels (2..256)
     FILTER_TINT        color (RGB565), amount (0..255)
     FILTER_DUOTONE     color (dark), color2 (light), both RGB565
     FILTER_DITHER      dither, levels (0 = panel 5/6/5), mono
   Unused fields are ignored, so designated initializers can leave them
   at zero. A mono dither quantizes luminance; levels then counts gray
   steps (2 = one bit). A color dither quantizes each channel. A mono
   dither followed by FILTER_DUOTONE gives a two-colour palette, the
   Game Boy look.

   The named functions below are the same filters with signatures that
   show exactly which options apply. */
typedef struct FilterOptions {
  FilterKind kind;
  DitherAlgorithm dither;
  int levels;
  int mono;
  int amount;
  uint16_t color;
  uint16_t color2;
} FilterOptions;

/* Start with a zeroed Image (static storage already is), or call this
   before the first load. image_free leaves the struct in this state. */
void image_init(Image *img);

/* Decode JPEG, PNG, GIF (first frame), or BMP. Returns 0 on success.
   image_load reads a file; image_load_memory takes the encoded bytes,
   which is the form firmware can use for an embedded asset. */
int image_load(Image *img, const char *path);
int image_load_memory(Image *img, const void *data, int len);

void image_free(Image *img);
int image_copy(Image *dst, const Image *src);

/* Resize in place. BOX only differs when shrinking. */
int image_resize(Image *img, int w, int h, ImageResample how);

/* Resize and crop or pad to exactly w x h. */
int image_fit(Image *img, int w, int h, ImageFit fit, ImageResample how);

/* Direct RGBA access, the loadPixels() of this API. */
uint8_t *image_pixels(Image *img);

/* Apply a filter in place. */
void filter(Image *img, FilterOptions options);

/* Named filter entry points. These are thin wrappers over filter(), but
   the parameters make the available options obvious and type-checked. */
void image_grayscale(Image *img);
void image_invert(Image *img);
void image_threshold(Image *img, int level);
void image_brightness(Image *img, int amount);
void image_contrast(Image *img, int amount);
void image_posterize(Image *img, int levels);
void image_tint(Image *img, uint16_t color, int amount);
void image_duotone(Image *img, uint16_t dark, uint16_t light);
void image_dither(Image *img, DitherAlgorithm algorithm, int levels, int mono);

/* Blit to the panel framebuffer at (x, y), clipping to the panel. */
void image_draw(const Image *img, int x, int y);

#ifdef __cplusplus
}
#endif

#endif
