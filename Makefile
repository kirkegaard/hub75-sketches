SKETCH ?= drift
WIDTH ?= 128
HEIGHT ?= 64
PITCH ?= P1
ROTATE ?= 0
BRIGHTNESS ?= 255
SATURATION ?= 100
GAMMA ?= 1.0
DEPTH ?= 4
BROWNOUT ?= 1
NETDEBUG ?= 0
ARGS ?=

# BOARD picks the HUB75 pin map (firmware/pins.h) and the matching FQBN.
# Both are ESP32-S3 boards, but the panel is wired to different GPIOs.
BOARD ?= adafruit
PORT ?=

ifeq ($(BOARD),waveshare)
# Waveshare ESP32-S3-RGB-Matrix: ESP32-S3-N32R16 (32 MB flash, 16 MB
# octal PSRAM). The board exposes native USB-Serial/JTAG, which the
# esp32s3 default (USBMode=hwcdc) matches.
FQBN ?= esp32:esp32:esp32s3:FlashSize=32M,PSRAM=opi
else
FQBN ?= esp32:esp32:adafruit_matrixportal_esp32s3
endif

BOARD_DEFS := $(if $(filter waveshare,$(BOARD)),-DHUB75_BOARD_WAVESHARE=1,)

CFLAGS = -std=c11 -Wall -Wextra -O2 -Iinclude -Isrc -Isim -I. -Ivendor/yyjson

# Vendored yyjson, built read-only: sketches only parse responses, so
# drop the writer. The JSON Pointer utility (yyjson_ptr_get) stays on.
YYJSON_DEFS = -DYYJSON_DISABLE_WRITER

# BROWNOUT=0 disables the ESP32 brownout detector, a workaround for
# running a panel plus WiFi off a weak 5V supply (a laptop USB port).
# It removes the safety trip, so the rail still has to be good enough.
BROWNOUT_DEFS := $(if $(filter 0,$(BROWNOUT)),-DHUB75_BROWNOUT_OFF=1,)

# NETDEBUG=1 draws the shared net/ws status overlay on the panel (reset
# reason, heap, net/ws state and counters) after every sketch frame.
NETDEBUG_DEFS := $(if $(filter 1,$(NETDEBUG)),-DHUB75_NET_DEBUG=1,)
CFLAGS += $(NETDEBUG_DEFS)

PKG_CONFIG ?= pkg-config
SDL_CFLAGS := $(shell $(PKG_CONFIG) --cflags sdl3 2>/dev/null)
SDL_LIBS := $(shell $(PKG_CONFIG) --libs sdl3 2>/dev/null)
ifeq ($(strip $(SDL_LIBS)),)
SDL_CFLAGS := $(shell sdl3-config --cflags 2>/dev/null)
SDL_LIBS := $(shell sdl3-config --libs 2>/dev/null)
endif

# The simulator's fetch() uses libcurl. If pkg-config cannot find it,
# fall back to -lcurl, which is present in the macOS and Debian SDKs.
CURL_CFLAGS := $(shell $(PKG_CONFIG) --cflags libcurl 2>/dev/null)
CURL_LIBS := $(shell $(PKG_CONFIG) --libs libcurl 2>/dev/null)
ifeq ($(strip $(CURL_LIBS)),)
CURL_LIBS := -lcurl
endif

# The simulator's WebSocket transport uses OpenSSL for wss://. Without
# it, the sim still builds and speaks plain ws:// only.
OPENSSL_CFLAGS := $(shell $(PKG_CONFIG) --cflags openssl 2>/dev/null)
OPENSSL_LIBS := $(shell $(PKG_CONFIG) --libs openssl 2>/dev/null)
ifeq ($(strip $(OPENSSL_LIBS)),)
WS_TLS_CFLAGS :=
WS_TLS_LIBS :=
else
WS_TLS_CFLAGS := -DWS_SIM_TLS=1 $(OPENSSL_CFLAGS)
WS_TLS_LIBS := $(OPENSSL_LIBS)
endif

# Optional local credentials, picked up by net.h and staged for firmware.
SECRETS := $(wildcard secrets.h)

BUILD = build
TARGET = $(BUILD)/hub75-$(SKETCH)

