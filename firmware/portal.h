#ifndef HUB75_PORTAL_H
#define HUB75_PORTAL_H

#ifdef __cplusplus
extern "C" {
#endif

/* Start Protomatter and the shared RGB565 framebuffer.
   Returns 1 on success, 0 on failure. */
int portal_begin(int w, int h);

/* Copy the sketch framebuffer and refresh the panel, dimmed to the
   current brightness. */
void portal_present(void);

/* The two user buttons: 1 pressed, 0 released. */
int portal_button_up(void);
int portal_button_down(void);

/* Step the panel brightness up or down by one notch. */
void portal_brightness_up(void);
void portal_brightness_down(void);

#ifdef __cplusplus
}
#endif

#endif
