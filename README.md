# ATTiny-i2c-RGB
Via an ATTiny85, control the color and patters of an RGB LED over I2C

## Features
- Seperate Red, Green, and Blue PWM color channels
- I2C slave(default 7bit address: 0x52) to configure
- 4 modes: off, full on, fade in/out, rainbow loop

## Using
Wire up an RGB LED with a common cathode(negative) to the pins of the ATTiny85 according to the Fritzing diagram or based on the code. Connect the cathode to ground via a resistor(about 220ohm). Connect the I2C pins(see code or diagram) to your controller, and don't forget you need pull up resistors on both lines from the master(not shown).

Using the I2C protocol, read/write to the memory provided, the positions are documented in the code. All changes take effect immediatly. Generally only one byte can be read/written at a time.

I have used my Bus Pirate for testing, providing power and control. I cannot recommend it highly enough, it's been perfect for what I needed.

Over i2c write the following byte numbers to make changes:
- Byte 0x00: Mode: Set the mode for the device.
	- Write 0x00: Off - don't actually do anything
	- Write 0x01: Solid On. Use the RGB values and just turn on the light at those intensities
	- Write 0x02: Fade Mode. Fades between off and the RGB values set, speed is controlled by Byte 0x01
		- Write 0x00 to Byte 0x01 to get really fast
		- Write 0xFF to Byte 0x01 to get slowest supported
	- Write 0x03: Rainbow Loop. Fades between R-Y-G-C-B-M-R in a loop. Max intensity of R,G & B are set by further bytes
		- Speed of loop is set in the same way as with Fade (0x02) above.
- Byte 0x01: Mode Argument: Set mode specific values(see modes above)
- Byte 0x02: Red
	- Set Red value of the RGB LED. 0x00 is off, 0xFF is max intensity
	- If you are using only one current limiting resistor, on the cathode(-) terminal of the RGB LED, you will probably find that Red acts weird near max intensity. Dialing back the max value of this register to something like 0xAA usually resolves this with no loss of color ability.
- Byte 0x03: Green
	- Set Green value of the RGB LED. 0x00 is off, 0xFF is max intensity
- Byte 0x04: Blue
	- Set Blue value of the RGB LED. 0x00 is off, 0xFF is max intensity
- Byte 0x05: Control and EOF (or maybe End of Regs?), last value in the settings before reads loop to 0x00
	- Write 0x52(R): Re-read values from EEPROM into the registers and apply them
	- Write 0x57(W): Write current register values to EEPROM to be ready for next boot
	- Once commands are run, value is reset to 0x17
	
Changes are not permanent until you write 0x57 to address 0x05. Then you can pull the power and the device will power back on with the save settings.

## Note
### Red Taking Over
In order to simplify my circuit, I have cheated a bit. Normally there should be current limiting resistors on each of the anode(+) inputs to each of the R, G & B LEDs. Due to how most of these are built, Red generally needs a different resistor then Green and Blue(which often use the same value). So this would need 3 resistors, which take up a lot of space on a project that I wanted to have really small. If space is not an issue, you can go ahead and do this properly. There is a good page on what you need to consider in the References section below. I cheat and just use a single one on the cathode(-) connection, and that leads to some weird(to me, I don't yet understand enough to figure it out) behaviours as mentioned in the registers section for Red above. Mostly it just means don't try to max out the Red value if you want to use others. I have gotten a reasonable white from this setup using values of 0xAAFFFF for my color.

### Why
My design uses this config to replace the power LED on a first generation NES front loader. Just desolder the old Red LED and replace it with this circuit. Before or after installing it can be programmed with new patterns which will load on next power on. If you have another device in your NES that can talk I2C, including the NES itself theoretically, you can adjust it from that as well. ATTiny was chosen to keep the cost and complexity down, but the limited pins led to the fun learning experience that this has been.

## TODO
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
- http://wiring.org.co/learning/basics/rgbled.html - proper wiring of an RGB LED with common cathode(-). Your LED may differ, check the datasheets(but often you can wing it a bit)