ARDUINO_CLI_CANDIDATE := $(shell command -v arduino-cli 2>/dev/null)
ifeq ($(ARDUINO_CLI_CANDIDATE),)
ARDUINO_CLI_CANDIDATE := /Applications/Arduino IDE.app/Contents/Resources/app/lib/backend/resources/arduino-cli
endif
ARDUINO_CLI ?= $(ARDUINO_CLI_CANDIDATE)

# Every sketch in sketches/ is linked into the simulator so the browser
# can switch between them at runtime. The name is sorted and any dash
# becomes an underscore for the renamed setup/draw symbols.
SKETCHES := $(sort $(notdir $(basename $(wildcard sketches/*.c))))
SKETCH_OBJS = $(foreach s,$(SKETCHES),$(BUILD)/sk-$(s).o)

# Shared sketch helpers. Every sketch object depends on these, and the
# firmware stage copies them next to hub75.h.
UTIL_HEADERS := include/util.h $(wildcard include/util/*.h)

OBJS = \
	$(BUILD)/graphics.o \
	$(BUILD)/runtime.o \
	$(BUILD)/brightness.o \
	$(BUILD)/color.o \
	$(BUILD)/net.o \
	$(BUILD)/ws.o \
	$(BUILD)/ws_sim.o \
	$(BUILD)/netdebug.o \
	$(BUILD)/yyjson.o \
	$(BUILD)/panel.o \
	$(BUILD)/ppm.o \
	$(BUILD)/input.o \
	$(BUILD)/main-$(SKETCH).o \
	$(BUILD)/window.o \
	$(BUILD)/browser.o \
	$(BUILD)/font5x7.o \
	$(BUILD)/sketch_registry.o \
	$(SKETCH_OBJS)

.PHONY: sim firmware flash secrets monitor check-sketch check-sdl check-board check-rotate check-rows check-brightness check-color check-depth clean

# Copy the credential template into place once. secrets.h is gitignored.
secrets:
	@if [ -f secrets.h ]; then \
		echo "secrets.h already exists"; \
	else \
		cp secrets.example.h secrets.h; \
		echo "wrote secrets.h; fill in your WiFi and last.fm values"; \
	fi

sim: check-sketch check-sdl $(TARGET)
	./$(TARGET) --width $(WIDTH) --height $(HEIGHT) --pitch $(PITCH) --rotate $(ROTATE) --brightness $(BRIGHTNESS) $(ARGS)

$(TARGET): $(OBJS)
	$(CC) -o $@ $(OBJS) $(SDL_LIBS) $(CURL_LIBS) $(WS_TLS_LIBS) -lm -pthread

check-sdl:
	@test -n "$(strip $(SDL_LIBS))" || { \
		echo "error: SDL3 not found."; \
		echo "The simulator window uses SDL3 and its GPU layer."; \
		echo "  brew install sdl3"; \
		echo "  sudo apt install libsdl3-dev"; \
		exit 1; \
	}

check-sketch:
	@test -f sketches/$(SKETCH).c || { \
		echo "error: sketches/$(SKETCH).c does not exist"; \
		echo "sketches:"; \
		ls sketches/*.c; \
		exit 1; \
	}

check-board:
	@case "$(BOARD)" in \
		adafruit|waveshare) ;; \
		*) echo "error: BOARD must be adafruit or waveshare (got $(BOARD))"; \
		   exit 1 ;; \
	esac

check-rotate:
	@case "$(ROTATE)" in \
		0|90|180|270) ;; \
		*) echo "error: ROTATE must be 0, 90, 180, or 270 (got $(ROTATE))"; \
		   exit 1 ;; \
	esac

# A single HUB75 panel tops out at 64 rows because Protomatter infers the
# height from the address lines (2 << addrCount, addrCount 1..5). A
# physically rotated panel wants ROTATE, not a taller HEIGHT.
check-rows:
	@case "$(HEIGHT)" in \
		16|32|64) ;; \
		*) echo "error: HEIGHT must be 16, 32, or 64 (got $(HEIGHT))."; \
		   echo "A single HUB75 panel cannot be taller than 64 rows."; \
		   echo "For a portrait image on a rotated 128x64 panel:"; \
		   echo "  make flash PORT=... WIDTH=128 HEIGHT=64 ROTATE=90"; \
		   exit 1 ;; \
	esac

check-brightness:
	@case "$(BRIGHTNESS)" in \
		''|*[!0-9]*) echo "error: BRIGHTNESS must be an integer from 0 to 255 (got $(BRIGHTNESS))"; \
		   exit 1 ;; \
	esac; \
	if [ "$(BRIGHTNESS)" -gt 255 ]; then \
		echo "error: BRIGHTNESS must be an integer from 0 to 255 (got $(BRIGHTNESS))"; \
		exit 1; \
	fi

check-depth:
	@case "$(DEPTH)" in \
		3|4|5|6) ;; \
		*) echo "error: DEPTH must be 3, 4, 5, or 6 (got $(DEPTH))"; \
		   exit 1 ;; \
	esac

check-color:
	@case "$(SATURATION)" in \
		''|*[!0-9]*) echo "error: SATURATION must be an integer from 0 to 200 (got $(SATURATION))"; \
		   exit 1 ;; \
	esac; \
	if [ "$(SATURATION)" -gt 200 ]; then \
		echo "error: SATURATION must be an integer from 0 to 200 (got $(SATURATION))"; \
		exit 1; \
	fi
	@printf '%s' "$(GAMMA)" | grep -Eq '^[0-9]+(\.[0-9]+)?$$' || { \
		echo "error: GAMMA must be a number like 1.0 or 2.2 (got $(GAMMA))"; \
		exit 1; \
	}

$(BUILD):
	mkdir -p $(BUILD)

$(BUILD)/graphics.o: src/graphics.c include/hub75.h src/graphics.h | $(BUILD)
	$(CC) $(CFLAGS) -c src/graphics.c -o $@

$(BUILD)/runtime.o: src/runtime.c src/runtime.h include/hub75.h src/graphics.h | $(BUILD)
	$(CC) $(CFLAGS) $(SDL_CFLAGS) -c src/runtime.c -o $@

$(BUILD)/brightness.o: src/brightness.c src/brightness.h | $(BUILD)
	$(CC) $(CFLAGS) -c src/brightness.c -o $@

$(BUILD)/color.o: src/color.c src/color.h | $(BUILD)
	$(CC) $(CFLAGS) -c src/color.c -o $@

$(BUILD)/net.o: sim/net_sim.c include/net.h $(SECRETS) | $(BUILD)
	$(CC) $(CFLAGS) $(CURL_CFLAGS) -c sim/net_sim.c -o $@

$(BUILD)/ws.o: src/ws.c include/ws.h include/util/base64.h | $(BUILD)
	$(CC) $(CFLAGS) -c src/ws.c -o $@

$(BUILD)/ws_sim.o: sim/ws_sim.c include/ws.h Makefile | $(BUILD)
	$(CC) $(CFLAGS) $(WS_TLS_CFLAGS) -c sim/ws_sim.c -o $@

$(BUILD)/netdebug.o: src/netdebug.c include/netdebug.h include/hub75.h include/net.h include/ws.h include/util/font3x5.h | $(BUILD)
	$(CC) $(CFLAGS) -c src/netdebug.c -o $@

# Makefile is a prerequisite so a change to YYJSON_DEFS rebuilds this
# (make does not otherwise track compile-flag changes).
$(BUILD)/yyjson.o: vendor/yyjson/yyjson.c vendor/yyjson/yyjson.h Makefile | $(BUILD)
	$(CC) $(CFLAGS) $(YYJSON_DEFS) -c vendor/yyjson/yyjson.c -o $@

$(BUILD)/panel.o: sim/panel.c sim/panel.h src/brightness.h | $(BUILD)
	$(CC) $(CFLAGS) -c sim/panel.c -o $@

$(BUILD)/input.o: sim/input.c sim/input.h sim/panel.h sim/browser.h src/brightness.h | $(BUILD)
	$(CC) $(CFLAGS) $(SDL_CFLAGS) -c sim/input.c -o $@

$(BUILD)/ppm.o: sim/ppm.c sim/ppm.h | $(BUILD)
	$(CC) $(CFLAGS) -c sim/ppm.c -o $@

$(BUILD)/main-$(SKETCH).o: sim/main.c include/hub75.h src/graphics.h src/runtime.h sim/panel.h sim/ppm.h sim/window.h sim/browser.h | $(BUILD)
	$(CC) $(CFLAGS) -DHUB75_SKETCH_NAME=\"$(SKETCH)\" -c sim/main.c -o $@

$(BUILD)/window.o: sim/window.c sim/window.h sim/panel.h sim/input.h include/hub75.h src/graphics.h src/brightness.h sim/browser.h sim/font5x7.h | $(BUILD)
	$(CC) $(CFLAGS) $(SDL_CFLAGS) -c sim/window.c -o $@

# Makefile is a prerequisite of the flag-sensitive objects (NETDEBUG etc.)
# so a change to a build flag actually rebuilds them.
$(BUILD)/browser.o: sim/browser.c sim/browser.h include/hub75.h src/graphics.h sim/font5x7.h include/netdebug.h Makefile | $(BUILD)
	$(CC) $(CFLAGS) -c sim/browser.c -o $@

$(BUILD)/font5x7.o: sim/font5x7.c sim/font5x7.h | $(BUILD)
	$(CC) $(CFLAGS) -c sim/font5x7.c -o $@

# The registry lists every sketch in sketches/ so the browser can show
# them. It depends on the folder itself, not just the files, so a rename
# (which keeps the file's mtime) still regenerates it.
$(BUILD)/sketch_registry.c: sketches/ $(wildcard sketches/*.c) sim/browser.h Makefile | $(BUILD)
	@{ \
		echo '#include "browser.h"'; \
		for s in $(SKETCHES); do \
			sym=`echo "$$s" | tr '-' '_'`; \
			echo "void $${sym}_setup(void);"; \
			echo "void $${sym}_draw(void);"; \
		done; \
		echo 'const Hub75Sketch hub75_sketches[] = {'; \
		for s in $(SKETCHES); do \
			sym=`echo "$$s" | tr '-' '_'`; \
			echo "  {\"$$s\", $${sym}_setup, $${sym}_draw},"; \
		done; \
		echo '};'; \
		echo 'const int hub75_sketch_count ='; \
		echo '    (int)(sizeof(hub75_sketches) / sizeof(hub75_sketches[0]));'; \
	} > $@

$(BUILD)/sketch_registry.o: $(BUILD)/sketch_registry.c sim/browser.h
	$(CC) $(CFLAGS) -c $(BUILD)/sketch_registry.c -o $@

$(BUILD)/sk-%.o: sketches/%.c include/hub75.h include/net.h include/ws.h include/netdebug.h vendor/yyjson/yyjson.h $(UTIL_HEADERS) $(SECRETS) Makefile | $(BUILD)
	$(CC) $(CFLAGS) -Dsetup=$(subst -,_,$*)_setup -Ddraw=$(subst -,_,$*)_draw -c $< -o $@

firmware: check-sketch check-board check-rotate check-rows check-brightness check-color check-depth
	@cli="$(ARDUINO_CLI)"; \
	fqbn="$(FQBN)"; \
	if [ ! -x "$$cli" ]; then \
		echo "error: arduino-cli not found."; \
		echo "The simulator does not need it. Build that with: make sim"; \
		echo ""; \
		echo "To build firmware later, install Arduino CLI (or Arduino IDE 2,"; \
		echo "which ships a copy) and then:"; \
		echo "  arduino-cli config init"; \
		echo "  arduino-cli config add board_manager.additional_urls https://espressif.github.io/arduino-esp32/package_esp32_index.json"; \
		echo "  arduino-cli core update-index"; \
		echo "  arduino-cli core install esp32:esp32"; \
		echo "  arduino-cli lib install \"Adafruit Protomatter\""; \
		echo "  make firmware SKETCH=$(SKETCH)"; \
		exit 1; \
	fi; \
	if ! "$$cli" board details -b "$$fqbn" >/dev/null 2>&1; then \
		echo "error: board $$fqbn is not installed."; \
		echo "The Matrix Portal S3 core is missing. The simulator still builds: make sim"; \
		echo ""; \
		echo "When you want firmware:"; \
		echo "  arduino-cli config init"; \
		echo "  arduino-cli config add board_manager.additional_urls https://espressif.github.io/arduino-esp32/package_esp32_index.json"; \
		echo "  arduino-cli core update-index"; \
		echo "  arduino-cli core install esp32:esp32"; \
		echo "  arduino-cli lib install \"Adafruit Protomatter\""; \
		echo "  make firmware SKETCH=$(SKETCH) WIDTH=$(WIDTH) HEIGHT=$(HEIGHT)"; \
		exit 1; \
	fi; \
	if ! "$$cli" lib list 2>/dev/null | grep -q "Adafruit Protomatter"; then \
		echo "error: Adafruit Protomatter is not installed."; \
		echo "The simulator does not need it."; \
		echo "  arduino-cli lib install \"Adafruit Protomatter\""; \
		echo "That also installs Adafruit GFX, which Protomatter requires."; \
		exit 1; \
	fi; \
	stage="$(BUILD)/sketch"; \
	rm -rf "$$stage"; \
	mkdir -p "$$stage"; \
	cp include/hub75.h include/util.h include/net.h include/ws.h include/netdebug.h src/graphics.h src/graphics.c src/brightness.h src/brightness.c src/color.h src/color.c src/ws.c src/netdebug.c "$$stage/"; \
	mkdir -p "$$stage/util"; \
	cp include/util/*.h "$$stage/util/"; \
	cp firmware/pins.h firmware/portal.h firmware/portal.cpp firmware/net_fw.cpp firmware/ws_fw.cpp "$$stage/"; \
	cp firmware/runtime_fw.c firmware/glue.c firmware/sketch.ino "$$stage/"; \
	cp vendor/yyjson/yyjson.h vendor/yyjson/yyjson.c "$$stage/"; \
	if [ -f secrets.h ]; then cp secrets.h "$$stage/"; fi; \
	cp sketches/$(SKETCH).c "$$stage/sketch_user.inc"; \
	printf '#pragma once\n#define HUB75_PANEL_WIDTH %s\n#define HUB75_PANEL_HEIGHT %s\n#define HUB75_ROTATE %s\n#define HUB75_BRIGHTNESS %s\n#define HUB75_SATURATION %s\n#define HUB75_GAMMA %sf\n#define HUB75_DEPTH %s\n' \
		"$(WIDTH)" "$(HEIGHT)" "$(ROTATE)" "$(BRIGHTNESS)" "$(SATURATION)" "$(GAMMA)" "$(DEPTH)" > "$$stage/config.h"; \
	echo "compiling $(SKETCH) for $$fqbn (board $(BOARD) $(WIDTH)x$(HEIGHT) rot $(ROTATE) bri $(BRIGHTNESS) sat $(SATURATION) gam $(GAMMA) depth $(DEPTH))"; \
	"$$cli" compile --fqbn "$$fqbn" --build-property "compiler.c.extra_flags=$(YYJSON_DEFS) -DHUB75_FIRMWARE=1 $(NETDEBUG_DEFS) $(BOARD_DEFS)" --build-property "compiler.cpp.extra_flags=$(BROWNOUT_DEFS) $(NETDEBUG_DEFS) $(BOARD_DEFS)" --output-dir "$(BUILD)/firmware-out" "$$stage"

# Follow the board's Serial output at 115200 (net status, fetch results).
monitor:
	@if [ -z "$(PORT)" ]; then \
		echo "error: set PORT to the Matrix Portal's serial device."; \
		echo "  arduino-cli board list"; \
		echo "  make monitor PORT=/dev/cu.usbmodem1101"; \
		exit 1; \
	fi
	@"$(ARDUINO_CLI)" monitor -p "$(PORT)" -b "$(FQBN)" -c baudrate=115200

flash: firmware
	@if [ -z "$(PORT)" ]; then \
		echo "error: set PORT to the Matrix Portal's serial device."; \
		echo "  arduino-cli board list"; \
		echo "  make flash PORT=/dev/cu.usbmodem1101 SKETCH=$(SKETCH)"; \
		exit 1; \
	fi
	@"$(ARDUINO_CLI)" upload -p "$(PORT)" --fqbn "$(FQBN)" --input-dir "$(BUILD)/firmware-out"

clean:
	rm -rf $(BUILD)
