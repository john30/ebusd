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
   Requests an initialization of the interface and requests special features in the data byte (tbd).
 * send data request  
   `<SEND> <data>`  
   Requests the specified data byte in `d` to be sent to the eBUS.  
   For data byte values <0x80, the short form without the `<SEND>` prefix is allowed as well.
 * arbitration start request  
   `<START> <master>`  
   Requests the start of the arbitration process after the next received `<SYN>` symbol with the specified master address in `d`.

#### from interface to ebusd
  * initialization response  
    `<RESETTED> <features>`  
    Indicates a reboot or an initial ebusd connection on the interface and is expected to be returned after an `<INIT`> request.  
    The data byte `d` indicates availability of certain features (like full message sending instead of arbitration only, tbd).
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

### Command response symbols (from interface to ebusd)
 * RESETTED 0x0
 * RECEIVED 0x1
 * STARTED 0x2
 * FAILED 0xa

### Error codes (from interface to ebusd)
 * ERR_FRAMING 0x00: framing error
 * ERR_OVERRUN 0x00: buffer overrun error


## Examples

### Passive receive
The master-slave data sequence (without SYN, ACK, and CRC) `1008951200 / 0164` when ebusd is only listening to traffic on the bus would usually be transferred as follows (with all extra symbols seen on the bus):

|order|eBUS proto|eBUS byte|sender|enhanced proto|enhanced bytes|
|----:|-----|-----|-----|-----|-----|
|1|`SYN`|0xAA|interface|`<RECEIVED> <0xAA>`|0xC6 0xAA|
|2|`QQ`|0x10|interface|`<0x10>`|0x10|
|3|`ZZ`|0x08|interface|`<0x08>`|0x08|
|4|`PB`|0x95|interface|`<RECEIVED> <0x95>`|0xC6 0x95|
|5|`SB`|0x12|interface|`<0x12>`|0x12|
|6|`NN`|0x00|interface|`<0x00>`|0x00|
|7|`CRC`|0xB1|interface|`<RECEIVED> <0xB1>`|0xC6 0xB1|
|8|`ACK`|0x00|interface|`<0x00>`|0x00|
|9|`NN`|0x01|interface|`<0x01>`|0x01|
|10|`DD`|0x64|interface|`<0x64>`|0x64|
|11|`CRC`|0xFF|interface|`<RECEIVED> <0xFF>`|0xC7 0xBF|
|12|`ACK`|0x00|interface|`<0x00>`|0x00|
|13|`SYN`|0xAA|interface|`<RECEIVED> <0xAA>`|0xC6 0xAA|

### Active successful send
The same data sequence `1008951200 / 0164` when initiated by ebusd as master (with address 0x10) would usually be transferred as follows (with all extra symbols seen on the bus):

|order|eBUS proto|eBUS byte|sender|enhanced proto|enhanced bytes|
|----:|-----|-----|-----|-----|-----|
|1| | |ebusd|`<START> <0x10>`|0xC8 0x90|
|2|`SYN`|0xAA|interface|`<RECEIVED> <0xAA>`|0xC6 0xAA|
|3|`QQ`|0x10|interface|`<STARTED> <0x10>`|0xC8 0x90|
|4|`ZZ`|0x08|ebusd|`<0x08>`|0x08|
|5|`ZZ`|0x08|interface|`<0x08>`|0x08|
|6|`PB`|0x95|ebusd|`<SEND> <0x95>`|0xC6 0x95|
|7|`PB`|0x95|interface|`<RECEIVED> <0x95>`|0xC6 0x95|
|8|`SB`|0x12|ebusd|`<0x12>`|0x12|
|9|`SB`|0x12|interface|`<0x12>`|0x12|
|10|`NN`|0x00|ebusd|`<0x00>`|0x00|
|11|`NN`|0x00|interface|`<0x00>`|0x00|
|12|`CRC`|0xB1|ebusd|`<SEND> <0xB1>`|0xC6 0xB1|
|13|`CRC`|0xB1|interface|`<RECEIVED> <0xB1>`|0xC6 0xB1|
|14|`ACK`|0x00|interface|`<0x00>`|0x00|
|15|`NN`|0x01|interface|`<0x01>`|0x01|
|16|`DD`|0x64|interface|`<0x64>`|0x64|
|17|`CRC`|0xFF|interface|`<RECEIVED> <0xFF>`|0xC7 0xBF|
|18|`ACK`|0x00|ebusd|`<0x00>`|0x00|
|19|`ACK`|0x00|interface|`<0x00>`|0x00|
|20|`SYN`|0xAA|interface|`<RECEIVED> <0xAA>`|0xC6 0xAA|


### Active successful send as SYN generator
The same data sequence `1008951200 / 0164` when initiated by ebusd as master (with address 0x10) and acting as SYN generator would usually be transferred as follows (with all extra symbols seen on the bus):

|order|eBUS proto|eBUS byte|sender|enhanced proto|enhanced bytes|
|----:|-----|-----|-----|-----|-----|
|1| | |ebusd|`<START> <0x10>`|0xC8 0x90|
|2|`SYN`|0xAA|ebusd|`<SEND> <0xAA>`|0xC6 0xAA|
|3|`SYN`|0xAA|interface|`<RECEIVED> <0xAA>`|0xC6 0xAA|
|4|`QQ`|0x10|interface|`<STARTED> <0x10>`|0xC8 0x90|
|...|see above| | | | |
The rest of the communcation is the same as before (from 4.)


### Active failed traffic
A failed arbitration when initiated by ebusd as master (with address 0x10) would usually be transferred as follows (with all extra symbols seen on the bus):

|order|eBUS proto|eBUS byte|sender|enhanced proto|enhanced byte|
|----:|-----|-----|-----|-----|-----|
|1| | |ebusd|`<START> <0x10>`|0xC8 0x90|
|2|`SYN`|0xAA|interface|`<RECEIVED> <0xAA>`|0xC6 0xAA|
|3| |0x10|ebusd|`<FAILED> <0x10>`|0xE0 0x90|
|4|`QQ`|0x03|interface|`<0x03>`|0x03|

