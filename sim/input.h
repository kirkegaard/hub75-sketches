#ifndef HUB75_INPUT_H
#define HUB75_INPUT_H

#include <SDL3/SDL.h>

struct Hub75Panel;

/* Everything the event handler needs from the window. The browser state
   lives in browser.c, the LED shape lives on the panel, pause is owned by
   the window loop, and `redraw` asks the loop to re-raster a paused frame. */
typedef struct Hub75Input {
  SDL_Window *window;
  SDL_Renderer *renderer;
  struct Hub75Panel *panel;
  int *paused;
  int *redraw;
  int view_w;
  int view_h;
} Hub75Input;

/* Matrix Portal S3 user buttons: 1 pressed, 0 released.
   The simulator maps the up and down arrow keys. */
int sim_button_up(void);
int sim_button_down(void);

/* Apply one SDL event. Returns 1 when the window should close. */
int input_handle_event(Hub75Input *input, const SDL_Event *event);

#endif
