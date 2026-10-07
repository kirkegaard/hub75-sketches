# HUB75 Sketcher

C sketches for a 128—64 HUB75 panel. The same `setup` / `draw` source builds into a desktop LED simulator and, later, firmware for an Adafruit Matrix Portal S3.

The simulator is the thing that runs without the board. It draws round LEDs with gaps, not a scaled-up bitmap.

## Run the simulator

Needs a C compiler, `make`, and SDL2. The window uses SDL2. A headless `--dump` still needs the library, because it is linked into the simulator.

```sh
# MacOS
brew install sdl2
# Debian
sudo apt install build-essential libsdl2-dev
```

On macos you might need to install command line tools.

```sh
make sim
```

That builds `build/hub75-<sketch>` (for the default, `build/hub75-drift`) and opens the panel. The default sketch is `drift`, 128—64, pitch P1. Press `f` to toggle fullscreen, or `q` or esc to close the window. The up and down arrow keys are the board's two user buttons.

`make sim` also accepts the sketch, the LED count, and the pitch:

```sh
make sim SKETCH=lattice
make sim SKETCH=steps WIDTH=64 HEIGHT=32
make sim SKETCH=drift PITCH=P4
make sim SKETCH=lattice WIDTH=64 HEIGHT=32 PITCH=P2.5
```

`WIDTH` and `HEIGHT` are LED counts. Typical panels step by 32 (32, 64, 128). The default is 128—64.

### Pitch

Pitch is the center-to-center LED spacing. The same resolution is a physically larger panel at P4 than at P1. The simulator uses 8 screen pixels per millimeter, and each LED disk is about 70% of the cell so the gap stays visible. P1 uses a 7px disk. A 5px disk in that 8px cell is a 4×4 square.

| Pitch | Spacing | Cell | LED disk | 128—64 image |
| --- | --- | --- | --- | --- |
| P1 | 1.0 mm | 8 px | 7 px | 1024—512 |
| P2.5 | 2.5 mm | 20 px | 14 px | 2560—1280 |
| P4 | 4.0 mm | 32 px | 22 px | 4096—2048 |

Names are `P1`, `P2.5`, and `P4`. If the panel is wider than the screen, each LED is drawn with fewer whole pixels, so every disk stays the same circle. The PPM dump is always full size. Off LEDs are dark disks on a darker face, so the grid reads even where the sketch paints black.

### Headless frame dump

No window. Draws N frames and writes the last one as a binary PPM (P6):

```sh
make sim SKETCH=drift ARGS="--dump /tmp/hub75-frame.ppm --frames 30"
```

`--frames` defaults to 1 if you leave it out. The same flags work on the binary. Make passes size and pitch first, then `ARGS`, and later copies of a flag win:

```sh
./build/hub75-drift --width 64 --height 32 --pitch P4 --dump frames/out.ppm --frames 30
```

Sample dumps live in `frames/` as PPM, with a PNG preview of each.

### Add a sketch

Put a `.c` file in `sketches/` and point `make` at its name (no path, no extension):

```c
#include "hub75.h"

static uint16_t bg, fg;

void setup(void) {
    bg = color(0, 0, 0);
    fg = color(255, 255, 255);
}

void draw(void) {
    int x = (int)(frameCount % (unsigned)width);
    background(bg);
    stroke(fg);
    line(x, 0, x, height - 1);
}
```

```sh
make sim SKETCH=mysketch
```

That file is what the firmware builds too. The public API is `include/hub75.h`:

- `setup()`, `draw()`
- `width`, `height`, `frameCount` (1 on the first `draw`), `millis()`
- `color(r, g, b)` packs 8-bit channels into RGB565
- `background`, `fill`, `noFill`, `stroke`, `noStroke`, `strokeWeight`
- `point`, `line`, `rect(x, y, w, h)`, `circle(x, y, diameter)`, `triangle`, `polygon`
- `buttonUp()`, `buttonDown()` return 1 when pressed

