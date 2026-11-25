# HUB75 Sketcher

C sketches for a HUB75 LED panel. The same `setup` / `draw` source runs in a desktop LED simulator and as firmware on an ESP32-S3 (Adafruit Matrix Portal S3 or Waveshare ESP32-S3-RGB-Matrix).

## Simulator

Needs a C compiler, `make`, SDL3, and libcurl; OpenSSL enables `wss://`.

```sh
# macOS
brew install sdl3 curl openssl
# Debian
sudo apt install build-essential libsdl3-dev libcurl4-openssl-dev libssl-dev
```

```sh
make sim
```

Builds and opens `build/hub75-<sketch>` (default `drift`). Keys: `f` fullscreen, `q`/esc quit, `b` sketch browser (up/down or mouse, enter to load; left/right jump), up/down dim/brighten, `s` circle/square/none, `r` reset, `p` pause.

Options, with defaults:

```sh
make sim SKETCH=lattice
make sim SKETCH=steps WIDTH=64 HEIGHT=32
make sim SKETCH=drift PITCH=P4 ROTATE=90 BRIGHTNESS=64
```

- `SKETCH` (`drift`) — sketch to start; the browser lists all files in `sketches/`.
- `WIDTH`/`HEIGHT` (`128`/`64`) — panel LED counts; height must be 16, 32, or 64.
- `PITCH` (`P1`) — LED spacing: `P1`, `P2.5`, or `P4`.
- `ROTATE` (`0`) — `0`, `90`, `180`, or `270`; 90/270 swap the sketch's width/height.
- `BRIGHTNESS` (`255`) — start level, 0–255.

Headless: draw N frames and write the last as a PPM.

```sh
make sim SKETCH=drift ARGS="--dump /tmp/frame.ppm --frames 30"
```

## Sketches

Add a `.c` file in `sketches/` and build it by name:

```c
#include "hub75.h"

void setup(void) {}

void draw(void) {
    int x = (int)(frameCount % (unsigned)width);
    background(color(0, 0, 0));
    stroke(color(255, 255, 255));
    line(x, 0, x, height - 1);
}
```

```sh
make sim SKETCH=mysketch
```

API (`include/hub75.h`):

- `setup()`, `draw()`
- `width`, `height`, `frameCount`, `millis()`
- `color(r, g, b)` — packs 8-bit channels into RGB565
- `background`, `fill`/`noFill`, `stroke`/`noStroke`, `strokeWeight`
- `point`, `line`, `rect(x, y, w, h)`, `circle(x, y, diameter)`, `triangle`, `polygon`

`rect` takes the top-left corner and size; `circle` takes the center and diameter. `include/util.h` adds math, easing, hashes, an RNG, and `Vec2`/`Vec3` value types (`include/util/vector.h`); `include/util/font3x5.h` is a 3x5 microfont. The simulator and firmware both step one frame every 33 ms.

## Images and filters

`include/image.h` decodes JPEG, PNG, GIF (first frame), and BMP into
RGBA8888, resizes to the panel, and applies filters. The simulator reads
paths from the working directory; firmware can use `image_load_memory`
with an embedded asset.

```c
#include "hub75.h"
#include "image.h"

static Image photo;

void setup(void) {
    image_init(&photo);
    if (image_load(&photo, "tmp/picture.jpg") != 0) return;
    image_fit(&photo, width, height, IMAGE_COVER, IMAGE_BOX);

    /* image_dither(image, algorithm, levels, mono)
       levels 0 = panel 5/6/5, mono 1 = quantize luminance */
    image_dither(&photo, DITHER_FLOYD_STEINBERG, 0, 0);
}

void draw(void) {
    background(color(0, 0, 0));
    image_draw(&photo, 0, 0);
}
```

- `image_resize` and `image_fit` take `IMAGE_NEAREST`, `IMAGE_BILINEAR`, or
  `IMAGE_BOX`; `image_fit` takes `IMAGE_STRETCH`, `IMAGE_COVER`, or
  `IMAGE_CONTAIN`.
