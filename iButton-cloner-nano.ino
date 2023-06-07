/*
 * iButton Cloner for Arduino Nano
 * Reading tested on RW1990 and DS1996
 * Writing only tested on RW1990
 * 
 * The fob has 16 persistent memory slots selected by 4 GPIO pins.
 * 
 * The READ button is for reading fob into current momory slot.
 * Hold the button, the red light means it is ready to read.
 * Tap the fob, 5 green blinks mean the code was memorized.
 * 
 * The WRITE button is for writing the code in the current memory
 * slot to the fob. Before pressing write, firmly press the fob
 * against the contacts. Press WRITE. The red flickering light
 * means the fob is being programmed. NO NOT REMOVE THE FOB.
 * 5 green blinks means programming was successful.
 * 5 red blinks means no code is saved in this memory slot.
 * 
 * Holding both buttons for 3 seconds will clear the current
 * memory slot. The LED will blink red 3 times slowly, then
 * green 3 times quickly after the memory slot is cleared.
 * 
 * The serial console allows reading the memory slot, writing
 * new codes to the slot, and clearing the slot. When setting
 * a memory slot via the serial console, you can set an ASCII
 * name up to 8 characters that will display in the serial
 * console when reading the slot. Clearing the slot either
 * through the console or buttons will erase the name.
 * 
 * Serial details:
 *   Baud rate: 115200
 *   Commands:
 *     R[slot_number]: Read slot number
 *     W[slot_number]: Write slot number
 *     C[slot_number]: Clear slot
 *     D: Dump all slots
 *
 *
 * 
 * 
 *  3.3V
 *    |                   /
 *   <                   /
 *     > 4.7k           /
 *   <                 /
 *     >              /
 *    |              /
 *    |--GPIO       /
 *    |         |-||
 *    |--CENTER(| ||
 *              |-||
 *    |-------SHELL
 *    |
 *   GND
 *   
 *   The serials are 8 bytes long, but the first is always 0C (the family code),
 *   and the last is always a CRC (calculated value). So, only 6 bytes are user-programmable.
 *   Therefore, the serial console only allows the user access to those 6 unique bytes.
 */

#define IBUTTON 8 //iButton center, see above graphic
#define RED 10 //Red LED annode
#define GREEN 2 //Green LED annode
#define READ 3 //Grounding button for reading
#define WRITE 4 //Grounding button for writing
const int SLOT[] = {9, 7, 6, 5}; //pins for slot address, LSB first, grounding switches


#include <EEPROM.h>
#include <OneWire.h>

OneWire ibutton(IBUTTON);

void(* resetFunc) (void) = 0; //declare reset function @ address 0

byte addr[8];
byte code[8];
bool read_pressed, write_pressed;
int serial_choice = -1; //0=read, 1=write, 2=clear, 3=dump, -1=none
byte slot; //only lower nibble is used

void setup(){
  Serial.begin(115200);
  
  pinMode(RED, OUTPUT);
  pinMode(GREEN, OUTPUT);
  pinMode(READ, INPUT_PULLUP); //internal pullup
  pinMode(WRITE, INPUT_PULLUP); //internal pullup

  for(int x = 0; x < 4; x++) {
    pinMode(SLOT[x], INPUT_PULLUP);
  }

  digitalWrite(RED, LOW); //off
  digitalWrite(GREEN, LOW); //off
  
  printWelcome(); //print serial console welcome message
}

