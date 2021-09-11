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

#### from ebusd to interface
 * initialization request  
   `<INIT> <features>`  
   Requests an initialization of the interface and optionally requests special features.  
   The data byte `d` indicates interest in certain features (like full message sending instead of arbitration only).
   The feature bits are defined below in the symbols section.
 * send data request  
   `<SEND> <data>`  
   Requests the specified data byte in `d` to be sent to the eBUS.  
   For data byte values <0x80, the short form without the `<SEND>` prefix is allowed as well.
 * arbitration start request  
   `<START> <master>`  
   Requests the start of the arbitration process after the next received `<SYN>` symbol with the specified master address in `d`.
   If the master address is `<SYN>`, the current arbitration is supposed to be cancelled.
 * information request  
  `<INFO> <info_id>`  
   Requests the transfer of additional info identified by `info_id`.
   The possible `info_id` values are defined below in the symbols section.
   Sending a new info request while the response for the previous one is still in progress immediately terminates the
   transfer of the previous response.

#### from interface to ebusd
  * initialization response  
    `<RESETTED> <features>`  
    Indicates a reboot or an initial ebusd connection on the interface and is expected to be returned after an `<INIT`> request.  
    The data byte `d` indicates availability of certain features (like full message sending instead of arbitration only).
    The feature bits are defined below in the symbols section.
  * receive data notification  
    `<RECEIVED> <data>`  
    Indicates that the specified data byte in `d` was received from the eBUS.  
    For data byte values <0x80, the short form without the `<RECEIVED>` prefix is allowed as well.
    Note that this message shall not be sent when the byte received was part of an arbitration request initiated by ebusd.
  * arbitration start succeeded
    `<STARTED> <master>`  
    Indicates the the last arbitration request succeeded (arbitration was won).  
    The data byte in `d` contains the master address that was sent to eBUS during arbitration.
  * arbitration start failed  
    `<FAILED> <master>`  
    Indicates that the last arbitration request failed (arbitration was lost or sending failed).  
    The data byte in `d` contains the master address that has won the arbitration.
  * information response  
    `<INFO> <data>`  
    Transfers one data byte in response to the INFO request. The first byte transferred in response is the number of
    data bytes to be transferred (excluding the length itself). The format of the data sequence depends on the `info_id`
    value from the request.
    The possible `info_id` values are defined below in the symbols section.
  * eBUS communication error  
    `<ERROR_EBUS> <error>`  
    Indicates an error in the eBUS UART.  
    The data byte in `d` contains the error message.
  * host communication error  
    `<ERROR_HOST> <error>`  
    Indicates an error in the host UART.  
    The data byte in `d` contains the error message.


## Symbols

These are the predefined symbols as used above.

### Bus symbols
 * SYN 0xAA

### Command request symbols (from ebusd to interface)
 * INIT 0x0
 * SEND 0x1
 * START 0x2
 * INFO 0x3

### Command response symbols (from interface to ebusd)
 * RESETTED 0x0
 * RECEIVED 0x1
 * STARTED 0x2
 * INFO 0x3
 * FAILED 0xa

### Error codes (from interface to ebusd)
 * ERR_FRAMING 0x00: framing error
 * ERR_OVERRUN 0x01: buffer overrun error

### Feature bits (both directions)
 * bit 7-1: tbd
 * bit 2: full message sending (complete sequence instead of single bytes)
 * bit 1: high speed transfer at 115200 Bd  
   When requested, the UART speed is changed to 115200 Bd immediately after sending the complete RESETTED reponse.
 * bit 0: additional infos (version, PIC ID, etc.)

### Information IDs (both directions)
The first level below is the `info_id` value and the second level describes the response data byte sequence.
The first byte transferred in response is always the number of data bytes to be transferred (excluding the length itself).
 * 0x00: version  
   * `length`: =2
   * `version`: version number
   * `features`: feature bits
 * 0x01: PIC ID
   * `length`: =9
   * 9*`mui`: PIC MUI
 * 0x02: PIC config
   * `length`: =8
   * 8*`config_H` `config_L`: PIC config
 * 0x03: PIC temperature
   * `length`: =1
   * `temp`: temperature in degrees Celsius
 * 0x04: PIC supply voltage
   * `length`: =2
   * `millivolt_H` `millivolt_L`: voltage value in mV
 * 0x05: bus voltage
   * `length`: =2
   * `voltage_max`: maximum bus voltage in 10th volts
   * `voltage_min`: minimum bus voltage in 10th volts