`rect` is the top-left corner and the size. `circle` is the center and the diameter. Their stroke is drawn inside that footprint. `triangle` is three corners in fractional pixels, so a slow turn does not snap. `polygon` is a regular polygon: center, radius out to a vertex, side count, and rotation in radians, with 0 putting a vertex on the right. Their stroke lies on the edges. Fill and stroke start white and enabled. The framebuffer is row-major RGB565.

The window and the firmware both step one frame every 33 ms, so a sketch paced on `frameCount` feels the same on both. A headless dump does not sleep; it just runs the requested number of frames. `frameCount` is 30 in a `--frames 30` dump.

## Firmware

Board: Adafruit Matrix Portal S3 (ESP32-S3, 8 MB flash, 2 MB RAM), one HUB75 panel. WiFi, BLE, and the LIS3DH are out of scope. The up and down buttons are readable from the sketch.

```sh
make firmware SKETCH=drift WIDTH=128 HEIGHT=64
```

This needs `arduino-cli`, the Espressif ESP32 core (it contains the Matrix Portal S3 board), and Adafruit Protomatter. Adafruit GFX comes in with Protomatter. If any of that is missing, `make firmware` prints the install commands and exits. The simulator does not use them.

```sh
arduino-cli config init
arduino-cli config add board_manager.additional_urls https://espressif.github.io/arduino-esp32/package_esp32_index.json
arduino-cli core update-index
arduino-cli core install esp32:esp32
arduino-cli lib install "Adafruit Protomatter"
make firmware SKETCH=drift
```

The compile is `esp32:esp32:adafruit_matrixportal_esp32s3`. Arduino CLI is also bundled inside Arduino IDE 2; `make` uses a `PATH` copy first, then that bundled binary.

Flash after the board is on USB-C. `arduino-cli board list` prints the port.

```sh
make flash PORT=/dev/cu.usbmodem1101 SKETCH=drift
```

`HEIGHT` must be 16, 32, or 64 so the address line count is 3, 4, or 5. A 64-row panel uses address E. Protomatter is asked for 4-bit color and a double buffer first. If `begin()` fails (usually DMA RAM), the build falls back to a single buffer, then to 4-bit's smaller cousin, depth 3. Each frame is copied from the sketch's RGB565 buffer into Protomatter's canvas and `show()` pushes it. The C++ is only `firmware/portal.cpp`. Sketch `setup` / `draw` stay C; the firmware build renames those two symbols so they do not collide with Arduino's `setup`.

### Pin map

Taken from CircuitPython `ports/espressif/boards/adafruit_matrixportal_s3/pins.c` (`MTX_R1` through `MTX_OE`, `MTX_ADDRE`) and from `Adafruit_Protomatter` `examples/simple/simple.ino` (`ARDUINO_ADAFRUIT_MATRIXPORTAL_ESP32S3`).

| Signal | GPIO |
| --- | --- |
| R1, G1, B1 | 42, 41, 40 |
| R2, G2, B2 | 38, 39, 37 |
| A, B, C, D, E | 45, 36, 48, 35, 21 |
| CLK, LAT, OE | 2, 47, 14 |
| Button up, button down | 6, 7 |

Buttons are active low and the board has no external pull-ups, so the firmware enables the internal pull-up. A press reads as 1 from `buttonUp()` / `buttonDown()`.

Address E is GPIO 21. The learn guide's "Address E Line Jumper" is about the HUB75 connector, not that GPIO: the jumper ships closed to connector pin 8, which is what Adafruit's 64-row panels use. Other panels that want E on connector pin 16 need the trace cut and the other pad bridged. GPIO 8 on this board is the RX pad.

## Layout

- `include/hub75.h` — sketch API
- `src/graphics.c` — RGB565 drawing
- `src/runtime.c` — simulator clock and button reads
- `sim/panel.c` — round-LED raster
- `sim/window.c` — SDL2 window
- `sketches/` — `drift`, `lattice`, `steps`
- `firmware/` — pin map and the Protomatter wrapper
- `frames/` — dumped panels
