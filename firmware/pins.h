#ifndef HUB75_PINS_H
#define HUB75_PINS_H

/* Matrix Portal S3 HUB75 wiring.
   Looked up, not guessed:

   CircuitPython board pins
     ports/espressif/boards/adafruit_matrixportal_s3/pins.c
     MTX_R1..MTX_B2, MTX_ADDRA..MTX_ADDRE, MTX_CLK, MTX_LAT, MTX_OE

   Adafruit_Protomatter examples/simple/simple.ino
     the ARDUINO_ADAFRUIT_MATRIXPORTAL_ESP32S3 block
     rgbPins  {42, 41, 40, 38, 39, 37}
     addrPins {45, 36, 48, 35, 21}
     clock 2, latch 47, oe 14

   Learn guide "Adafruit MatrixPortal S3 / Pinouts", Address E Line Jumper:
     the PCB jumper ships closed to HUB75 connector pin 8, which is what
     Adafruit's own 64-row panels expect. That "8" is the ribbon pin, not
     an ESP32 GPIO. The GPIO that drives address E is 21 (MTX_ADDRE).
     GPIO 8 on this board is the RX pad.

   Buttons, same pinout page: up is GPIO 6, down is GPIO 7.
   Neither has an external pull-up; a press pulls the pin low.
*/

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

#endif