- `image_pixels(&photo)` returns the RGBA bytes (the `loadPixels` equivalent).
- Named filters show their options in the signature: `image_grayscale`,
  `image_invert`, `image_threshold`, `image_brightness`, `image_contrast`,
  `image_posterize`, `image_tint`, `image_duotone`, and `image_dither`.
  `filter(img, FilterOptions)` is the generic form if you prefer one
  dispatcher.
- Dither algorithms: Bayer 2/4/8, halftone, 16x16 blue noise,
  Floyd-Steinberg, Atkinson, Stucki, Burkes, Jarvis, and Sierra Lite.
- `levels` picks the palette size per channel; `mono = 1` dithers
  luminance instead. `FILTER_DUOTONE` after a mono dither gives a
  two-colour palette, the Game Boy look.

`make sim SKETCH=image-dither` loads `tmp/picture.jpg` and cycles the
algorithms. `HUB75_IMAGE=path` picks another file, `HUB75_MODE=n` starts
on one, and `HUB75_CYCLE=0` stops the cycle.

## Network

```c
#include "net.h"

void setup(void) { net_begin(); }

void draw(void) {
    NetResponse r;

    if (net_status() != NET_READY) return;   /* net_status_text() is a label */
    if (fetch("https://example.com/x.json", &r)) {
        /* r.body is NUL-terminated; parse it */
    }
    net_free(&r);
}
```

`net_begin()` is non-blocking; poll `net_status()` until `NET_READY`. `fetch` blocks; `fetch_start` / `fetch_busy` / `fetch_poll` are the non-blocking form. JSON uses the vendored yyjson. Logs go to stderr (sim) or Serial at 115200 (board):

```sh
make monitor PORT=/dev/cu.usbmodem1101
```

### WebSocket

```c
#include "net.h"
#include "ws.h"

void setup(void) {
    net_begin();
    ws_open("wss://example.com/feed");
}

void draw(void) {
    static char buf[4096];
    while (ws_poll(buf, sizeof buf) > 0) {
        /* buf holds one text message */
    }
}
```

`ws_status()` is `WS_CONNECTING` / `WS_OPEN` / `WS_CLOSED`; `ws_send()` writes a text frame. Neither side checks the TLS certificate.

### Secrets

WiFi credentials and API tokens go in `secrets.h` (gitignored):

```sh
make secrets   # copy secrets.example.h to secrets.h
```

## Firmware

Needs `arduino-cli`, the ESP32 core, and Adafruit Protomatter (Adafruit GFX comes with it):

```sh
arduino-cli config init
arduino-cli config add board_manager.additional_urls https://espressif.github.io/arduino-esp32/package_esp32_index.json
arduino-cli core update-index
arduino-cli core install esp32:esp32
arduino-cli lib install "Adafruit Protomatter"
```

`BOARD` selects the pin map and FQBN: `adafruit` (default, Matrix Portal S3) or `waveshare` (Waveshare ESP32-S3-RGB-Matrix). The boards wire the panel to different GPIOs. Flash after the board is on USB-C, using the port from `arduino-cli board list`:

```sh
make flash PORT=/dev/cu.usbmodem1101 SKETCH=drift BOARD=waveshare
```

Besides the simulator options, the firmware has `SATURATION` (0–200, default 100) and `GAMMA` (default 1.0) to match the panel's color to the sim, and `DEPTH` (3–6, default 4) for color bitplanes.

```sh
make firmware SKETCH=drift GAMMA=2.2 SATURATION=85 DEPTH=5
```

`NETDEBUG=1` draws a status overlay on the panel. `BROWNOUT=0` disables the brownout detector for a weak 5 V supply.

## Layout

- `include/hub75.h` — sketch API
- `include/image.h` — image decode, resize, and filter API
- `include/net.h`, `include/ws.h` — network and WebSocket sketch APIs
- `include/util.h`, `include/util/` — shared helpers
- `src/` — drawing, image pipeline, runtime, brightness, color, WebSocket protocol
- `sim/` — SDL3 window, raster, browser, net/ws transports, font
- `firmware/` — pin map, Protomatter wrapper, WiFi/TLS
- `vendor/yyjson/` — JSON reader (MIT)
- `vendor/stb/` — image decoder (public domain)
- `sketches/`, `frames/` — sketches and sample dumps
