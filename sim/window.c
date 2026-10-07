#define SDL_MAIN_HANDLED
#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>

#include "browser.h"
#include "font5x7.h"
#include "graphics.h"
#include "hub75.h"
#include "input.h"
#include "panel.h"
#include "window.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* The simulator window. The LED panel is rasterized on the CPU into an
   RGB24 image (sim/panel.c) and blitted with SDL3's 2D renderer, the
   same as the original SDL2 window. Events live in sim/input.c. */

typedef struct View {
  SDL_Window *window;
  SDL_Renderer *renderer;
  SDL_Texture *texture;
  Hub75Panel *panel;
  uint8_t *rgb;
  int own_rgb;
  int paused;
  int redraw;
  int view_w;
  int view_h;
  int cell;
  int diameter;
} View;

static void render_frame(View *v) {
  hub75_leds_render(v->rgb, v->cell, v->diameter, v->panel->shape,
                    hub75_pixels(), v->panel->matrix_w, v->panel->matrix_h);
}

static void step_frame(View *v) {
  frameCount++;
  draw();
  render_frame(v);
}

static void fill_rect(SDL_Renderer *r, float x, float y, float w, float h,
                      Uint8 cr, Uint8 cg, Uint8 cb, Uint8 ca) {
  SDL_FRect rect;

  rect.x = x;
  rect.y = y;
  rect.w = w;
  rect.h = h;
  SDL_SetRenderDrawColor(r, cr, cg, cb, ca);
  SDL_RenderFillRect(r, &rect);
}

static void draw_text(SDL_Renderer *r, int x, int y, int scale,
                      const char *text, Uint8 cr, Uint8 cg, Uint8 cb) {
  int i;

  for (i = 0; text && text[i] != '\0'; i++) {
    const unsigned char *glyph = hub75_font_glyph(text[i]);
    int row;

    for (row = 0; row < HUB75_FONT_H; row++) {
      unsigned char bits = glyph[row];
      int col;

      for (col = 0; col < HUB75_FONT_W; col++) {
        if (bits & (1u << (HUB75_FONT_W - 1 - col))) {
          fill_rect(r, (float)(x + col * scale), (float)(y + row * scale),
                    (float)scale, (float)scale, cr, cg, cb, 255);
        }
      }
    }
    x += (HUB75_FONT_W + 1) * scale;
  }
}

static void draw_overlay(View *v) {
  BrowserLayout layout;
  int i;

  if (!browser_is_open()) {
    return;
  }

  browser_layout(v->view_w, v->view_h, &layout);
  if (!layout.visible) {
    return;
  }

  SDL_SetRenderDrawBlendMode(v->renderer, SDL_BLENDMODE_BLEND);
  fill_rect(v->renderer, (float)layout.x, (float)layout.y, (float)layout.w,
            (float)layout.h, 10, 10, 14, 235);
  fill_rect(v->renderer, (float)layout.x, (float)layout.y, (float)layout.w,
            (float)layout.title_h, 44, 44, 58, 255);
  draw_text(v->renderer, layout.x + layout.pad,
            layout.y + (layout.title_h - HUB75_FONT_H * layout.scale) / 2,
            layout.scale, "sketches", 235, 235, 245);

  for (i = 0; i < layout.rows; i++) {
    int index = layout.first_row + i;
    int row_y = layout.y + layout.title_h + i * layout.row_h;

    if (index >= layout.count) {
      break;
    }
    if (index == browser_selection()) {
      fill_rect(v->renderer, (float)layout.x, (float)row_y, (float)layout.w,
                (float)layout.row_h, 70, 70, 94, 255);
    }
    draw_text(v->renderer, layout.x + layout.pad,
              row_y + (layout.row_h - HUB75_FONT_H * layout.scale) / 2,
              layout.scale, hub75_sketches[index].name, 228, 228, 236);
  }
  SDL_SetRenderDrawBlendMode(v->renderer, SDL_BLENDMODE_NONE);
}

static void present(View *v) {
  SDL_UpdateTexture(v->texture, NULL, v->rgb, v->view_w * 3);
  /* SDL_RenderClear paints the letterbox bars too, so pin the clear
     color to black before the overlay can leave it on a light color. */
  SDL_SetRenderDrawColor(v->renderer, 0, 0, 0, 255);
  SDL_RenderClear(v->renderer);
  SDL_RenderTexture(v->renderer, v->texture, NULL, NULL);
  draw_overlay(v);
  SDL_RenderPresent(v->renderer);
}

static void refresh_title(View *v) {
  const Hub75Sketch *sketch = browser_current();

  if (sketch) {
    char buf[128];
    snprintf(buf, sizeof buf, "%s  %dx%d", sketch->name, v->panel->matrix_w,
             v->panel->matrix_h);
    SDL_SetWindowTitle(v->window, buf);
  }
}

