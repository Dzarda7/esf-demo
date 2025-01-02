# ESP-Serial-Flasher Demo

The goal of this demo is to show a possible use case of [esp-serial-flasher](https://github.com/espressif/esp-serial-flasher) library which is used for flashing Espressif SoCs from other host microcontrollers. [M5stack Dial](https://docs.m5stack.com/en/core/M5Dial) devkit is used which enables easy binary selection for the target and shows the progress of flashing. This demo also uses an SD card as a binary carrier for targets. Flashing utilizes a USB CDC ACM interface, so only some of the Espressif SoCs can be used for now.

![ESF Demo](images/esf-demo.gif)

## How to use the demo

The demo checks if SD card is present in the slot. It also checks if target device is connected. When both are present, the selector screen shows up. The app for flashing can be selected using knob and after pressing the knob, flashing starts.

> [!NOTE]
> If you put the target into Download mode differently than using the demo (DTR and RTS USB lines), the demo cannot start the app after flashing.

### Hardware required

* [M5Stack Dial](https://docs.m5stack.com/en/core/M5Dial)
* Micro SD Card Adapter
* FAT formatted SD Card
* Power supply 6 ~ 36 V
* USB C - C cable
* Espressif SoC as a target device

### Wiring

[M5Stack Dial](https://docs.m5stack.com/en/core/M5Dial) has two connectors which are used for connection to the Micro SD Card Adapter.

| Dial Pin | SD Card Pin |
|:--------:|:-----------:|
| 13       | MISO        |
| 15       | MOSI        |
| 1        | CLK         |
| 2        | CD          |

> [!NOTE]
> If SD Card Adapter does not have pull down resistor on CS pin, it needs to be added.

### Build and Flash

To run the example, type the following command:

```CMake
idf.py -p PORT flash monitor
```

(To exit the serial monitor, type ``Ctrl-]``.)

See the [Getting Started Guide](https://docs.espressif.com/projects/esp-idf/en/stable/esp32/index.html) for full steps to configure and use ESP-IDF to build projects.

### Binaries name requirements

The demo shows all the first level directories on the display. Every directory can be selected. The demo then looks for three file names in the directory - bootloader.bin, partition-table.bin, app.bin. These names are mandatory, other files will be ignored.

All these files can be found in build folder of every ESP-IDF project. Arduino IDE also creates these files when building for Espressif SoCs, so projects from it can also be used. USB Mode needs to be changed to Hardware CDC and JTAG in Arduino IDE, otherwise the demo will not be able to start the app after flashing.
