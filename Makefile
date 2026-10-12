SKETCH ?= drift
WIDTH ?= 128
HEIGHT ?= 64
PITCH ?= P1
ARGS ?=

FQBN ?= esp32:esp32:adafruit_matrixportal_esp32s3
PORT ?=

# Secondary target: a CH552 (ch55xduino) driving an 8x8 WS2812 matrix, the
# board from nataliethenerd/GBC-DCPixel. Its own core and FQBN; the ESP32
# target above is unchanged.
CH55X_FQBN ?= CH55xDuino:mcs51:ch552:clock=16internal
CH55X_CORE ?= CH55xDuino:mcs51
CH55X_URL := https://raw.githubusercontent.com/DeqingSun/ch55xduino/ch55xduino/package_ch55xduino_mcs51_index.json
CH552_WIDTH ?= 8
CH552_HEIGHT ?= 8

CFLAGS = -std=c11 -Wall -Wextra -O2 -Iinclude -Isrc -Isim

PKG_CONFIG ?= pkg-config
SDL_CFLAGS := $(shell $(PKG_CONFIG) --cflags sdl3 2>/dev/null)
SDL_LIBS := $(shell $(PKG_CONFIG) --libs sdl3 2>/dev/null)
ifeq ($(strip $(SDL_LIBS)),)
SDL_CFLAGS := $(shell sdl3-config --cflags 2>/dev/null)
SDL_LIBS := $(shell sdl3-config --libs 2>/dev/null)
endif

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
	$(BUILD)/panel.o \
	$(BUILD)/ppm.o \
	$(BUILD)/input.o \
	$(BUILD)/main-$(SKETCH).o \
	$(BUILD)/window.o \
	$(BUILD)/browser.o \
	$(BUILD)/font5x7.o \
	$(BUILD)/sketch_registry.o \
	$(SKETCH_OBJS)

.PHONY: sim firmware flash firmware-ch552 flash-ch552 check-sketch check-sdl clean

sim: check-sketch check-sdl $(TARGET)
	./$(TARGET) --width $(WIDTH) --height $(HEIGHT) --pitch $(PITCH) $(ARGS)

$(TARGET): $(OBJS)
	$(CC) -o $@ $(OBJS) $(SDL_LIBS) -lm

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

$(BUILD):
	mkdir -p $(BUILD)

$(BUILD)/graphics.o: src/graphics.c include/hub75.h src/graphics.h | $(BUILD)
	$(CC) $(CFLAGS) -c src/graphics.c -o $@

$(BUILD)/runtime.o: src/runtime.c src/runtime.h include/hub75.h src/graphics.h sim/input.h | $(BUILD)
	$(CC) $(CFLAGS) $(SDL_CFLAGS) -c src/runtime.c -o $@

$(BUILD)/panel.o: sim/panel.c sim/panel.h | $(BUILD)
	$(CC) $(CFLAGS) -c sim/panel.c -o $@

$(BUILD)/input.o: sim/input.c sim/input.h sim/panel.h sim/browser.h include/hub75.h | $(BUILD)
	$(CC) $(CFLAGS) $(SDL_CFLAGS) -c sim/input.c -o $@

$(BUILD)/ppm.o: sim/ppm.c sim/ppm.h | $(BUILD)
	$(CC) $(CFLAGS) -c sim/ppm.c -o $@

$(BUILD)/main-$(SKETCH).o: sim/main.c include/hub75.h src/graphics.h src/runtime.h sim/panel.h sim/ppm.h sim/window.h sim/browser.h | $(BUILD)
	$(CC) $(CFLAGS) -DHUB75_SKETCH_NAME=\"$(SKETCH)\" -c sim/main.c -o $@

$(BUILD)/window.o: sim/window.c sim/window.h sim/panel.h sim/input.h include/hub75.h src/graphics.h sim/browser.h sim/font5x7.h | $(BUILD)
	$(CC) $(CFLAGS) $(SDL_CFLAGS) -c sim/window.c -o $@

$(BUILD)/browser.o: sim/browser.c sim/browser.h include/hub75.h src/graphics.h sim/font5x7.h | $(BUILD)
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

$(BUILD)/sk-%.o: sketches/%.c include/hub75.h $(UTIL_HEADERS) | $(BUILD)
	$(CC) $(CFLAGS) -Dsetup=$(subst -,_,$*)_setup -Ddraw=$(subst -,_,$*)_draw -c $< -o $@

