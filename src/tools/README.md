eBUS Adapter 3 PIC Loader
=========================

This is a tool for loading new firmware to the
[eBUS adapter 3 PIC](https://adapter.ebusd.eu/)
and to configure IP settings for the Ethernet variant.

All of this is done via the bootloader on the PIC. Consequently, when the
bootloader is not running, this tool can't do anything.
Check the [eBUS adapter 3 documentation](https://adapter.ebusd.eu/picfirmware)
on how to start the bootloader.

This tool was developed due to very unreadable output of the MPLAB bootloader
host application.

Usage
-----
`ebuspicloader --help` reveals the options:
```
Usage: ebuspicloader [OPTION...] PORT
A tool for loading firmware to the eBUS adapter PIC.

  -d, --dhcp                 set IP address to DHCP
  -f, --flash=FILE           flash the FILE to the device
  -i, --ip=IP                set IP address (e.g. 192.168.0.10)
  -m, --mask=MASK            set IP mask (e.g. 24)
  -M, --macip                set the MAC address suffix from the IP address
  -r, --reset                reset the device at the end on success
  -s, --slow                 use low speed for transfer
  -v, --verbose              enable verbose output
  -?, --help                 give this help list
      --usage                give a short usage message
  -V, --version              print program version

PORT is the serial port to use (e.g./dev/ttyUSB0)
```

Flash firmware
--------------
For flashing a new firmware, you would typically do something like this:  
`ebuspicloader -f firmware.hex /dev/ttyUSB0`

On success, the output looks like this:
```
Device ID: 30b0 (PIC16F15356)
Device revision: 0.1
Bootloader version: 1 [0a6c]
Firmware version not found
MAC address: ae:b0:53:26:15:80
IP address: DHCP

New firmware version: 1 [c5e7]
erasing flash: done.
flashing:

0x0400 ................................................................
...
0x2c00 ................................................................
0x3000 .
flashing finished.
flashing succeeded.
```

Configure IP
------------
For changing the IP address of an Ethernet enabled adapter, you would do
something like this:  
`ebuspicloader -i 192.168.1.10 -m 24 /dev/ttyUSB0`

On success, the output looks like this:
```
Device ID: 30b0 (PIC16F15356)
Device revision: 0.1
Bootloader version: 1 [0a6c]
Firmware version: 1 [c5e7]
MAC address: ae:b0:53:26:15:80
IP address: DHCP

Writing IP settings: done.
IP settings changed to:
MAC address: ae:80:53:26:15:80
IP address: 192.168.1.10/24
```
