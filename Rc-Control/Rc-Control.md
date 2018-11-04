## Purpose
Connect the radio control transmitter trainer port as a computer joystick. Works for flight simulators and other programs supporting a joystick.

## Short version
Uses a Pro Micro, a 3.5 mm audio connector and the Joystick library.
Install the [Joystick library](https://github.com/MHeironimus/ArduinoJoystickLibrary). Install the [sketch](../../../raw/master/Rc-Control/Rc-Control.ino).
Connect the audio plug.

## Longer version
I have an old USB Transmitter simulator called “Simulator FMS” from Esky Hobby. It seams to work well with my different simulators, but I would rather use my “real” Spectrum DX6i which I use for flying outdoors.

My path was not completely straight, but here it is:

I remembered I did a project like that many years ago, with assembler code on a small ATTiny connected to the computer serial port. That worked well but today none of my computers have a serial port; today it’s all USB.
Anyway, that memory helped me to know what to search for on the Internet. I found an article by Richard Prinz called [Connecting your R/C Transmitter to a PC using an Arduino UNO](https://www.min.at/prinz/?x=entry:entry130320-204119). His sketch had been improved by Reynir Siik to use interupts for better accuracy.

Next I found an article somewhere that wasn’t much help except for the statement that my DX6i uses 3.3 V on the Trainer port. That’s good. It means I can connect it directly to any kind of Arduino board without burning anything.

I found a tiny board called [Pro Micro Arduino-compatible](https://www.kjell.com/se/sortiment/el-verktyg/arduino/utvecklingskort/pro-micro-arduino-kompatibelt-utvecklingskort-p87965) at my local shop for less than (the equivalent of) $11. It’s a cool little board, but it’s a clone of a clone and it took me some trial-and-error to get it working in the Arduino IDE. Running as a Leonardo worked, without any extra drivers.
So I connected it according to Richards advice and installed his sketch.

BTW, [here](http://www.pighixxx.net/wp-content/uploads/2016/07/pro_micro_pinout_v1_0_blue.pdf) I found a useful pin diagram.

Next I started up crrcsim and had some trouble to find the adapter from Linux. After some more Internet searches I found out that I had to create a symbolic link “sudo ln -s /dev/ttyACM0 /dev/ttyUSB1” and after that it worked just fine, using the FMSPIC method for input.

(That symbolic link should be automatically created when the USB is connected, by tweaking /etc/udev/rules.d/00-joystick.rules, but I leave that for now).

As it turned out, this old FMS interface is not supported by my, nowadays, favourite simulator [liftoff](http://www.liftoff-game.com). So OK, this FMSPIC is too old to be supported now when everyone is using game controllers such as joysticks. Back to the Internet and look for some way to use the Arduino to emulate a game controller on USB. I found the Joystick library by Matthew Heironimus on [Github](https://github.com/MHeironimus/ArduinoJoystickLibrary).
Aha, that is what I need. I want to use Richards/Reynis method of reading the signal from the transmitters trainer port, and then use Matthews method to send it to the computer.

When I loaded my first sketch and connected everything it was all very strange. The data looked almost random.
I suspected it was overload in the USB interface, as the transmitter sends new data at 50 hz.
Well, half that rate would actually be good enough, so I changed the way I send data. Every second loop I just store the data and every other loop I send the average of two cycles.

This worked well and the last touch was to assign the channels to the correct game controller functions.
I connected to windows and it was identified as a game controller in the Devices and Printers page. Right-click and open Game controller settings, then click Properties. Testing one stick movement at a time I could find out how to map the transmitter channels to joystick functions.

The final step was to build a small box. I just cut some plywood and glued together. I mounted the board and the USB cable inside using a little hot glue, and then wrapped it all with tape.

[Here](../../../raw/master/Rc-Control/Rc-Control.ino) is the final sketch.

ps: Don't ask about Real Flight. You have to buy their own adapter, due to their copy protection.