void sim_window_run(const char *title, Hub75Panel *panel) {
  View v;
  Hub75Input input;
  SDL_Rect bounds;
  int max_w;
  int max_h;
  int fit;
  int running = 1;
  Uint64 next;

  if (!panel || !panel->rgb || panel->img_w < 1 || panel->img_h < 1) {
    return;
  }

  memset(&v, 0, sizeof v);
  v.panel = panel;

  SDL_SetMainReady();
  if (!SDL_Init(SDL_INIT_VIDEO)) {
    fprintf(stderr, "SDL_Init: %s\n", SDL_GetError());
    return;
  }

  if (!SDL_GetDisplayUsableBounds(SDL_GetPrimaryDisplay(), &bounds) ||
      bounds.w < 1 || bounds.h < 1) {
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

  /* Whole pixels per LED. The cell is the pitch, shrunk to whole pixels
     only when the panel is wider or taller than the screen. */
  v.cell = panel->pitch_px;
  fit = max_w / panel->matrix_w;
  if (max_h / panel->matrix_h < fit) {
    fit = max_h / panel->matrix_h;
  }
  if (fit < 1) {
    fit = 1;
  }
  if (fit < v.cell) {
    v.cell = fit;
  }

  v.view_w = panel->matrix_w * v.cell;
  v.view_h = panel->matrix_h * v.cell;
  v.diameter = hub75_led_diameter(v.cell);

  if (v.cell == panel->pitch_px && v.diameter == panel->led_px) {
    v.rgb = panel->rgb;
  } else {
    v.rgb = (uint8_t *)malloc((size_t)v.view_w * (size_t)v.view_h * 3u);
    if (!v.rgb) {
      SDL_Quit();
      return;
    }
    v.own_rgb = 1;
  }

  v.window = SDL_CreateWindow(title ? title : "hub75", v.view_w, v.view_h,
                              SDL_WINDOW_HIGH_PIXEL_DENSITY);
  if (!v.window) {
    fprintf(stderr, "SDL_CreateWindow: %s\n", SDL_GetError());
    if (v.own_rgb) {
      free(v.rgb);
    }
    SDL_Quit();
    return;
  }

  v.renderer = SDL_CreateRenderer(v.window, NULL);
  if (!v.renderer) {
    fprintf(stderr, "SDL_CreateRenderer: %s\n", SDL_GetError());
    SDL_DestroyWindow(v.window);
    if (v.own_rgb) {
      free(v.rgb);
    }
    SDL_Quit();
    return;
  }
  SDL_SetRenderVSync(v.renderer, 1);
  SDL_SetRenderLogicalPresentation(v.renderer, v.view_w, v.view_h,
                                   SDL_LOGICAL_PRESENTATION_LETTERBOX);

  v.texture =
      SDL_CreateTexture(v.renderer, SDL_PIXELFORMAT_RGB24,
                        SDL_TEXTUREACCESS_STREAMING, v.view_w, v.view_h);
  if (!v.texture) {
    fprintf(stderr, "SDL_CreateTexture: %s\n", SDL_GetError());
    SDL_DestroyRenderer(v.renderer);
    SDL_DestroyWindow(v.window);
    if (v.own_rgb) {
      free(v.rgb);
    }
    SDL_Quit();
    return;
  }
  SDL_SetTextureScaleMode(v.texture, SDL_SCALEMODE_NEAREST);

  input.window = v.window;
  input.renderer = v.renderer;
  input.panel = v.panel;
  input.paused = &v.paused;
  input.redraw = &v.redraw;
  input.view_w = v.view_w;
  input.view_h = v.view_h;

  step_frame(&v);
  present(&v);
  next = SDL_GetTicks() + 33;

  while (running) {
    SDL_Event event;
    Uint64 now;

    while (SDL_PollEvent(&event)) {
      if (input_handle_event(&input, &event)) {
        running = 0;
      }
    }

    /* A shape change while paused still needs a re-raster so the view
       reflects it; the sketch itself stays frozen. */
    if (v.redraw) {
      if (v.paused) {
        render_frame(&v);
      }
      v.redraw = 0;
    }

    refresh_title(&v);

    if (!running) {
      break;
    }

    now = SDL_GetTicks();

    if ((Sint64)(now - next) >= 0) {
      if (!v.paused) {
        step_frame(&v);
      }
      present(&v);
      next += 33;
      if ((Sint64)(now - next) >= 0) {
        next = now + 33;
      }
    } else {
      SDL_Delay(1);
    }
  }

  SDL_DestroyTexture(v.texture);
  SDL_DestroyRenderer(v.renderer);
  SDL_DestroyWindow(v.window);
  if (v.own_rgb) {
    free(v.rgb);
  }
  SDL_Quit();
}
