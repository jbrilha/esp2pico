.PHONY: all clean pico esp32 pico-clean esp32-clean flash-pico flash-esp32 monitor-esp32

all: pico esp32

PICO_BUILD_DIR := build_pico
ESP32_BUILD_DIR := build_esp32

pico:
	@echo "Building for Pico..."
	@mkdir -p $(PICO_BUILD_DIR)
	cd $(PICO_BUILD_DIR) && cmake -DBUILD_PICO=1 .. && make

esp32:
	@echo "Building for ESP32..."
	idf.py -B $(ESP32_BUILD_DIR) -DBUILD_ESP32=1 build

clean: pico-clean esp32-clean

clean-pico:
	@echo "Cleaning Pico build..."
	rm -rf $(PICO_BUILD_DIR)

clean-esp32:
	@echo "Cleaning ESP32 build..."
	rm -rf $(ESP32_BUILD_DIR)

flash-pico:
	@echo "Flashing Pico..."
	picotool load -f $(PICO_BUILD_DIR)/main/pico32.uf2

flash-esp32:
	@echo "Flashing ESP32..."
	idf.py -B $(ESP32_BUILD_DIR) flash

monitor-esp32:
	@echo "Starting ESP32 monitor..."
	idf.py monitor
