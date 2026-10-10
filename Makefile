SKETCH ?= drift
WIDTH ?= 128
HEIGHT ?= 64
PITCH ?= P1
ARGS ?=

FQBN ?= esp32:esp32:adafruit_matrixportal_esp32s3
PORT ?=

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

.PHONY: sim firmware flash check-sketch check-sdl clean

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
	cp include/hub75.h include/util.h src/graphics.h src/graphics.c "$$stage/"; \
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

clean:
	rm -rf $(BUILD)