firmware: check-sketch
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
	stage="$(BUILD)/firmware-sketch"; \
	rm -rf "$$stage"; \
	mkdir -p "$$stage"; \
	cp include/hub75.h include/util.h include/sprites.h src/graphics.h src/graphics.c "$$stage/"; \
	mkdir -p "$$stage/util"; \
	cp include/util/*.h "$$stage/util/"; \
	cp firmware/pins.h firmware/portal.h firmware/portal.cpp "$$stage/"; \
	cp firmware/runtime_fw.c firmware/glue.c firmware/sketch.ino "$$stage/"; \
	cp sketches/$(SKETCH).c "$$stage/sketch_user.inc"; \
	printf '#pragma once\n#define HUB75_PANEL_WIDTH %s\n#define HUB75_PANEL_HEIGHT %s\n' \
		"$(WIDTH)" "$(HEIGHT)" > "$$stage/config.h"; \
	echo "compiling $(SKETCH) for $$fqbn ($(WIDTH)x$(HEIGHT))"; \
	"$$cli" compile --fqbn "$$fqbn" --output-dir "$(BUILD)/firmware-out" "$$stage"

flash: firmware
	@if [ -z "$(PORT)" ]; then \
		echo "error: set PORT to the Matrix Portal's serial device."; \
		echo "  arduino-cli board list"; \
		echo "  make flash PORT=/dev/cu.usbmodem1101 SKETCH=$(SKETCH)"; \
		exit 1; \
	fi
	@"$(ARDUINO_CLI)" upload -p "$(PORT)" --fqbn "$(FQBN)" --input-dir "$(BUILD)/firmware-out"

# CH552 + 8x8 WS2812 matrix (ch55xduino). Separate core, separate stage;
# the ESP32 and simulator targets are untouched.
#
# The CH552 has 14 KB flash and 876 B external RAM, so the build drops the
# float shape helpers (triangle/polygon) with HUB75_NO_FLOAT_SHAPES. That
# fits the integer-only sketches; a sketch that calls triangle, polygon, or
# libm float functions will not link or will overflow.
firmware-ch552: check-sketch
	@cli="$(ARDUINO_CLI)"; \
	fqbn="$(CH55X_FQBN)"; \
	if [ ! -x "$$cli" ]; then \
		echo "error: arduino-cli not found."; \
		echo "The simulator does not need it. Build that with: make sim"; \
		echo ""; \
		echo "To build CH552 firmware, install Arduino CLI and then:"; \
		echo "  arduino-cli config init"; \
		echo "  arduino-cli config add board_manager.additional_urls $(CH55X_URL)"; \
		echo "  arduino-cli core update-index"; \
		echo "  arduino-cli core install $(CH55X_CORE)"; \
		echo "  make firmware-ch552 SKETCH=$(SKETCH)"; \
		exit 1; \
	fi; \
	if ! "$$cli" core list 2>/dev/null | grep -q "$(CH55X_CORE)"; then \
		echo "error: core $(CH55X_CORE) is not installed."; \
		echo "The Matrix Portal S3 and the simulator are unaffected."; \
		echo ""; \
		echo "  arduino-cli config add board_manager.additional_urls $(CH55X_URL)"; \
		echo "  arduino-cli core update-index"; \
		echo "  arduino-cli core install $(CH55X_CORE)"; \
		echo "  make firmware-ch552 SKETCH=$(SKETCH)"; \
		exit 1; \
	fi; \
	stage="$(BUILD)/ch552-sketch"; \
	rm -rf "$$stage"; \
	mkdir -p "$$stage"; \
	cp include/hub75.h include/util.h include/sprites.h src/graphics.h src/graphics.c "$$stage/"; \
	mkdir -p "$$stage/util"; \
	cp include/util/*.h "$$stage/util/"; \
	cp firmware/runtime_fw.c "$$stage/"; \
	cp firmware/ch552/matrix.h firmware/ch552/matrix.c firmware/ch552/buttons.c firmware/ch552/glue.c firmware/ch552/alloc.c "$$stage/"; \
	cp firmware/ch552/sketch.ino "$$stage/ch552-sketch.ino"; \
	cp sketches/$(SKETCH).c "$$stage/sketch_user.h"; \
	printf '#pragma once\n#define HUB75_PANEL_WIDTH %s\n#define HUB75_PANEL_HEIGHT %s\n' \
		"$(CH552_WIDTH)" "$(CH552_HEIGHT)" > "$$stage/config.h"; \
	echo "compiling $(SKETCH) for $$fqbn ($(CH552_WIDTH)x$(CH552_HEIGHT))"; \
	"$$cli" compile --fqbn "$$fqbn" \
		--build-property "compiler.c.extra_flags=-DHUB75_NO_FLOAT_SHAPES" \
		--build-property "compiler.cpp.extra_flags=-DHUB75_NO_FLOAT_SHAPES" \
		--output-dir "$(BUILD)/ch552-out" "$$stage"

flash-ch552: firmware-ch552
	@if [ -z "$(PORT)" ]; then \
		echo "error: set PORT to the CH552's serial device."; \
		echo "  arduino-cli board list"; \
		echo "  make flash-ch552 PORT=/dev/cu.usbmodem1101 SKETCH=$(SKETCH)"; \
		exit 1; \
	fi
	@"$(ARDUINO_CLI)" upload -p "$(PORT)" --fqbn "$(CH55X_FQBN)" --input-dir "$(BUILD)/ch552-out"

clean:
	rm -rf $(BUILD)
