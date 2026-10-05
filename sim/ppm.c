#include "ppm.h"

#include <stdio.h>

int ppm_write(const char *path, const uint8_t *rgb, int w, int h) {
  FILE *fp;
  size_t n;
  size_t wrote;

  if (!path || !rgb || w <= 0 || h <= 0) {
    return -1;
  }
  fp = fopen(path, "wb");
  if (!fp) {
    return -1;
  }
  if (fprintf(fp, "P6\n%d %d\n255\n", w, h) < 0) {
    fclose(fp);
    return -1;
  }
  n = (size_t)w * (size_t)h * 3u;
  wrote = fwrite(rgb, 1, n, fp);
  if (fclose(fp) != 0 || wrote != n) {
    return -1;
  }
  return 0;
}
