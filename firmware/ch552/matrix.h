#ifndef HUB75_MATRIX_H
#define HUB75_MATRIX_H

#ifdef __cplusplus
extern "C" {
#endif

/* Start the WS2812 matrix and the shared RGB565 framebuffer.
   Returns 1 on success, 0 on failure. */
int matrix_begin(int w, int h);

/* Convert the sketch framebuffer (RGB565) to WS2812 GRB bytes and push. */
void matrix_present(void);

#ifdef __cplusplus
}
#endif

#endif
