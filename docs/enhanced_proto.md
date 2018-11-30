## Protocol

Every communication in either direction is prefixed with at most one byte indicating the kind of action/result.

### from ebusd to device
 * initialization (after connect)  
   `<INIT>`  
   This is not yet implemented in ebusd and supposed to be enhanced by another byte requesting certain features.
 * send request  
   `<SEND> <symbol>`  
   Requests the specified symbol to be sent to the eBUS.
 * arbitration start request  
   `<START> <master address>`  
   Requests the start of the arbitration process with the specified master address after the next received `<SYN>` symbol.

### from device to ebusd
  * initialization (after re-connect)  
    `<RESETTED>`  
    Indicates a reboot or an initial ebusd connection on the device.  
    This is actually desired to be enhanced by another byte in future indicating availability of certain features (like full message sending instead of arbitration onnly).
  * symbol received from eBUS  
    `<RECEIVED> <symbol>`  
    Indicates that the specified symbol was received from the eBUS.
  * arbitration start succeeded  
    `<STARTED>`  
    Indicates that the last arbitration request was successful (arbitration was won).
  * arbitration start failed  
    `<FAILED>`  
    Indicates that the last arbitration request failed (arbitration was lost or sending failed).


## Symbols

These are the predefined symbols as used above.

### Generic
 * SYN 0xAA

### From ebusd to device
 * INIT 0x00
 * SEND 0x01
 * START 0x02

### From device to ebusd
 * RESETTED 0x00
 * RECEIVED 0x01
 * STARTED 0x02
 * FAILED 0x82


