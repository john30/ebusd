## Transfer speed

In order to compensate potential overhead of transfer encoding, the transfer speed is set to 9600 Baud with 8 bits, no parity, and 1 stop bit.


## Protocol
Data bytes with a value below 0x80 can be transferred as is.

Data bytes with value above or equal to 0x80 are split up into two bytes, each one with the highest bit set to 1.
The second bit indicates whether it is the first or second byte of a split transfer. This way protocol errors can easily be detected.
The bits in the two bytes look like this:

```
first    second
76543210 76543210
11ccccdd 10dddddd
```
4 bits in `c` are used for indicating a special purpose and is set to one of the command request/response symbols as stated below.  
8 bits in `d` are the data byte to be transferred (might also be unused).

### Command request/response symbols

#### from ebusd to device
 * initialization request  
   `<INIT> <features>`  
   Requests an initialization of the device and requests special features in the data byte (tbd).
 * send data request  
   `<SEND> <data>`  
   Requests the specified data byte in `d` to be sent to the eBUS.  
   For data byte values <0x80, the short form without the `<SEND>` prefix is allowed as well.
 * arbitration start request  
   `<START> <master>`  
   Requests the start of the arbitration process after the next received `<SYN>` symbol with the specified master address in `d`.

#### from device to ebusd
  * initialization response  
    `<RESETTED> <features>`  
    Indicates a reboot or an initial ebusd connection on the device and is expected to be returned after an `<INIT`> request.  
    The data byte `d` indicates availability of certain features (like full message sending instead of arbitration only, tbd).
  * receive data notification  
    `<RECEIVED> <data>`  
    Indicates that the specified data byte in `d` was received from the eBUS.  
    For data byte values <0x80, the short form without the `<RECEIVED>` prefix is allowed as well.
  * arbitration start succeeded
    `<STARTED> <info>`  
    Indicates the the last arbitration request succeeded (arbitration was won).  
    The data byte in `d` may contain additional information (tbd).
  * arbitration start failed  
    `<FAILED> <master>`  
    Indicates that the last arbitration request failed (arbitration was lost or sending failed).  
    The data byte in `d` contains the address of the master that won the arbitration.


## Symbols

These are the predefined symbols as used above.

### Bus symbols
 * SYN 0xAA

### Command request symbols (from ebusd to device)
 * INIT 0x0
 * SEND 0x1
 * START 0x2

### Command response symbols (from device to ebusd)
 * RESETTED 0x0
 * RECEIVED 0x1
 * STARTED 0x2
 * FAILED 0xa
