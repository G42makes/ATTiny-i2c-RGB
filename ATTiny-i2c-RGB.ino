//Accept control requests over I2C and use them to display on a RGB LED.

//We start with the TinyWireS library. Basicaly using the onboard USI for I2C communications as a slave.
//With a git submodule here, we can ensure we have the exact version we want without including everything in our code
// https://github.com/rambo/TinyWire
//  A lot is from the examples there - why re-invent the wheel?
#define I2C_SLAVE_ADDRESS 0x52 // the 7-bit address: 'R' in ASCII - because RGB would not fit - //TODO: make this configurable if possible

#include "TinyWire/TinyWireS/TinyWireS.h"
// The default buffer size, Can't recall the scope of defines right now
#ifndef TWI_RX_BUFFER_SIZE
#define TWI_RX_BUFFER_SIZE ( 16 )
#endif

//We want to remember the last state of our config, so we use the EEPROM library to store data between startups
// Used the examples available in the Arduino IDE for the EEPROM segments
#include <EEPROM.h>
#define EEPROM_REVISION 0x0001  //used to version the data so we can change the array below and have it override whats in EEPROM
#define EEPROM_OFFSET   0x00    //if we want to move the storage around, change this

// In order to do software PWM using interrupts we need a bit of interesting code
// References I used(and copied some code from):
//  - http://jaywiggins.com/2011/05/04/attiny-software-pwm-to-drive-rgb-led/
//      - used some of the structure of this code, plus it helped translate registers below
//  - http://www.engblaze.com/microcontroller-tutorial-avr-and-arduino-timer-interrupts/  
//      - this got it figured out for me, but some registers were different on the ATTiny
//  - http://www.atmel.com/images/atmel-2586-avr-8-bit-microcontroller-attiny25-attiny45-attiny85_datasheet.pdf
//      - I have read every word of this datasheet, I still don't understand it all, but it's a good reference
//        to understand code that someone else has written that is very low level, like the above two
//  - https://blog.blinkenlight.net/experiments/removing-flicker/glowing-bounce/
//      - ended up not using the code from here, but some good ideas for fast software PWM
//  - https://learn.sparkfun.com/tutorials/tiny-avr-programmer-hookup-guide/all
//      - this thing saved me so much work figuring things out
//
// Create a few vars to hold counters used in the RGB PWM
unsigned char compare[3];
volatile unsigned char compbuff[3];

//define the pins
//  i2c DATA  0     //Physical Pin 5
#define RED   1     //Physical Pin 6
//  i2c CLOCK 2     //Physical Pin 7
#define BLUE  3     //Physical Pin 2
#define GREEN 4     //Physical Pin 3
//  power/V+        //Physical Pin 8
//  ground/-        //Physical Pin 4
//  reset           //Physical Pin 1

volatile uint8_t i2c_regs[] =
{
    0x01,         // 0: Mode: 0=off, 1=solid, 2=fade, 3=rainbow loop
    0x20,         // 1: Mode arguments(mode 0/1 = nothing, mode 2 = delay between changes)
    0xAA,         // 2: Red value
    0xFF,         // 3: Green value
    0xFF,         // 4: Blue value
    0x17,         // 5: Should be 0x17 most of the time, changing it triggers special 
                  //      actions and returns to 0x17(arbitrary choice - used to help see 
                  //      the end of the options list before looping)
};
// Tracks the current register pointer position
volatile byte reg_position = 0;
const byte reg_size = sizeof(i2c_regs);

