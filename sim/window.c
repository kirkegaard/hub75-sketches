#include "SDL_render.h"
#define SDL_MAIN_HANDLED
#include <SDL.h>

#include "graphics.h"
#include "hub75.h"
#include "panel.h"
#include "window.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

static int g_up = 0;
static int g_down = 0;

int sim_button_up(void) { return g_up; }

int sim_button_down(void) { return g_down; }

static void step_frame(uint8_t *rgb, int cell, int diameter, int matrix_w,
                       int matrix_h) {
  frameCount++;
  draw();
  hub75_leds_render(rgb, cell, diameter, hub75_pixels(), matrix_w, matrix_h);
}

static void present(SDL_Renderer *renderer, SDL_Texture *texture, uint8_t *rgb,
                    int img_w) {
  SDL_UpdateTexture(texture, NULL, rgb, img_w * 3);
  SDL_RenderClear(renderer);
  SDL_RenderCopy(renderer, texture, NULL, NULL);
  SDL_RenderPresent(renderer);
}

static int handle_event(const SDL_Event *event) {
  if (event->type == SDL_QUIT) {
    return 1;
  }

  if (event->type == SDL_KEYDOWN && !event->key.repeat) {
    if (event->key.keysym.sym == SDLK_UP) {
      g_up = 1;
    } else if (event->key.keysym.sym == SDLK_DOWN) {
      g_down = 1;
    } else if (event->key.keysym.sym == SDLK_ESCAPE ||
               event->key.keysym.sym == SDLK_q) {
      return 1;
    }
  } else if (event->type == SDL_KEYUP) {
    if (event->key.keysym.sym == SDLK_UP) {
      g_up = 0;
    } else if (event->key.keysym.sym == SDLK_DOWN) {
      g_down = 0;
    }
  }

  return 0;
}

void sim_window_run(const char *title, Hub75Panel *panel) {
  SDL_Window *window = NULL;
  SDL_Renderer *renderer = NULL;
  SDL_Texture *texture = NULL;
  SDL_Rect bounds;
  int max_w;
  int max_h;
  int cell;
  int fit;
  int diameter;
  int view_w;
  int view_h;
  uint8_t *view = NULL;
  int own_view = 0;
  int running = 1;
  Uint32 next;

  if (!panel || !panel->rgb || panel->img_w < 1 || panel->img_h < 1) {
    return;
  }

  SDL_SetMainReady();
  /* Nearest pixel sampling. Set before the renderer exists; the Metal
     backend on macOS ignores the hint and needs the texture scale mode
     below as well. */
  SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "nearest");
  if (SDL_Init(SDL_INIT_VIDEO) != 0) {
    fprintf(stderr, "SDL_Init: %s\n", SDL_GetError());
    return;
  }

  if (SDL_GetDisplayUsableBounds(0, &bounds) != 0 || bounds.w < 1 ||
      bounds.h < 1) {
    bounds.w = 1440;
    bounds.h = 900;
  }
  max_w = bounds.w * 92 / 100;
  max_h = bounds.h * 92 / 100;

  if (max_w < 1) {
    max_w = 1;
  }
  if (max_h < 1) {
    max_h = 1;
  }

  /* Whole pixels per LED. A fractional scale drops a different column
     from each disk, so neighboring circles look cut off. */
  cell = panel->pitch_px;
  fit = max_w / panel->matrix_w;

  if (max_h / panel->matrix_h < fit) {
    fit = max_h / panel->matrix_h;
  }

  if (fit < 1) {
    fit = 1;
  }

  if (fit < cell) {
    cell = fit;
  }

  view_w = panel->matrix_w * cell;
  view_h = panel->matrix_h * cell;
  diameter = hub75_led_diameter(cell);

  if (cell == panel->pitch_px && diameter == panel->led_px) {
    view = panel->rgb;
  } else {
    view = (uint8_t *)malloc((size_t)view_w * (size_t)view_h * 3u);
    if (!view) {
      SDL_Quit();
      return;
    }
    own_view = 1;
  }

  /* ALLOW_HIGHDPI draws into the real pixel buffer. Without it the
     window server stretches a 1x image onto a retina screen and the
     LED disks go soft. */
  window = SDL_CreateWindow(title ? title : "hub75", SDL_WINDOWPOS_CENTERED,
                            SDL_WINDOWPOS_CENTERED, view_w, view_h,
                            SDL_WINDOW_SHOWN | SDL_WINDOW_ALLOW_HIGHDPI);
  if (!window) {
    fprintf(stderr, "SDL_CreateWindow: %s\n", SDL_GetError());

    if (own_view) {
      free(view);
    }

    SDL_Quit();

    return;
  }

  renderer = SDL_CreateRenderer(
      window, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
  if (!renderer) {
    renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_SOFTWARE);
  }

  if (!renderer) {
    fprintf(stderr, "SDL_CreateRenderer: %s\n", SDL_GetError());

    SDL_DestroyWindow(window);

    if (own_view) {
      free(view);
    }

    SDL_Quit();

    return;
  }

  texture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_RGB24,
                              SDL_TEXTUREACCESS_STREAMING, view_w, view_h);
  if (!texture) {
    fprintf(stderr, "SDL_CreateTexture: %s\n", SDL_GetError());

    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);

    if (own_view) {
      free(view);
    }

    SDL_Quit();

    return;
  }

  if (SDL_SetTextureScaleMode(texture, SDL_ScaleModeNearest) != 0) {
    fprintf(stderr, "SDL_SetTextureScaleMode: %s\n", SDL_GetError());
  }

  step_frame(view, cell, diameter, panel->matrix_w, panel->matrix_h);
  present(renderer, texture, view, view_w);
  next = SDL_GetTicks() + 33;

  while (running) {
    SDL_Event event;
    Uint32 now;

    while (SDL_PollEvent(&event)) {
      if (handle_event(&event)) {
        running = 0;
      }
    }

    if (!running) {
      break;
    }

    now = SDL_GetTicks();

    if ((Sint32)(now - next) >= 0) {
      step_frame(view, cell, diameter, panel->matrix_w, panel->matrix_h);
      present(renderer, texture, view, view_w);
      next += 33;
      if ((Sint32)(now - next) >= 0) {
        next = now + 33;
      }
    } else {
      SDL_Delay(1);
    }
  }

  g_up = 0;
  g_down = 0;

  if (own_view) {
    free(view);
  }

  SDL_DestroyTexture(texture);
  SDL_DestroyRenderer(renderer);
  SDL_DestroyWindow(window);
  SDL_Quit();
}
