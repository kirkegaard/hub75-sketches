#ifndef HUB75_TRANSITION_H
#define HUB75_TRANSITION_H

#include <stdint.h>

/* Animates the framebuffer from one sketch's last frame to the next
   sketch's first frame. Every lit pixel (non-black RGB565) in each frame
   becomes a dot. Both dot lists are sorted in reading order and paired
   index-wise, so each dot slides to its counterpart along an axis-aligned
   L path (a horizontal leg and a vertical leg, never a diagonal, longer
   leg first) while the colors blend; surplus dots fade out (source) or
   fade in (destination).
   The whole morph is eased over the timeline.

   The module owns no timing. The caller drives it one frame at a time:
   begin(), then step() until it returns 0. It knows nothing about SDL,
   so the firmware path can reuse it later. */

/* Ease curve for the morph. Defaults to smoothstep, as the original
   playlist morph used. Pass NULL to restore the default. */
typedef float (*Hub75TransitionEase)(float);

/* Begin morphing src into dst. Gathers the lit dots from both frames, so
   the caller may reuse the buffers right away. Returns 0 when there is
   nothing to land on (no lit destinations) or on allocation failure, in
   which case the caller should switch instantly. */
int transition_begin(const uint16_t *src, const uint16_t *dst, int w, int h);

/* 1 while an animation is in flight. */
int transition_active(void);

/* Advance one animation frame, writing the intermediate image into fb.
   Returns 1 while still running, 0 on the frame that lands on dst. */
int transition_step(uint16_t *fb);

/* Abandon the running animation and free its scratch buffers. */
void transition_cancel(void);

/* Off skips animation: begin() returns 0 and the caller switches
   instantly. On by default. */
void transition_set_enabled(int on);
int transition_enabled(void);

/* Swap the ease curve used for the morph. */
void transition_set_ease(Hub75TransitionEase ease);

#endif
