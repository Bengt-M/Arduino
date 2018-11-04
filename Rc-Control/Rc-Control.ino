#include <Joystick.h>

// FMS PIC protocol for connecting a PPM stream to a PC.
// http://modelsimulator.com/
//
// Converted from code by
// Richard.Prinz@MIN.at 3.2013
// by Reynir Siik
// 2015-MAR-14, Pi day
//
// Changed by Bengt Månsson to use the joystick library from Matthew Heironimus, instead of FMSPIC
// 2018-10-31
//
// This is designed for Arduino Pro Micro (blue)
// A mono 3.5 mm plug is connected with inner pin to pin 2, and outer sleeve to ground pin
// To use a stereo plug; connect both inner and outer sleeve to ground
// Arduino IDE (1.8.5) is set for an Arduino Leonardo board, using AVRISP mkII
//
/*
    PPM pulse train:
    __   ___________________________________   _____   ____   _______   ___ //___   __________   __ HIGH
      |_|      Sync pulse > 3000 us         |_|     |_|    |_|       |_|   //    |_|          |_|
                                               Channel pulse 1000 to 2000 us                        LOW
    Pulse start on falling edge, pulse length is until next falling edge (which also is start for next)
*/

const unsigned int numChannels = 6;
const unsigned int lastChannel = numChannels - 1;
unsigned int channelValue[numChannels] = {0};
unsigned int channelIndex = 0;
boolean timeToSend = false; // Full speed from the Tx seems to be too much on USB so use only half

// Variables used in interupt routine are declared volatile to survive task switching
volatile boolean nextPtr = 0;
volatile unsigned long timeStamp[2];
volatile boolean trig = false;

Joystick_ Joystick(
    JOYSTICK_DEFAULT_REPORT_ID, JOYSTICK_TYPE_JOYSTICK, 2, 0,
    true, true, false,
    false, false, false,
    true, true, false, false, false); // Check ~/Arduino/libraries/Joystick/src/Joystick.h if you want a different mapping

void setup()
{
    noInterrupts();
    // Set Range Values
    Joystick.setXAxisRange(0, 1000);
    Joystick.setYAxisRange(0, 1000);
    Joystick.setThrottleRange(0, 1000);
    Joystick.setRudderRange(0, 1000);
    Joystick.begin();
    pinMode(2, INPUT_PULLUP);
    attachInterrupt(1, ISR1, FALLING); //Trigg on negative edge on pin 2
    interrupts();
}

void loop()
{
    if (trig == true) {
        noInterrupts();
        trig = false;
        unsigned int pulseLen = (int)(timeStamp[!nextPtr] - timeStamp[nextPtr] - 1000L); // pulseLen now in 0..1000, except for sync
        interrupts();
        if (pulseLen > 2000) {  // sync pulse
            channelIndex = 0;
        } else {                // channel pulse
            if (timeToSend) {
                channelValue[channelIndex] = (channelValue[channelIndex] + pulseLen) / 2; // use a two-cycle average
                if (channelIndex == lastChannel) { // update USB after last pulse of a cycle
                    Joystick.setXAxis(1000 - channelValue[1]); // reverse this one
                    Joystick.setYAxis(1000 - channelValue[2]);
                    Joystick.setThrottle(channelValue[0]);
                    Joystick.setRudder  (1000 - channelValue[3]);
                    Joystick.setButton(0, channelValue[4] > 500 ? 1 : 0); // testing how Gear and Flap can be like buttons
                    Joystick.setButton(1, channelValue[5] < 250 ? 1 : 0);
                    Joystick.sendState();
                    channelIndex = lastChannel;
                    timeToSend = false;
                }
            } else {
                channelValue[channelIndex] = pulseLen;
                if (channelIndex == lastChannel) {
                    timeToSend = true;
                }
            }
            channelIndex++;
        }
    }
}

// Pulse timing interupt service routine
void ISR1()
{
    timeStamp[nextPtr] = micros();
    nextPtr = !nextPtr;
    trig = true;
}

