# ATTiny-i2c-RGB
Via an ATTiny85, control the color and patters of an RGB LED over I2C

## Features
- Seperate Red, Green, and Blue PWM color channels
- I2C slave(default 7bit address: 0x52) to configure
- 3 modes: off, full on, fade in/out

## Using
Wire up an RGB LED with a common cathode(negative) to the pins of the ATTiny85 according to the Fritzing diagram or based on the code. Connect the cathode to ground via a resistor(about 220ohm). Connect the I2C pins(see code or diagram) to your controller, and don't forget you need pull up resistors on both lines from the master(not shown).

Using the I2C protocol, read/write to the memory provided, the positions are documented in the code. All changes take effect immediatly. Generally only one byte can be read/written at a time.

I have used my Bus Pirate for testing, providing power and control. I cannot recommend it highly enough, it's been perfect for what I needed.

At this time, settings are forgotten on power loss, and it returns to default.

## TODO
- Save settings to EEPROM - should be an easy one
- Figure out why Red acts weird and deal with it - I have theories that I might be able to test
- More modes for the LEDs to cycle through
- Configurable address(in EEPROM) - because I can
- Seperate the RGB PWM into it's own library, it's a usefull config and I see a lot of people looking for it, but not many answers on how to get it working

## References
- https://github.com/rambo/TinyWire - I2C slave library
- http://jaywiggins.com/2011/05/04/attiny-software-pwm-to-drive-rgb-led/ - probably the first bit of code that made me believe I could get this working
- http://www.engblaze.com/microcontroller-tutorial-avr-and-arduino-timer-interrupts/ - this got me working, took a little translating, but that's it
- http://www.atmel.com/images/atmel-2586-avr-8-bit-microcontroller-attiny25-attiny45-attiny85_datasheet.pdf basically required reading after playing with ATTiny85's for any time
- https://blog.blinkenlight.net/experiments/removing-flicker/glowing-bounce/ - some good discussion of fast software PWM, not used here, but helped me learn
- https://learn.sparkfun.com/tutorials/tiny-avr-programmer-hookup-guide/all - and this saved me all the time in development