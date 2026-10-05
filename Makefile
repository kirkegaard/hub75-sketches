SKETCH ?= drift
WIDTH ?= 128
HEIGHT ?= 64
PITCH ?= P1
ARGS ?=

FQBN ?= esp32:esp32:adafruit_matrixportal_esp32s3
PORT ?=

CFLAGS = -std=c11 -Wall -Wextra -O2 -Iinclude -Isrc -Isim

PKG_CONFIG ?= pkg-config
SDL_CFLAGS := $(shell $(PKG_CONFIG) --cflags sdl2 2>/dev/null)
SDL_LIBS := $(filter-out -lSDL2main,$(shell $(PKG_CONFIG) --libs sdl2 2>/dev/null))
ifeq ($(strip $(SDL_LIBS)),)
SDL_CFLAGS := $(shell sdl2-config --cflags 2>/dev/null)
SDL_LIBS := $(filter-out -lSDL2main,$(shell sdl2-config --libs 2>/dev/null))
endif

BUILD = build
TARGET = $(BUILD)/hub75-$(SKETCH)

ARDUINO_CLI_CANDIDATE := $(shell command -v arduino-cli 2>/dev/null)
ifeq ($(ARDUINO_CLI_CANDIDATE),)
ARDUINO_CLI_CANDIDATE := /Applications/Arduino IDE.app/Contents/Resources/app/lib/backend/resources/arduino-cli
endif
ARDUINO_CLI ?= $(ARDUINO_CLI_CANDIDATE)

PLAYLIST_SHOWS = drift lattice steps cube triangle

ifeq ($(SKETCH),playlist)
PLAY_OBJS = $(foreach s,$(PLAYLIST_SHOWS),$(BUILD)/play-$(s).o)
OBJS = \
	$(BUILD)/graphics.o \
	$(BUILD)/runtime.o \
	$(BUILD)/panel.o \
	$(BUILD)/ppm.o \
	$(BUILD)/main-$(SKETCH).o \
	$(BUILD)/window.o \
	$(BUILD)/playlist.o \
	$(PLAY_OBJS)
else
OBJS = \
	$(BUILD)/graphics.o \
	$(BUILD)/runtime.o \
	$(BUILD)/panel.o \
	$(BUILD)/ppm.o \
	$(BUILD)/main-$(SKETCH).o \
	$(BUILD)/window.o \
	$(BUILD)/$(SKETCH).o
endif

.PHONY: sim firmware flash check-sketch check-sdl clean

sim: check-sketch check-sdl $(TARGET)
	./$(TARGET) --width $(WIDTH) --height $(HEIGHT) --pitch $(PITCH) $(ARGS)

$(TARGET): $(OBJS)
	$(CC) -o $@ $(OBJS) $(SDL_LIBS) -lm

check-sdl:
	@test -n "$(strip $(SDL_LIBS))" || { \
		echo "error: SDL2 not found."; \
		echo "The simulator window uses SDL2, so the same build runs on macOS and Linux."; \
		echo "  brew install sdl2"; \
		echo "  sudo apt install libsdl2-dev"; \
		exit 1; \
	}

check-sketch:
ifeq ($(SKETCH),playlist)
	@missing=0; \
	for s in $(PLAYLIST_SHOWS); do \
		if [ ! -f "sketches/$$s.c" ]; then \
			echo "error: sketches/$$s.c does not exist"; \
			missing=1; \
		fi; \
	done; \
	test "$$missing" -eq 0
else
	@test -f sketches/$(SKETCH).c || { \
		echo "error: sketches/$(SKETCH).c does not exist"; \
		echo "sketches:"; \
		ls sketches/*.c; \
		exit 1; \
	}
endif

$(BUILD):
	mkdir -p $(BUILD)

$(BUILD)/graphics.o: src/graphics.c include/hub75.h src/graphics.h | $(BUILD)
	$(CC) $(CFLAGS) -c src/graphics.c -o $@

$(BUILD)/runtime.o: src/runtime.c src/runtime.h include/hub75.h src/graphics.h sim/window.h | $(BUILD)
	$(CC) $(CFLAGS) -c src/runtime.c -o $@

$(BUILD)/panel.o: sim/panel.c sim/panel.h | $(BUILD)
	$(CC) $(CFLAGS) -c sim/panel.c -o $@

$(BUILD)/ppm.o: sim/ppm.c sim/ppm.h | $(BUILD)
	$(CC) $(CFLAGS) -c sim/ppm.c -o $@

$(BUILD)/main-$(SKETCH).o: sim/main.c include/hub75.h src/graphics.h src/runtime.h sim/panel.h sim/ppm.h sim/window.h | $(BUILD)
	$(CC) $(CFLAGS) -DHUB75_SKETCH_NAME=\"$(SKETCH)\" -c sim/main.c -o $@

$(BUILD)/window.o: sim/window.c sim/window.h sim/panel.h include/hub75.h src/graphics.h | $(BUILD)
	$(CC) $(CFLAGS) $(SDL_CFLAGS) -c sim/window.c -o $@

ifneq ($(SKETCH),playlist)
$(BUILD)/$(SKETCH).o: sketches/$(SKETCH).c include/hub75.h | $(BUILD)
	$(CC) $(CFLAGS) -c sketches/$(SKETCH).c -o $@
endif

$(BUILD)/playlist.o: sim/playlist.c include/hub75.h src/graphics.h | $(BUILD)
	$(CC) $(CFLAGS) -c sim/playlist.c -o $@

$(BUILD)/play-%.o: sketches/%.c include/hub75.h | $(BUILD)
	$(CC) $(CFLAGS) -Dsetup=$*_setup -Ddraw=$*_draw -c $< -o $@

firmware: check-sketch
	@if [ "$(SKETCH)" = "playlist" ]; then \
		echo "error: the playlist links every sketch into the simulator."; \
		echo "  make sim SKETCH=playlist"; \
		exit 1; \
	fi
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
	cp include/hub75.h src/graphics.h src/graphics.c "$$stage/"; \
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