void setup()
{
  // Disable global interrupts before initialization of the Timer
  noInterrupts();   //cli();

  //Set the RGB pins up
  // Reminder: taking care of pull-ups is the masters job
  pinMode(RED,   OUTPUT);
  pinMode(GREEN, OUTPUT);
  pinMode(BLUE,  OUTPUT);

  // Setup as an i2c slave, the ports are fixed in the USI
  //  Pin 7(PB2): SCL - clock
  //  Pin 5(PB0): SDA - data
  TinyWireS.begin(I2C_SLAVE_ADDRESS);
  TinyWireS.onReceive(receiveEvent);
  TinyWireS.onRequest(requestEvent);

  //Set the LED to OFF to start, it will init to match the registers on first interrupt
  digitalWrite(RED,   LOW); //Red
  digitalWrite(GREEN, LOW); //Blue
  digitalWrite(BLUE,  LOW); //Green

  //Initialize Timer1
  //  We use Timer1 because Timer0 is setup in the arduino libraries to run the millis() function
  //  Below we set timer1 to call an ISR(interrupt service routine) whenever it overflows value.
  //   This occurs every 256 timer ticks, because this is an 8 bit timer(see datasheet).
  //   Every cpu cycle at 8MHz is 125ns, times 256 to overflow has us running the ISR every 32us, 
  //    or 31.25KHz. We use 256 cycles of the ISR to do one PWM cycle, so the LEDs flicker around
  //    every 8.2ms, or 122Hz, enough to keep your eyes fooled, but probably not for PoV use.
  TCCR1 = 0;            // set entire TCCR1 register to 0, removes any existing values
  TIMSK = (1 << TOIE1); // enable Timer1 overflow interrupt, calls ISR(TIMER1_OVF_vect) below
  TCCR1 |= (1 << CS10); // Set CS10 bit so timer runs at the cpu clock speed

  //Read the EEPROM values where appropriate
  unsigned int ver;
  EEPROM.get(EEPROM_OFFSET, ver);               //read version stored at the beginning of the EEPROM(w/ offset)
  if ( ver == EEPROM_REVISION ) {
    EEPROM.get(EEPROM_OFFSET + 2, i2c_regs);    //read the settings if the version has not changed
                                                // offset by +2 to give room for REVISION
  } else {
    EEPROM.put(EEPROM_OFFSET, EEPROM_REVISION); //store the current version
    EEPROM.put(EEPROM_OFFSET + 2, i2c_regs);    //and store the array as it currently exists in code
  }

  // Re-enable interrupts before we enter the main loop.
  interrupts();
}

void loop()
{
  switch(i2c_regs[0]) {
   case 0x00:
    //all off
    compbuff[0] = 0;
    compbuff[1] = 0;
    compbuff[2] = 0;
    break;

   case 0x01:
    //all at set value
    compbuff[0] = i2c_regs[2]; //Red
    compbuff[1] = i2c_regs[3]; //Green
    compbuff[2] = i2c_regs[4]; //Blue
    break;

   case 0x02:
    //fade in
    for(int i = 0; i < 256; i++ ) {
      compbuff[0] = i2c_regs[2] * i / 256; //Red
      compbuff[1] = i2c_regs[3] * i / 256; //Green
      compbuff[2] = i2c_regs[4] * i / 256; //Blue
      tws_delay(i2c_regs[1]);
    }
    //fade out
    for(int i = 255; i >= 0; i-- ) {
      compbuff[0] = i2c_regs[2] * i / 256; //Red
      compbuff[1] = i2c_regs[3] * i / 256; //Green
      compbuff[2] = i2c_regs[4] * i / 256; //Blue
      tws_delay(i2c_regs[1]);
    }
    break;
   case 0x03:
    //rotate colors, using RGB to set max intensity on those
    // R-Y-G-C-B-M-R
    //speed is still controlled by regs[1] value
    compbuff[0] = i2c_regs[2];    //Set red full on
    for(int i = 0; i < 256; i++ ) {
      compbuff[1] = i2c_regs[3] * i / 256;  //fade in Green
      tws_delay(i2c_regs[1]);
    }
    for(int i = 255; i >= 0; i-- ) {
      compbuff[0] = i2c_regs[2] * i / 256;  //fade out Red
      tws_delay(i2c_regs[1]);
    }
    for(int i = 0; i < 256; i++ ) {
      compbuff[2] = i2c_regs[4] * i / 256;  //fade in Blue
      tws_delay(i2c_regs[1]);
    }
    for(int i = 255; i >= 0; i-- ) {
      compbuff[1] = i2c_regs[3] * i / 256;  //fade out Green
      tws_delay(i2c_regs[1]);
    }
    for(int i = 0; i < 256; i++ ) {
      compbuff[0] = i2c_regs[2] * i / 256;  //fade in Red
      tws_delay(i2c_regs[1]);
    }
    for(int i = 255; i >= 0; i-- ) {
      compbuff[2] = i2c_regs[4] * i / 256;  //fade out Blue
      tws_delay(i2c_regs[1]);
    }
    //and we are back to Red full on    
    break;

  }

  //Control Byte(last) can be used for other stuff
  // Basically, write 0x57 to the last byte of regs and this should write settings to EEPROM
  //  See README.md
  switch(i2c_regs[sizeof(i2c_regs) - 1]) {
    case 0x52:
      //re-read eeprom
      EEPROM.get(EEPROM_OFFSET + 2, i2c_regs);
      i2c_regs[sizeof(i2c_regs) - 1] = 0x17;    //reset to default value
      break;
    case 0x57:
      //write current regs to eeprom
      EEPROM.put(EEPROM_OFFSET + 2, i2c_regs);
      i2c_regs[sizeof(i2c_regs) - 1] = 0x17;    //reset to default value
      break;

    case 0x17:
    default:
      //if the magic number or non-used value, do nothing
      break;
  }
  
    //From the TinyWireS code:
    //  REMINDER: Do *not* use delay() anywhere, use tws_delay()
    TinyWireS_stop_check();
}

