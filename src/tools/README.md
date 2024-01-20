eBUS Adapter 3 PIC Loader
=========================

This is a tool for loading new firmware to the
[eBUS adapter 3 PIC](https://adapter.ebusd.eu/v31)
and to configure the variant, IP settings for the Ethernet variant, and other settings.

All of this is done via the bootloader on the PIC. Consequently, when the
bootloader is not running, this tool can't do anything.
Check the [eBUS adapter 3 documentation](https://adapter.ebusd.eu/v31/picfirmware)
on how to start the bootloader.

The binary is part of the [release](https://github.com/john30/ebusd/releases) and a Windows binary based on Cygwin is available for download here:
[ebuspicloader-windows.zip](https://adapter.ebusd.eu/v31/firmware/ebuspicloader-windows.zip)  
It can be started from within Windows `cmd.exe` after extracting the files to a folder and `cd` into that folder.
If Cygwin is already installed, only the `ebuspicloader.exe` needs to be extracted and can be called directly
from within a Cygwin shell.  
In Cygwin, Windows COM ports are mapped under `/dev/ttyS*`, e.g. `COM1` would be `/dev/ttyS0`.

This tool is an alternative to and extension of the MPLAB bootloader host application that produces a lot
of unreadable output.


Usage
-----
`ebuspicloader --help` reveals the options:
```
Usage: ebuspicloader [OPTION...] PORT
A tool for loading firmware to the eBUS adapter PIC and configure adjustable
settings.

 IP options:
  -d, --dhcp                 set dynamic IP address via DHCP (default)
  -g, --gateway=GW           set fix IP gateway to GW (if necessary and other
                             than net address + 1)
  -i, --ip=IP                set fix IP address (e.g. 192.168.0.10)
  -m, --mask=MASK            set fix IP mask (e.g. 24)
  -M, --macip                set the MAC address suffix from the IP address
  -N, --macid                set the MAC address suffix from internal ID
                             (default)

 eBUS options:
  -a, --arbdel=US            set arbitration delay to US microseconds (0-620 in
                             steps of 10, default 200, since firmware
                             20211128)

 PIC options:
  -f, --flash=FILE           flash the FILE to the device
  -o, --pingoff              disable visual ping
  -p, --pingon               enable visual ping (default)
  -r, --reset                reset the device at the end on success
      --variant=VARIANT      set the VARIANT to U=USB/RPI (high-speed), W=WIFI,
                             E=Ethernet, N=non-enhanced USB/RPI/WIFI,
                             F=non-enhanced Ethernet (lowercase to allow
                             hardware jumpers, default "u", since firmware
                             20221206)

 Tool options:
  -s, --slow                 low speed mode for transfer (115kBd instead of
                             921kBd)
  -v, --verbose              enable verbose output

  -?, --help                 give this help list
      --usage                give a short usage message
  -V, --version              print program version

PORT is either the serial port to use (e.g. /dev/ttyACM0) that also supports a
trailing wildcard '*' for testing multiple ports, or a network port as
"ip:port" for use with e.g. socat or ebusd-esp in PIC pass-through mode.
```

Flash firmware
--------------
For flashing a new firmware, you would typically do something like this:  
`ebuspicloader -f firmware.hex /dev/ttyACM0`

On success, the output looks similar to this:
```
Device ID: 30b0 (PIC16F15356)
Device revision: 0.2
Bootloader version: 2 [73c8]
Firmware version not found
MAC address: ae:b0:53:26:15:80
IP address: DHCP (default)
Arbitration delay: 200 us (default)
Visual ping: on (default)
Variant: USB/RPI (high-speed), allow hardware jumpers (default)

New firmware version: 1 [7f16]
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
Changing the IP address of an Ethernet enabled adapter, would be done like this:
`ebuspicloader -i 192.168.1.10 -m 24 /dev/ttyACM0`

On success, the output looks similar to this:
```
Device ID: 30b0 (PIC16F15356)
Device revision: 0.2
Bootloader version: 2 [73c8]
Firmware version: 1 [7f16]
MAC address: ae:b0:53:26:15:80
IP address: DHCP (default)
Arbitration delay: 200 us (default)
Visual ping: on (default)
Variant: Ethernet, ignore hardware jumpers

Writing settings: done.
Settings changed to:
MAC address: ae:80:53:26:15:80
IP address: 192.168.1.10/24
Arbitration delay: 200 us (default)
Visual ping: on (default)
Variant: Ethernet, ignore hardware jumpers
```
