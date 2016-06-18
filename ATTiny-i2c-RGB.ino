//Accept control requests over I2C and use them to display on a RGB LED.

//We start with the TinyWireS library. Basicaly using the onboard USI for I2C communications as a slave.
//With a git submodule here, we can ensure we have the exact version we want without including everything in our code
// https://github.com/rambo/TinyWire
//  A lot is from the examples there - why re-invent the wheel?
#define I2C_SLAVE_ADDRESS 0x52 // the 7-bit address: 'R' in ASCII - because RGB would not fit - //TODO: make this configurable if possible
//#define I2C_SLAVE_ADDRESS 0x4
#include "TinyWire/TinyWireS/TinyWireS.h"
// The default buffer size, Can't recall the scope of defines right now
#ifndef TWI_RX_BUFFER_SIZE
#define TWI_RX_BUFFER_SIZE ( 16 )
#endif

//define the pins
#define RED   1
#define GREEN 3
#define BLUE  4

volatile uint8_t i2c_regs[] =
{
    0xFF,         //Red value
    0x7F,         //Green value
    0x00,         //Blue value
    0x11,         //Unused, but I plan to eventually, gamma or mode
};
// Tracks the current register pointer position
volatile byte reg_position;
const byte reg_size = sizeof(i2c_regs);

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
 * The I2C data received -handler
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


void setup()
{
    // TODO: Tri-state this and wait for input voltage to stabilize 
    //pinMode(3, OUTPUT); // OC1B-, Arduino pin 3, ADC
    //digitalWrite(3, LOW); // Note that this makes the led turn on, it's wire this way to allow for the voltage sensing above.

    //pinMode(1, OUTPUT); // OC1A, also The only HW-PWM -pin supported by the tiny core analogWrite
    pinMode(RED, OUTPUT);
    pinMode(GREEN, OUTPUT);
    pinMode(BLUE, OUTPUT);

    /**
     * Reminder: taking care of pull-ups is the masters job
     */

    TinyWireS.begin(I2C_SLAVE_ADDRESS);
    TinyWireS.onReceive(receiveEvent);
    TinyWireS.onRequest(requestEvent);

    
    // Whatever other setup routines ?
    
    analogWrite(RED, 0); //Red
    analogWrite(GREEN, 0); //Blue
    analogWrite(BLUE, 0); //Green
}

void loop()
{
    analogWrite(RED, i2c_regs[0]); //Red
    analogWrite(GREEN, i2c_regs[2]); //Blue
    analogWrite(BLUE, i2c_regs[1]); //Green
    /**
     * This is the only way we can detect stop condition (http://www.avrfreaks.net/index.php?name=PNphpBB2&file=viewtopic&p=984716&sid=82e9dc7299a8243b86cf7969dd41b5b5#984716)
     * it needs to be called in a very tight loop in order not to miss any (REMINDER: Do *not* use delay() anywhere, use tws_delay() instead).
     * It will call the function registered via TinyWireS.onReceive(); if there is data in the buffer on stop.
     */
    TinyWireS_stop_check();
}
