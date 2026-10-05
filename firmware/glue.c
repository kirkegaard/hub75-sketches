/* The sketch's setup/draw keep those names in source. This translation
   unit renames them so they do not collide with Arduino's setup(). */
#define setup hub75_setup
#define draw hub75_draw
#include "sketch_user.inc"
