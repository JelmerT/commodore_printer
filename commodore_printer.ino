/*
 * Brother HR-5C thermal matrix printer driver for Arduino
 *
 *                 Adaptation by Jelmer Tiete 21/11/14
 *                     for arduino leonardo and
 *              removal of the external buffer and inverter
 *
 *                    Version 1.0, 11/05/2009
 *                    tapani (at) rantakokko.net
 *               (c) Copyright 2009 Tapani Rantakokko
 *
 *
 * Commodore 64 microcomputer used to be very popular in its own time.
 * Many kinds of peripheral devices were made for it, including printers.
 * Printers were connected to C-64's non-standard serial port, just like
 * more common floppy disk drives (remember the famous 1541?).
 *
 * This module provides an API for using Commodore 64's printers from
 * Arduino. It has been developed and tested with Brother HR-5C thermal
 * matrix printer and MPS 803, but should work (perhaps after some adjustments) with
 * other Commodore 64 compatible printers too.
 *
 *
 * Happy hacking!
 *
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 1 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */
 
 // pinnout CBM-64 serial port:
 // 1  n/c
 // 2  GND
 // 3  CBM_ATN_OUT
 // 4  CBM_CLK_OUT
 // 5  CBM_DATA_OUT & CBM_DATA_IN
 // 6  CBM_RESET_OUT

// CONSTANTS

// Mapping of CBM-64's serial port lines to Arduino's digital I/O pins.
// With Brother HR-5C, we need to be able to read only the data line.
int CBM_ATN_OUT    = 2;     //  A_up
int CBM_RESET_OUT  = 3;     //  A_dn
int CBM_CLK_OUT    = 4;     //  B_up
int CBM_DATA_IN    = 7;     //  B_dn
int CBM_DATA_OUT   = 8;     //  A_strt

// Brother HR-5C printer modes (graphic|business, different char set).
int CMB_PRN_MODE_GRAPHIC = 0;
int CBM_PRN_MODE_BUSINESS = 7;

// CBM printer device addresses.
int CBM_PRN_ADDR   = 4;      // Typically CBM printer is device 4 or 5.
int CBM_PRN_ADDR_2 = CBM_PRN_MODE_BUSINESS; // 2nd addr = printer mode.

// For testing:
int TEST_MODE = -1;
const int DATA_MAX_LENGTH = 10;
char data[DATA_MAX_LENGTH];
int index = -1;

// FUNCTIONS

// Resets CBM peripheral device.
void cbm_reset_device()
{
  digitalWrite(CBM_RESET_OUT, LOW);
  delay(100); // 100 ms
  digitalWrite(CBM_RESET_OUT, HIGH);
  delay(3000); // 3 seconds
}

// Initializes CBM peripheral device.
void cbm_init_device()
{
  // Set I/O lines to idle/inactive.
  // CBM serial lines are active low, but we have an inverter chip!
  digitalWrite(CBM_RESET_OUT, HIGH);
  digitalWrite(CBM_ATN_OUT,   HIGH);
  digitalWrite(CBM_CLK_OUT,   HIGH);
  digitalWrite(CBM_DATA_OUT,  HIGH);

  // Reset device.
  cbm_reset_device();
}

// Returns 1 if device is ready, and 0 if device is busy.
int cbm_device_ready()
{
  // Note: DATA in is *not* inverted by HW.
  return digitalRead(CBM_DATA_IN);
}

// Writes one bit (data byte's LSB) to CBM serial line.
void cbm_serial_write_bit(unsigned char data)
{
  digitalWrite(CBM_DATA_OUT, (data) & 0x01);
  delayMicroseconds(20);
  digitalWrite(CBM_CLK_OUT, LOW);
  delayMicroseconds(20);
  digitalWrite(CBM_CLK_OUT, HIGH);
  delayMicroseconds(20);
}

