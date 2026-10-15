# HUB75 Sketcher

C sketches for a 128—64 HUB75 panel. The same `setup` / `draw` source builds into a desktop LED simulator and, later, firmware for an Adafruit Matrix Portal S3.

The simulator is the thing that runs without the board. It draws round LEDs with gaps, not a scaled-up bitmap.

## Run the simulator

Needs a C compiler, `make`, and SDL3. The window uses SDL3's GPU layer. A headless `--dump` still needs the library, because it is linked into the simulator.

```sh
# MacOS
brew install sdl3
# Debian
sudo apt install build-essential libsdl3-dev
```

On macos you might need to install command line tools.

```sh
make sim
```

That builds `build/hub75-<sketch>` (for the default, `build/hub75-drift`) and opens the panel. The default sketch is `drift`, 128—64, pitch P1. Press `f` to toggle fullscreen, or `q` or esc to close the window.

The simulator links every file in `sketches/`. Press `b` to open a small browser in the top right, use the up and down arrow keys (or the mouse) to pick a sketch, and enter to load it. The left and right arrow keys jump straight to the previous or next sketch. While the browser is closed the up and down arrow keys dim and brighten the panel; when it is open they move the list instead. The window title shows the current level. Press `s` to cycle the lit LED shape between a circle and a square, `r` to reset the running sketch (like the board's reset button), and `p` to pause the sketch.

`make sim` also accepts the sketch, the LED count, the pitch, the rotation, and the starting brightness:

```sh
make sim SKETCH=lattice
make sim SKETCH=steps WIDTH=64 HEIGHT=32
make sim SKETCH=drift PITCH=P4
make sim SKETCH=lattice WIDTH=64 HEIGHT=32 PITCH=P2.5
make sim SKETCH=textwave WIDTH=128 HEIGHT=64 ROTATE=90
make sim SKETCH=drift BRIGHTNESS=64
```

`WIDTH` and `HEIGHT` are the panel's electrical LED counts (columns by rows). Typical panels step by 32 (32, 64, 128) and are 16, 32, or 64 rows tall. The default is 128—64.

### Brightness

`BRIGHTNESS` is the level the panel starts at, `0` (off) to `255` (full, the default). The up and down keys in the simulator (and the buttons on the board) step it from there, so `BRIGHTNESS` just picks the starting point instead of always starting at full. `make firmware` rejects values outside 0—255.

### Rotation

A panel can be mounted turned on its side. `ROTATE` spins the sketch's view on the panel: `0`, `90`, `180`, or `270` (`0` by default). The panel keeps its electrical `WIDTH`—`HEIGHT`; `90` and `270` swap `width` and `height` inside the sketch, and `180` leaves them alone. A 128—64 panel with `ROTATE=90` gives a sketch a 64—128 portrait canvas, and the simulator opens a 64—128 window so the preview matches the board.

Which of `90` and `270` you want depends on the direction you turned the panel: if the picture comes out upside down, use the other one.

`HEIGHT` is capped at 64 on the board (see below), so a portrait panel uses `ROTATE`, not a taller `HEIGHT`. `make firmware WIDTH=64 HEIGHT=128` is a build error and points at the rotated form.

### Pitch

Pitch is the center-to-center LED spacing. The same resolution is a physically larger panel at P4 than at P1. The simulator uses 8 screen pixels per millimeter, and the lit shape is about 70% of the cell so the gap stays visible. The shape (circle or square, toggled with `s`) is separate from the pitch: pitch is always the spacing. P1 uses a 7px shape. A 5px shape in that 8px cell is a 4×4 square.

| Pitch | Spacing | Cell | Lit shape | 128—64 image |
| --- | --- | --- | --- | --- |
| P1 | 1.0 mm | 8 px | 7 px | 1024—512 |
| P2.5 | 2.5 mm | 20 px | 14 px | 2560—1280 |
| P4 | 4.0 mm | 32 px | 22 px | 4096—2048 |

Names are `P1`, `P2.5`, and `P4`. If the panel is wider than the screen, each LED is drawn with fewer whole pixels so the window still fits. The PPM dump is always full size. Off LEDs are dark shapes on a darker face, so the grid reads even where the sketch paints black.

### Rendering

The window rasterizes the round (or square) LEDs on the CPU into an RGB24 image (`sim/panel.c`) and blits it with SDL3's 2D renderer at the panel's pitch, so it needs SDL3 but no shaders. The browser overlay is drawn with the same renderer. The headless `--dump` uses the same rasterizer.

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

That file is what the firmware builds too. `SKETCH` only picks which sketch the simulator starts on; the browser lists all of them. The public API is `include/hub75.h`:

- `setup()`, `draw()`
- `width`, `height`, `frameCount` (1 on the first `draw`), `millis()`
- `color(r, g, b)` packs 8-bit channels into RGB565
- `background`, `fill`, `noFill`, `stroke`, `noStroke`, `strokeWeight`
- `point`, `line`, `rect(x, y, w, h)`, `circle(x, y, diameter)`, `triangle`, `polygon`

`rect` is the top-left corner and the size. `circle` is the center and the diameter. Their stroke is drawn inside that footprint. `triangle` is three corners in fractional pixels, so a slow turn does not snap. `polygon` is a regular polygon: center, radius out to a vertex, side count, and rotation in radians, with 0 putting a vertex on the right. Their stroke lies on the edges. Fill and stroke start white and enabled. The framebuffer is row-major RGB565.

Besides `hub75.h`, a sketch can `#include "util.h"` for the helpers the sketches share: interpolation primitives (`clampf`, `clamp01`, `clamp_int`, `clamp_byte`, `lerp`, `mapf`, `smoothstep`), the easing set (`ease_in_quad` through `ease_in_out_bounce`; the names follow easings.net), hashes (`hash_mix`, `hash_combine2`, `hash_combine3`, `hash_unit`, `hash_i01`), the RNG (`rng_next`, `rng_float`, which take a `uint32_t *` state the sketch owns), and `gcd_i`. They are `static inline`, split by kind under `include/util/` (primitives in `interp.h`, curves in `ease.h`), so a new one is a small header away. The firmware build stages these headers next to the sketch.

The window and the firmware both step one frame every 33 ms, so a sketch paced on `frameCount` feels the same on both. A headless dump does not sleep; it just runs the requested number of frames. `frameCount` is 30 in a `--frames 30` dump.

## Firmware

Board: Adafruit Matrix Portal S3 (ESP32-S3, 8 MB flash, 2 MB RAM), one HUB75 panel. WiFi, BLE, and the LIS3DH are out of scope. The up and down buttons dim and brighten the panel, starting from `BRIGHTNESS` (255 if unset).

```sh
make firmware SKETCH=drift WIDTH=128 HEIGHT=64
make firmware SKETCH=drift BRIGHTNESS=64
make firmware SKETCH=drift GAMMA=2.2 SATURATION=85
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

`HEIGHT` must be 16, 32, or 64 so the address line count is 3, 4, or 5; a build with any other `HEIGHT` stops with a note to use `ROTATE` instead. `ROTATE` (0, 90, 180, or 270) turns the sketch's view on the panel, so a rotated 128—64 panel shows a 64—128 canvas. A 64-row panel uses address E. Protomatter is asked for 4-bit color and a double buffer first. If `begin()` fails (usually DMA RAM), the build falls back to a single buffer, then to 4-bit's smaller cousin, depth 3. Each frame is copied from the sketch's RGB565 buffer into Protomatter's canvas, reoriented for `ROTATE`, and `show()` pushes it. The C++ is only `firmware/portal.cpp`. Sketch `setup` / `draw` stay C; the firmware build renames those two symbols so they do not collide with Arduino's `setup`.

### Color

The framebuffer is display-referred: the simulator shows it directly on an sRGB monitor. A HUB75 panel is not a monitor. It drives linear-PWM LEDs with narrow-band emitters and true-black gaps, so the same RGB565 sample looks brighter and more saturated on the board, most of all in the midtones. Two firmware-only knobs narrow that gap. The simulator stays the reference, so it does not apply them.

- `SATURATION` (percent, 0 to 200, default 100) mixes each pixel toward its luma. Lower values calm the panel down.
- `GAMMA` (default 1.0) treats the input as sRGB and linearizes it for the LEDs. `2.2` is the usual starting point; it darkens the midtones so they match the sim.

`100` and `1.0` leave every sample untouched. Both run in `src/color.c` before `BRIGHTNESS` scales the duty, and the build rejects values outside those ranges.

### Pin map

Taken from CircuitPython `ports/espressif/boards/adafruit_matrixportal_s3/pins.c` (`MTX_R1` through `MTX_OE`, `MTX_ADDRE`) and from `Adafruit_Protomatter` `examples/simple/simple.ino` (`ARDUINO_ADAFRUIT_MATRIXPORTAL_ESP32S3`).

| Signal | GPIO |
| --- | --- |
| R1, G1, B1 | 42, 41, 40 |
| R2, G2, B2 | 38, 39, 37 |
| A, B, C, D, E | 45, 36, 48, 35, 21 |
| CLK, LAT, OE | 2, 47, 14 |
| Button up, button down | 6, 7 |

Buttons are active low and the board has no external pull-ups, so the firmware enables the internal pull-up. A press steps the brightness; Protomatter has no brightness setting, so `portal_present` scales each RGB565 sample before the copy, the same way the simulator scales its LEDs. The buttons are no longer part of the sketch API.

Address E is GPIO 21. The learn guide's "Address E Line Jumper" is about the HUB75 connector, not that GPIO: the jumper ships closed to connector pin 8, which is what Adafruit's 64-row panels use. Other panels that want E on connector pin 16 need the trace cut and the other pad bridged. GPIO 8 on this board is the RX pad.

## Layout

- `include/hub75.h` — sketch API
- `src/graphics.c` — RGB565 drawing
- `src/runtime.c` — simulator clock
- `src/brightness.c` — panel brightness and the dim helpers
- `src/color.c` — panel saturation and gamma correction
- `sim/panel.c` — CPU raster and the LED shape/pitch model
- `sim/window.c` — SDL3 window, blit, and the frame loop
- `sim/input.c` — keyboard and mouse events
- `sim/browser.c` — sketch list and the setup/draw dispatch
- `sim/font5x7.c` — pixel font for the browser overlay
- `sketches/` — `drift`, `lattice`, `steps`
- `firmware/` — pin map and the Protomatter wrapper
- `frames/` — dumped panels
