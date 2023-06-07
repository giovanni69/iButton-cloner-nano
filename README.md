iButton Cloner for Arduino Nano
Reading tested on RW1990 and DS1996
Writing only tested on RW1990

The fob has 16 persistent memory slots selected by 4 GPIO pins.

The READ button is for reading fob into current momory slot.
Hold the button, the red light means it is ready to read.
Tap the fob, 5 green blinks mean the code was memorized.

The WRITE button is for writing the code in the current memory
slot to the fob. Before pressing write, firmly press the fob
against the contacts. Press WRITE. The red flickering light
means the fob is being programmed. NO NOT REMOVE THE FOB.
5 green blinks means programming was successful.
5 red blinks means no code is saved in this memory slot.

Holding both buttons for 3 seconds will clear the current
memory slot. The LED will blink red 3 times slowly, then
green 3 times quickly after the memory slot is cleared.

The serial console allows reading the memory slot, writing
new codes to the slot, and clearing the slot. When setting
a memory slot via the serial console, you can set an ASCII
name up to 8 characters that will display in the serial
console when reading the slot. Clearing the slot either
through the console or buttons will erase the name.

Serial details:
Baud rate: 115200
Commands:
R[slot_number]: Read slot number
W[slot_number]: Write slot number
C[slot_number]: Clear slot
D: Dump all slots

 3.3V
  |                   /
 <                   /
   > 4.7k           /
 <                 /
   >              /
  |              /
  |--GPIO       /
  |         |-||
  |--CENTER(| ||
            |-||
  |-------SHELL
  |
 GND
 
The serials are 8 bytes long, but the first is always 0C (the family code),
and the last is always a CRC (calculated value). So, only 6 bytes are user-programmable.
Therefore, the serial console only allows the user access to those 6 unique bytes.