// Writes one byte to CBM serial line (LSB first, MSB last).
void cbm_serial_write_byte(unsigned char data)
{
  cbm_serial_write_bit( data       & 0x01); // 1st bit (LSB)
  cbm_serial_write_bit((data >> 1) & 0x01); // 2nd bit
  cbm_serial_write_bit((data >> 2) & 0x01); // 3rd bit
  cbm_serial_write_bit((data >> 3) & 0x01); // 4th bit
  cbm_serial_write_bit((data >> 4) & 0x01); // 5th bit
  cbm_serial_write_bit((data >> 5) & 0x01); // 6th bit
  cbm_serial_write_bit((data >> 6) & 0x01); // 7th bit
  cbm_serial_write_bit((data >> 7) & 0x01); // 8th bit (MSB)
}

// Writes one data frame to CBM serial line.
void cbm_serial_write_frame(unsigned char data, int last_frame)
{
  // Begin new frame.
  digitalWrite(CBM_CLK_OUT, HIGH);

  // Device sets DATA high when ready to receive data (not busy).
  while(!cbm_device_ready()) delayMicroseconds(10);

  // Last frame recognition
  if (last_frame == 1)
  {
    // TODO is this even needed?
/*   
    delayMicroseconds(250); // <250us, this IS the last byte
    // ... wait until printer sets data low...
    while(cbm_device_ready) {
      delayMicroseconds(10);
    }
    // ... wait until printer sets data high...
    while(!cbm_device_ready()) {
      delayMicroseconds(10);
    }
    delayMicroseconds(20);
*/   
  }
  else
  {
    delayMicroseconds(40); // <200us, this is not the last byte
  }

  // Write the actual data byte.
  cbm_serial_write_byte(data);

  // End frame.
  digitalWrite(CBM_CLK_OUT, LOW);
  digitalWrite(CBM_DATA_OUT, HIGH); // DATA high (inverted)
  delayMicroseconds(20);

  // Device sets DATA low when it begins processing the data (busy).
  while(cbm_device_ready()) delayMicroseconds(10);

  // Delay between frames.
  delayMicroseconds(100);
}

// Begins communication sequence on CBM serial line.
void cbm_serial_begin()
{
  // Header begins (device acks by setting DATA low).
  digitalWrite(CBM_ATN_OUT, LOW);
  delayMicroseconds(2000); // 2 ms
  digitalWrite(CBM_CLK_OUT, LOW);
  delayMicroseconds(2000); // 2 ms

  // Write listener address.
  cbm_serial_write_frame((unsigned char)(0x20 + CBM_PRN_ADDR), 0);

  // Write secondary address.
  cbm_serial_write_frame((unsigned char)(0x60 + CBM_PRN_ADDR_2), 0);

  // Header ends.
  delayMicroseconds(20);
  digitalWrite(CBM_ATN_OUT, HIGH); // ATN inactive
}

/*
// Ends communication sequence on CBM serial line.
void cbm_serial_end()
{
  //TODO is this even needed?

  // End communication
  digitalWrite(CBM_ATN_OUT, HIGH);
  delayMicroseconds(20);

   // Write unlisten command.
  cbm_serial_write_frame((char)(0x3F), 0);

  digitalWrite(CBM_ATN_OUT, LOW);
  delayMicroseconds(100);
  digitalWrite(CBM_CLK_OUT, LOW);
}
*/

// Switches case for alpha values (they must be reversed).
int cbm_switch_case(char data)
{
  if (data >= 0x41 && data <= 0x5A)
  { // Convert from lower to upper (a->A)
    return data + 32;
  }
  else if (data >= 0x61 && data <= 0x7A)
  { // Convert from upper to lower (A->a)
    return data - 32;
  }
  else
  { // Not an alpha value, no conversion needed
    return data;
  }
}

// Prints text with a CBM-64 compatible printer.
void cbm_print(char* text, int length)
{
  for (int i=0; i<length; i++)
  {
    cbm_serial_write_frame(cbm_switch_case(text[i]), 0);
  }
}

// Prints a line of text with a CBM-64 compatible printer.
void cbm_println(char* text)
{
  int i=0;
  while(text[i] != '\0')
  {
    cbm_serial_write_frame(cbm_switch_case(text[i]), 0);
    i++;
  }
  cbm_serial_write_frame(13, 0); // new line
}

