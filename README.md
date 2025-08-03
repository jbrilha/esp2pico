# esp2pico

## Setup

### ESP-IDF

``` bash
mkdir -p esp
cd ~/esp

git clone -b v5.5 --depth=1 --recursive https://github.com/espressif/esp-idf.git
cd esp-idf

./install.sh all

. ./export.sh

# recommended
echo "alias get_idf='. $HOME/esp/esp-idf/export.sh'" >> ~/.bashrc
```

### PICO-SDK

``` bash
mkdir ~/pico
cd ~/pico

git clone --depth=1 https://github.com/raspberrypi/pico-sdk.git --branch master
cd pico-sdk

git submodule update --init

echo "export PICO_SDK_PATH=~/pico/pico-sdk/" >> ~/.bashrc
```

Also recommend installing ```picotool```

### FreeRTOS (to use with pico-sdk)

``` bash
mkdir ~/FreeRTOS/
cd ~/FreeRTOS

git clone --depth=1 https://github.com/FreeRTOS/FreeRTOS.git

echo "export FREERTOS_KERNEL_PATH=~/FreeRTOS/FreeRTOS-Kernel/" >> ~/.bashrc
```

## Building

### For ESP32

``` bash
cd esp32
# get_idf
idf.py set-target esp32 # ors esp32s3, etc
idf.py build
idf.py flash # idf.py -p PORT flash
idf.py monitor # idf.py -p PORT monitor
```

### For Pico W
``` bash
cd pico

mkdir build
cd build
cmake ..
make

picotool load -f src/freertos_test.uf2
```
