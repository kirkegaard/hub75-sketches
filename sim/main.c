#include "graphics.h"
#include "hub75.h"
#include "panel.h"
#include "ppm.h"
#include "runtime.h"
#include "window.h"

#include "browser.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifndef HUB75_SKETCH_NAME
#define HUB75_SKETCH_NAME "sketch"
#endif

static void usage(FILE *fp) {
  fputs("usage: hub75-sim [options]\n"
        "  --width N     LED columns (default 128)\n"
        "  --height N    LED rows (default 64)\n"
        "  --pitch NAME  P1, P2.5, or P4 (default P1)\n"
        "  --dump FILE   write the last frame as a PPM and exit\n"
        "  --frames N    frames to draw before --dump (default 1)\n"
        "  --help        show this text\n",
        fp);
}

static int need_int(int argc, char **argv, int *i, int *out) {
  char *end = NULL;
  long value;

  if (*i + 1 >= argc) {
    fprintf(stderr, "missing value for %s\n", argv[*i]);
    return -1;
  }
  value = strtol(argv[++(*i)], &end, 10);
  if (!end || *end != '\0') {
    fprintf(stderr, "not a number: %s\n", argv[*i]);
    return -1;
  }
  *out = (int)value;
  return 0;
}

int main(int argc, char **argv) {
  int width_arg = 128;
  int height_arg = 64;
  int frames = 1;
  const char *pitch = "P1";
  const char *dump = NULL;
  Hub75Panel panel;
  int i;

  memset(&panel, 0, sizeof panel);
  for (i = 1; i < argc; i++) {
    if (strcmp(argv[i], "--help") == 0) {
      usage(stdout);
      return 0;
    } else if (strcmp(argv[i], "--width") == 0) {
      if (need_int(argc, argv, &i, &width_arg) != 0) {
        return 1;
      }
    } else if (strcmp(argv[i], "--height") == 0) {
      if (need_int(argc, argv, &i, &height_arg) != 0) {
        return 1;
      }
    } else if (strcmp(argv[i], "--pitch") == 0) {
      if (i + 1 >= argc) {
        fprintf(stderr, "missing value for --pitch\n");
        return 1;
      }
      pitch = argv[++i];
    } else if (strcmp(argv[i], "--dump") == 0) {
      if (i + 1 >= argc) {
        fprintf(stderr, "missing value for --dump\n");
        return 1;
      }
      dump = argv[++i];
    } else if (strcmp(argv[i], "--frames") == 0) {
      if (need_int(argc, argv, &i, &frames) != 0) {
        return 1;
      }
    } else {
      fprintf(stderr, "unknown argument: %s\n", argv[i]);
      usage(stderr);
      return 1;
    }
  }

  if (width_arg < 1 || height_arg < 1 || width_arg > 256 || height_arg > 256) {
    fprintf(stderr, "width and height must be from 1 to 256\n");
    return 1;
  }
  if (frames < 1) {
    fprintf(stderr, "--frames must be at least 1\n");
    return 1;
  }
  if (hub75_runtime_init(width_arg, height_arg) != 0) {
    fprintf(stderr, "could not allocate a %dx%d framebuffer\n", width_arg,
            height_arg);
    return 1;
  }
  if (hub75_panel_init(&panel, width_arg, height_arg, pitch) != 0) {
    fprintf(stderr,
            "unknown pitch \"%s\" (use P1, P2.5, or P4) or the panel image is "
            "too large\n",
            pitch);
    return 1;
  }

  browser_init(HUB75_SKETCH_NAME);

  setup();
  if (dump) {
    int frame;
    for (frame = 0; frame < frames; frame++) {
      frameCount++;
      draw();
    }
    hub75_panel_render(&panel, hub75_pixels());
    if (ppm_write(dump, panel.rgb, panel.img_w, panel.img_h) != 0) {
      fprintf(stderr, "could not write %s\n", dump);
      hub75_panel_free(&panel);
      return 1;
    }
    fprintf(stderr, "wrote %s (%dx%d, frame %lu, %s)\n", dump, panel.img_w,
            panel.img_h, frameCount, panel.pitch_name);
    hub75_panel_free(&panel);
    return 0;
  }

  {
    char title[128];
    snprintf(title, sizeof title, "%s  %dx%d  %s", HUB75_SKETCH_NAME, width_arg,
             height_arg, panel.pitch_name);
    fprintf(stderr,
            "%s  --  b browses sketches, left/right switch, s changes the led "
            "shape, p pauses, arrows are the board buttons, f toggles "
            "fullscreen, q or esc closes\n",
            title);
    sim_window_run(title, &panel);
  }
  hub75_panel_free(&panel);
  return 0;
}
