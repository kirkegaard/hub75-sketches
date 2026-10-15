#include "graphics.h"
#include "hub75.h"
#include "panel.h"
#include "ppm.h"
#include "runtime.h"
#include "window.h"

#include "brightness.h"
#include "browser.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifndef HUB75_SKETCH_NAME
#define HUB75_SKETCH_NAME "sketch"
#endif

static void usage(FILE *fp) {
  fputs("usage: hub75-sim [options]\n"
        "  --width N        LED columns (default 128)\n"
        "  --height N       LED rows (default 64)\n"
        "  --pitch NAME     P1, P2.5, or P4 (default P1)\n"
        "  --rotate N       panel rotation: 0, 90, 180, or 270 (default 0)\n"
        "  --brightness N   panel brightness 0 to 255 (default 255)\n"
        "  --dump FILE      write the last frame as a PPM and exit\n"
        "  --frames N       frames to draw before --dump (default 1)\n"
        "  --help           show this text\n",
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
  int rotate = 0;
  int brightness = 255;
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
    } else if (strcmp(argv[i], "--rotate") == 0) {
      if (need_int(argc, argv, &i, &rotate) != 0) {
        return 1;
      }
    } else if (strcmp(argv[i], "--brightness") == 0) {
      if (need_int(argc, argv, &i, &brightness) != 0) {
        return 1;
      }
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
  if (rotate != 0 && rotate != 90 && rotate != 180 && rotate != 270) {
    fprintf(stderr, "--rotate must be 0, 90, 180, or 270\n");
    return 1;
  }
  if (brightness < HUB75_BRIGHTNESS_MIN || brightness > HUB75_BRIGHTNESS_MAX) {
    fprintf(stderr, "--brightness must be from 0 to 255\n");
    return 1;
  }
  if (frames < 1) {
    fprintf(stderr, "--frames must be at least 1\n");
    return 1;
  }

  /* The panel is width x height electrically; a rotated panel shows the
     sketch turned, so 90 and 270 swap the sketch's own width and height.
     The simulator draws what the sketch sees, so it renders the view. */
  {
    int view_w = (rotate == 90 || rotate == 270) ? height_arg : width_arg;
    int view_h = (rotate == 90 || rotate == 270) ? width_arg : height_arg;

    if (hub75_runtime_init(view_w, view_h) != 0) {
      fprintf(stderr, "could not allocate a %dx%d framebuffer\n", view_w,
              view_h);
      return 1;
    }
    if (hub75_panel_init(&panel, view_w, view_h, pitch) != 0) {
      fprintf(stderr,
              "unknown pitch \"%s\" (use P1, P2.5, or P4) or the panel image "
              "is too large\n",
              pitch);
      return 1;
    }
  }

  hub75_brightness_set(brightness);
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
    char rot[16];
    rot[0] = '\0';
    if (rotate != 0) {
      snprintf(rot, sizeof rot, "  rot %d", rotate);
    }
    snprintf(title, sizeof title, "%s  %dx%d  %s%s", HUB75_SKETCH_NAME, width_arg,
             height_arg, panel.pitch_name, rot);
    fprintf(stderr,
            "%s  --  b browses sketches, left/right switch, s changes the led "
            "shape, r resets, p pauses, up/down dim the panel, f toggles "
            "fullscreen, q or esc closes\n",
            title);
    sim_window_run(title, &panel);
  }
  hub75_panel_free(&panel);
  return 0;
}
