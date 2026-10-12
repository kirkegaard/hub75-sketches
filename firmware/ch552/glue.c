/* The sketch's setup/draw keep those names in source. This translation
   unit renames them so they do not collide with Arduino's setup().

   Same idea as firmware/glue.c, but the sketch is staged with a .h
   extension: arduino-cli only copies known sketch extensions into its
   build cache, and .inc is not one of them. */
#define setup hub75_setup
#define draw hub75_draw
#include "sketch_user.h"