void loop(){
  if (Serial.available() > 0) { //if there is data in the serial buffer
    delay(100); //wait for serial to catch up
    byte currChar;
    while(Serial.available() > 0) { //while data is left in the serial buffer
      currChar = Serial.read(); //read (pop) a byte off the serial buffer
      if(serial_choice == -1) { //if so serial state is set to run
        currChar = toupper(currChar); //capitalize
        
        if (currChar == 'R') { //READ command
          serial_choice = 0;
        }
        else if (currChar == 'W') { //WRITE command
          serial_choice = 1;
        }
        else if (currChar == 'C') { //CLEAR command
          serial_choice = 2;
        }
        else if(currChar == 'D') { //DUMP command
          serial_choice = 3;
        }
      }
    }

    clear_serial();

    switch (serial_choice) {
      case 0:
        update_slot();
        if(slot_is_full()) {
          Serial.print("Reading code from device slot ");
          Serial.print(slot, HEX);
          Serial.println("...");
          Serial.print("[SUCCESS] ");
          Serial.print(slot, HEX);
          Serial.print(" : ");
          print_mem_name();
          Serial.print(" : ");
          print_mem(1, 7); //Don't print the first byte (0x0C, family code) or last byte (CRC, a calculated cyclical redundancy check)
          Serial.println("\n");
        }
        else {
          Serial.print("[ERROR] No code stored in slot ");
          Serial.print(slot, HEX);
          Serial.println(" yet.\n");
        }
        serial_choice = -1;
        break;
      case 1:
        update_slot();
        Serial.print("Waiting for code to program slot ");
        Serial.print(slot, HEX);
        Serial.println(" with...");
        Serial.println("Format: 0x01, 0x02, 0x03, 0x04, 0x05, 0x06");
        while(Serial.available() <= 0);
        delay(100);
        while(Serial.available() > 0) {
          String hexstr[6];
          for(int x = 0; x < 6; x++) {
            if(Serial.read() != '0') {
              Serial.println("[ERROR] No leading 0 found (before leading x).\n");
              bad_form();
            }
            if(tolower(Serial.read()) != 'x') {
              Serial.println("[ERROR] No leading x found (before 2 hex digits).\n");
              bad_form();
            }
            hexstr[x] = (char)Serial.read();
            hexstr[x] += (char)Serial.read();
            if(x < 5) {
              if(Serial.read() != ',') {
                Serial.println("[ERROR] No comma found between values (after 2 hex digits).\n");
                bad_form();
              }
              if(Serial.read() != ' ') {
                Serial.println("[ERROR] No space found between values (after comma).\n");
                bad_form();
              }
            }
          }
          int result[6];
          for(int x = 0; x < 6; x++) {
            result[x] = hexstr_to_int(hexstr[x]);
            if(result[x] == -1) {
              Serial.println("[ERROR] Invalid hex character.\n");
              bad_form();
            }
          }

          byte start[7];
          EEPROM.write(0 + (slot << 5), 0x0C);
          start[0] = 0x0C;
          for(int x = 0; x < 6; x++) {
            EEPROM.write(x + (slot << 5) + 1, (byte)result[x]);
            start[x + 1] = (byte)result[x];
          }
          EEPROM.write(7 + (slot << 5), ibutton.crc8(start, 7));
          
          Serial.print("[SUCCESS] Code saved to slot ");
          Serial.print(slot, HEX);
          Serial.print("!\n");
          Serial.println("Waiting for name (up to 8 characters)...");
          while(Serial.available() < 1) delay(1);
          delay(100); //let serial catch up
          for(int x = 0; x < 8; x++) {
            if(Serial.available() < 1) {
              EEPROM.write(x + 8 + (slot << 5), 0x00);
            }
            else {
              EEPROM.write(x + 8 + (slot << 5), Serial.read());
            }
          }
          Serial.println("[SUCCESS] Name saved!\n");
          clear_serial();
        }
        serial_choice = -1;
        break;
      case 2:
        update_slot();
        Serial.print("Clearing slot ");
        Serial.print(slot, HEX);
        Serial.println("...");
        
        for(int x = 0; x < 16; x++) {
          EEPROM.write(x + (slot << 5), 0x00);
        }
          
        Serial.print("[SUCCESS] Slot ");
        Serial.print(slot, HEX);
        Serial.println(" cleared!\n");
        serial_choice = -1;
        break;
      case 3:
        Serial.println("Dumping all slots...");

        for(slot = 0x00; slot < 0x10; slot++) {
           Serial.print(slot, HEX);
           Serial.print(" : ");
           print_mem_name();
           Serial.print(" : ");
           print_mem(1, 7);
           Serial.print("\n");
        }

        Serial.println("[SUCCESS] Done dumping all slots!\n");
        break;
      default:
        serial_choice = -1;
        break;
    }
    printWelcome();
  }
  else {
    read_pressed = !digitalRead(READ);
    write_pressed = !digitalRead(WRITE);
  
    if(read_pressed && write_pressed) {
      for(int x = 0; x < 3; x++) {
        blinkPin(RED, 1, 1000);
        read_pressed = !digitalRead(READ);
        write_pressed = !digitalRead(WRITE);
        if(!read_pressed || !write_pressed) {
          return;
        }
      }
      for(int i = 0; i < 16; i++) {
        EEPROM.write(i + (slot << 5), 0x00);
      }
      blinkPin(GREEN, 3, 250);
    }
    if(read_pressed) {
      update_slot();
      if (!ibutton.search(addr)){//read attached ibutton and asign value to buffer
        digitalWrite(RED, HIGH);
        ibutton.reset_search();
        delay(1);
        digitalWrite(RED, LOW);
        return;
      }
  
      for(int i = 0; i < 8; i++) {
        EEPROM.write(i + (slot << 5), addr[i]);
        code[i] = addr[i];
      }
  
      blinkPin(GREEN, 5, 250);
      while(!digitalRead(READ)) delay(1);
    }
    else if(write_pressed) {
      update_slot();
      if(slot_is_full()) {
        if (!ibutton.search(addr)){//read attached ibutton and asign value to buffer
          digitalWrite(RED, HIGH);
          ibutton.reset_search();
          delay(1);
          digitalWrite(RED, LOW);
          return;
        }
        
        ibutton.skip();       // THESE LINES ARE VITAL
        ibutton.reset();      // I thought they were just for reading, but without these lines,
        ibutton.write(0x33);  // writing will brick the fob forever! I broke 6 writing this program.
        ibutton.skip();
        ibutton.reset();
        ibutton.write(0xD5);
        
        for (byte x = 0; x<8; x++){
          digitalWrite(RED, HIGH);
          delay(5);
          writeByte(code[x]);
          digitalWrite(RED, LOW);
          delay(15);
        }
        
        ibutton.reset();
        blinkPin(GREEN, 5, 250);
        while(!digitalRead(WRITE)) delay(1);
      }
      else {
        blinkPin(RED, 5, 250);
        while(!digitalRead(WRITE)) delay(1);
      }
    }
    else {
      delay(1);
    }
  }
} 

