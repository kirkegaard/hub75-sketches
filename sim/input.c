#include "input.h"

#include "browser.h"
#include "hub75.h"
#include "panel.h"

#include <stdio.h>

static int g_up = 0;
static int g_down = 0;

int sim_button_up(void) { return g_up; }

int sim_button_down(void) { return g_down; }

static void toggle_fullscreen(SDL_Window *window) {
  bool fullscreen = (SDL_GetWindowFlags(window) & SDL_WINDOW_FULLSCREEN) != 0;

  if (!SDL_SetWindowFullscreen(window, !fullscreen)) {
    fprintf(stderr, "SDL_SetWindowFullscreen: %s\n", SDL_GetError());
  }
}

/* Loading a sketch starts it fresh, so leave pause behind. */
static void unpause(Hub75Input *input) {
  if (input->paused) {
    *input->paused = 0;
  }
}

int input_handle_event(Hub75Input *input, const SDL_Event *event) {
  if (event->type == SDL_EVENT_QUIT) {
    return 1;
  }

  if (event->type == SDL_EVENT_KEY_DOWN && !event->key.repeat) {
    if (event->key.key == SDLK_S) {
      input->panel->shape = hub75_led_shape_next(input->panel->shape);
      if (input->redraw) {
        *input->redraw = 1;
      }
      fprintf(stderr, "led shape: %s\n",
              hub75_led_shape_name(input->panel->shape));
      return 0;
    }
    if (event->key.key == SDLK_B) {
      browser_toggle();
      return 0;
    }
    if (event->key.key == SDLK_P) {
      if (input->paused) {
        *input->paused = !*input->paused;
        fprintf(stderr, "%s\n", *input->paused ? "paused" : "resumed");
      }
      return 0;
    }
    if (event->key.key == SDLK_LEFT) {
      browser_step(-1);
      unpause(input);
      return 0;
    }
    if (event->key.key == SDLK_RIGHT) {
      browser_step(1);
      unpause(input);
      return 0;
    }
    if (browser_is_open()) {
      if (event->key.key == SDLK_UP) {
        browser_move(-1);
        return 0;
      }
      if (event->key.key == SDLK_DOWN) {
        browser_move(1);
        return 0;
      }
      if (event->key.key == SDLK_RETURN) {
        browser_commit();
        unpause(input);
        return 0;
      }
    }
    if (event->key.key == SDLK_UP) {
      g_up = 1;
    } else if (event->key.key == SDLK_DOWN) {
      g_down = 1;
    } else if (event->key.key == SDLK_F) {
      toggle_fullscreen(input->window);
    } else if (event->key.key == SDLK_ESCAPE || event->key.key == SDLK_Q) {
      return 1;
    }
  } else if (event->type == SDL_EVENT_KEY_UP) {
    if (event->key.key == SDLK_UP) {
      g_up = 0;
    } else if (event->key.key == SDLK_DOWN) {
      g_down = 0;
    }
  } else if (event->type == SDL_EVENT_MOUSE_BUTTON_DOWN &&
             event->button.button == SDL_BUTTON_LEFT && browser_is_open()) {
    BrowserLayout layout;
    float lx = 0.0f;
    float ly = 0.0f;
    int row;

    browser_layout(input->view_w, input->view_h, &layout);
    SDL_RenderCoordinatesFromWindow(input->renderer, event->button.x,
                                    event->button.y, &lx, &ly);
    row = browser_row_at(&layout, (int)lx, (int)ly);
    if (row >= 0) {
      browser_set_selection(row);
      browser_commit();
      unpause(input);
    }
  } else if (event->type == SDL_EVENT_MOUSE_WHEEL && browser_is_open()) {
    browser_move(event->wheel.y > 0.0f ? -1 : 1);
  }

  return 0;
}
