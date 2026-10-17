#ifndef HUB75_PINS_H
#define HUB75_PINS_H

/* HUB75 pin maps, chosen by BOARD= on the make command line:

     adafruit   Adafruit Matrix Portal S3 (default)
     waveshare  Waveshare ESP32-S3-RGB-Matrix

   The boards wire the panel to different GPIOs, so a build for one shows
   nothing on the other. HUB75_HAS_BUTTONS is 1 only where the board has
   up/down brightness buttons. */

#ifdef HUB75_BOARD_WAVESHARE

/* Waveshare ESP32-S3-RGB-Matrix (ESP32-S3-N32R16).
   Source: docs.waveshare.com, ESP32-Peripheral-Tutorials/Display/LED-Matrix.
   No up/down buttons; its GPIO 6 and 7 are HUB75 data (B1 and R2). */

#define HUB75_R1 4
#define HUB75_G1 5
#define HUB75_B1 6
#define HUB75_R2 7
#define HUB75_G2 15
#define HUB75_B2 16

#define HUB75_A 18
#define HUB75_B 8
#define HUB75_C 3
#define HUB75_D 42
#define HUB75_E 9

#define HUB75_CLK 41
#define HUB75_LAT 40
#define HUB75_OE 2

#define HUB75_HAS_BUTTONS 0

#else

/* Adafruit Matrix Portal S3.
   Source: CircuitPython pins.c (MTX_*) and Adafruit_Protomatter simple.ino.
   Up/down buttons on GPIO 6 and 7, active low, internal pull-ups. */

#define HUB75_R1 42
#define HUB75_G1 41
#define HUB75_B1 40
#define HUB75_R2 38
#define HUB75_G2 39
#define HUB75_B2 37

#define HUB75_A 45
#define HUB75_B 36
#define HUB75_C 48
#define HUB75_D 35
#define HUB75_E 21

#define HUB75_CLK 2
#define HUB75_LAT 47
#define HUB75_OE 14

#define HUB75_BUTTON_UP 6
#define HUB75_BUTTON_DOWN 7

#define HUB75_HAS_BUTTONS 1

#endif

#endif