// Prints a self test text.
// TODO: When your printer is working, comment this out to save memory.
void cbm_print_self_test()
{
  char buf[64] = "Hello World, here are the chars supported by the printer:\0";
  cbm_println(buf);
  for (int i=0; i<16; i++)
  {
    cbm_serial_write_frame(i+32, 0);
    cbm_serial_write_frame(32, 0);
    cbm_serial_write_frame(i+48, 0);
    cbm_serial_write_frame(32, 0);
    cbm_serial_write_frame(i+64, 0);
    cbm_serial_write_frame(32, 0);
    cbm_serial_write_frame(i+80, 0);
    cbm_serial_write_frame(32, 0);
    cbm_serial_write_frame(i+96, 0);
    cbm_serial_write_frame(32, 0);
    cbm_serial_write_frame(i+112, 0);
    cbm_serial_write_frame(32, 0);
    cbm_serial_write_frame(32, 0);
    cbm_serial_write_frame(32, 0);
    cbm_serial_write_frame(32, 0);
    cbm_serial_write_frame(i+160, 0);
    cbm_serial_write_frame(32, 0);
    cbm_serial_write_frame(i+176, 0);
    cbm_serial_write_frame(32, 0);
    cbm_serial_write_frame(i+192, 0);
    cbm_serial_write_frame(32, 0);
    cbm_serial_write_frame(i+200, 0);
    cbm_serial_write_frame(32, 0);
    cbm_serial_write_frame(i+224, 0);
    cbm_serial_write_frame(32, 0);
    cbm_serial_write_frame(i+240, 0);
    cbm_serial_write_frame(13, 0);
  }
}

// For testing, print menu to computer.
void test_menu()
{
  Serial.println("Select printer test mode:");
  Serial.println(" (1) Print self test (charset)");
  Serial.println(" (2) Print user message");
  Serial.print("Your selection: > ");
}

// Arduino setup function is run once when the sketch starts.
void setup()
{
  // Set pins to either input or output.
  pinMode(CBM_RESET_OUT, OUTPUT);
  pinMode(CBM_ATN_OUT,   OUTPUT);
  pinMode(CBM_CLK_OUT,   OUTPUT);
  pinMode(CBM_DATA_OUT,  OUTPUT);
  pinMode(CBM_DATA_IN,   INPUT);

  // Initialize printer.
  cbm_init_device();

  // Prepare for printing.
  cbm_serial_begin();

  // Begin serial communication with PC.
  Serial.begin(9600);
  while (!Serial) {
    ; // wait for serial port to connect. Needed for Leonardo only
  }
  // Print test menu.
  test_menu();
}

// Arduino loop function is run over and over again, forever.
void loop()
{
  char val;

  // Check if data has been sent from the computer.
  if (Serial.available())
  {
    // Read the most recent byte (which will be from 0 to 255).
    val = Serial.read();
    Serial.println(val);

    if (TEST_MODE <= 0)
      { // Set test mode
      if (val == '1') TEST_MODE = 1;
      else if (val == '2') TEST_MODE = 2;
      }

    if (TEST_MODE == 1)
    { // If self test mode selected, do self test now.
      Serial.println("Now printing...");
      cbm_print_self_test();
      Serial.println("Done.");
      TEST_MODE = 0;
      test_menu();
    }
    else if (TEST_MODE == 2)
    {
      if (index == -1)
      { // If user message test mode selected, ask it now.
        Serial.println("Type text to be printed (# ends):");
        index++;
      }
      else
      {
        if ( val == '#' )
        {
          data[index] = '\0';
          cbm_println(data);
          Serial.println("Done.");
          index = -1;
          TEST_MODE = 0;
          test_menu();
        }
        else if (index < DATA_MAX_LENGTH)
        {
          data[index] = val;
          index++;
          if (index + 1 >= DATA_MAX_LENGTH)
          {
            data[index] = '\0';
            cbm_print(data, DATA_MAX_LENGTH);
            index = 0;
          }
        }
      }
    }
  }
}