/**
 * Timer1 Overflow ISR
 * This sets the RGB pins on/off in a cycle of 256, allowing us to set multiple brightness values
 * 
 */
ISR(TIMER1_OVF_vect) {
  static unsigned char softcount=0xFF;    //counts how many times we have been in the ISR, resets at 256 times

  //a bit of a programming trick, we increment softcount inside the if statement
  //doing the ++ before the var applies it before anything else is done, even the compare
  //another trick is that the type of softcount will go from FF to 0 automatically here, due to overflow

  // we break this into 3 sections 85 cycles apart(256/3) to help balance power usage between the LEDs
  if(++softcount == 0) { 
  
    //These next three lines store the volatile values from compbuff into compate for internal use
    // It's done outside a for loop or similar to save CPU cycles in this routine
    compare[0] = compbuff[0];
    compare[1] = compbuff[1];
    compare[2] = compbuff[2];
    //toggle the PortB pin0 to HIGH or LOW
    //It would be faster to directly hit the pin, but I'm having issues with that breaking i2c
    //digitalWrite(RED,   HIGH); //Red
    digitalWrite(GREEN, HIGH); //Blue
    digitalWrite(BLUE,  HIGH); //Green
    digitalWrite(RED,   HIGH); //Red
  }
  // clear port pin on compare match (executed on next interrupt)
  if(compare[0] == softcount) digitalWrite(RED, LOW);
  if(compare[1] == softcount) digitalWrite(GREEN, LOW);
  if(compare[2] == softcount) digitalWrite(BLUE, LOW);
}

/**
 * This is called for each read request we receive, never put more than one byte of data (with TinyWireS.send) to the 
 * send-buffer when using this callback
 */
void requestEvent()
{  
    TinyWireS.send(i2c_regs[reg_position]);
    // Increment the reg position on each read, and loop back to zero
    reg_position++;
    if (reg_position >= reg_size)
    {
        reg_position = 0;
    }
}
/**
 * The I2C data received handler
 *
 * This needs to complete before the next incoming transaction (start, data, restart/stop) on the bus does
 * so be quick, set flags for long running tasks to be called from the mainloop instead of running them directly,
 */
void receiveEvent(uint8_t howMany)
{
    if (howMany < 1)
    {
        // Sanity-check
        return;
    }
    if (howMany > TWI_RX_BUFFER_SIZE)
    {
        // Also insane number
        return;
    }

    reg_position = TinyWireS.receive();
    howMany--;
    if (!howMany)
    {
        // This write was only to set the buffer for next read
        return;
    }
    while(howMany--)
    {
        i2c_regs[reg_position] = TinyWireS.receive();
        reg_position++;
        if (reg_position >= reg_size)
        {
            reg_position = 0;
        }
    }
}
