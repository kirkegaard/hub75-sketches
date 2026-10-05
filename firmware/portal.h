#ifndef HUB75_PORTAL_H
#define HUB75_PORTAL_H

#ifdef __cplusplus
extern "C" {
#endif

/* Start Protomatter and the shared RGB565 framebuffer.
   Returns 1 on success, 0 on failure. */
int portal_begin(int w, int h);

/* Copy the sketch framebuffer and refresh the panel. */
void portal_present(void);

#ifdef __cplusplus
}
#endif

#endif