int writeByte(byte data){
  int data_bit;
  for(data_bit=0; data_bit<8; data_bit++){
    if (data & 1){
      digitalWrite(IBUTTON, LOW); pinMode(IBUTTON, OUTPUT);
      delayMicroseconds(60);
      pinMode(IBUTTON, INPUT); digitalWrite(IBUTTON, HIGH);
      delay(10);
    } else {
      digitalWrite(IBUTTON, LOW); pinMode(IBUTTON, OUTPUT);
      pinMode(IBUTTON, INPUT); digitalWrite(IBUTTON, HIGH);
      delay(10);
    }
    data = data >> 1;
  }
  return 0;
}

void blinkPin(int LED, int cycles, int period) {
  for(int i=0; i<cycles; i++) {
    digitalWrite(LED, HIGH);
    delay(period/2);
    digitalWrite(LED, LOW);
    delay(period/2);
  }
}

void printWelcome() {
  Serial.println("iButton Cloner Serial Console");
  Serial.println("Enter 'R' to read current memory slot.");
  Serial.println("Enter 'W' to write to current memory slot.");
  Serial.println("Enter 'C' to clear the current memory slot.");
  Serial.println("Enter 'D' to dump all memory slots.\n");
}

void print_mem(int lower, int upper) {
  if(slot_is_full()) {
    for (byte x = lower; x<upper; x++){  
      byte curr_val = EEPROM.read(x + (slot << 5));
      Serial.print("0x");
      if (curr_val<0x10) {Serial.print("0");}
      Serial.print(curr_val,HEX);
      if (x < upper - 1) Serial.print(", ");
    }
  }
  else {
    Serial.print("<EMPTY SLOT>");
  }
}

void print_mem_name() {
  if(EEPROM.read(0 + 8 + (slot << 5)) == 0x00) {
    Serial.print("<UNNAMED>");
  }
  else {
    for(int x = 0; x < 8; x++) {
      byte value = EEPROM.read(x + 8 + (slot << 5));
      if(value == 0x00) {
        return;
      }
      Serial.print((char)value);
    }
  }
}

int hex_digit_val_dec(char ch)
{
  int returnType;
  switch(ch)
  {
    case '0':
      returnType = 0;
      break;
    case  '1' :
      returnType = 1;
      break;
    case  '2':
      returnType = 2;
      break;
    case  '3':
      returnType = 3;
      break;
    case  '4' :
      returnType = 4;
      break;
    case  '5':
      returnType = 5;
      break;
    case  '6':
      returnType = 6;
      break;
    case  '7':
      returnType = 7;
      break;
    case  '8':
      returnType = 8;
      break;
    case  '9':
      returnType = 9;
      break;
    case  'A':
      returnType = 10;
      break;
    case  'B':
      returnType = 11;
      break;
    case  'C':
      returnType = 12;
      break;
    case  'D':
      returnType = 13;
      break;
    case  'E':
      returnType = 14;
      break;
    case  'F' :
      returnType = 15;
      break;
    default:
      returnType = -1;
      break;
  }
  return returnType;
}

int hexstr_to_int(String hex) {
  int ret = 0;
  if(hex.length() == 2) {
    int temp = hex_digit_val_dec(toupper(hex[0]));

    if(temp == -1) {
      return -1;
    }
    ret += temp * 16;

    temp = hex_digit_val_dec(toupper(hex[1]));
    if(temp == -1) {
      return -1;
    }
    ret += temp;
  }
  else {
    return -1;;
  }
  return ret;
}

void clear_serial() {
  while(Serial.available() > 0) {
    char a = Serial.read();
  }
}

void bad_form() {
  Serial.flush();
  clear_serial();
  resetFunc();
}

void update_slot() {
  slot = 0x00;
  for(int x = 0; x < 4; x++) {
    slot = slot | (digitalRead(SLOT[x]) << x);
  }
  for(int x = 0; x < 8; x++) {
    code[x] = EEPROM.read(x + (slot << 5));
  }
}

bool slot_is_full() {
  byte result = 0x00;
  for(int x = 0; x < 8; x++) {
    result = result | EEPROM.read(x + (slot << 5));
  }
  if(result == 0x00) {
    return false;
  }
  else {
    return true;
  }
}
