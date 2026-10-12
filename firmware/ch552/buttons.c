#include "hub75.h"

/* The GBC-DCPixel board has no user buttons, so they always read released.
   A board that wires a switch to a CH552 pin can read it here instead. */
int buttonUp(void) { return 0; }
int buttonDown(void) { return 0; }
