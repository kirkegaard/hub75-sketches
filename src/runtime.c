#include "hub75.h"

#include "graphics.h"
#include "window.h"

#include <time.h>

int width = 0;
int height = 0;
unsigned long frameCount = 0;

static struct timespec started;

int hub75_runtime_init(int w, int h) {
  if (hub75_gfx_init(w, h) != 0) {
    return -1;
  }
  frameCount = 0;
  clock_gettime(CLOCK_MONOTONIC, &started);
  return 0;
}

unsigned long millis(void) {
  struct timespec now;
  long sec;
  long ms;

  clock_gettime(CLOCK_MONOTONIC, &now);
  sec = (long)(now.tv_sec - started.tv_sec);
  ms = sec * 1000L + (long)(now.tv_nsec - started.tv_nsec) / 1000000L;
  if (ms < 0) {
    return 0;
  }
  return (unsigned long)ms;
}

int buttonUp(void) { return sim_button_up(); }

int buttonDown(void) { return sim_button_down(); }
