# lsusb-rp2040

This project is an attempt to reproduce the output of linux `lsusb -v` command using a rp2040.

It is a superset of the [Pico-PIO-USB device_info example](https://github.com/sekigon-gonnoc/Pico-PIO-USB/blob/main/examples/arduino/device_info/device_info.ino).

## Setup

- Open Arduino IDE and select your device from the [arduino-pico](https://github.com/earlephilhower/arduino-pico) boards list
- Open this project and edit `HOST_PIN_DP` value in `lsusb.ino` to match your D+/D- pins
- From the tools menu, select `240MHz` for CPU Speed, and `Adafruit TinyUSB` for USB Stack, then flash the rp2040

## Limitations

- Only HID/CDC/AUDIO have named attributes, other device classes have generic attributes and may be missing details
- At the time of writing this, TinyUSB host still fails manage to negociate with USB-LS devices although it claims supporting them.

## Dependencies

- https://github.com/earlephilhower/arduino-pico
- https://github.com/sekigon-gonnoc/Pico-PIO-USB
- https://github.com/adafruit/Adafruit_TinyUSB_Arduino (a port of [TinyUSB](https://github.com/hathach/tinyusb))

## Optional tool

Since the vid/pid list is constantly changing, a tool is provided to re-generate the C code used by lsusb-rp2040 to populate device details.

- Download a fresh copy of [usb.ids](http://www.linux-usb.org/usb.ids) file into the `usb.org` folder.
- Run `gen.py` from its location

## Roadmap

- Implement missing device classes
- Host <=> Device forwarding

## Resources

- https://github.com/gregkh/usbutils
- http://www.linux-usb.org/usb.ids

## Credits/Thanks

- [@joelsernamoreno](https://github.com/joelsernamoreno)
- [@chegewara](https://github.com/chegewara)
