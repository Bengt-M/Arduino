#ifndef Timer_h
#define Timer_h

#include <Arduino.h>
#include "Logger.h"
#include <ESP8266WiFi.h>
#include <WiFiUdp.h>


class Timer
{
    public:
        void init(Logger* logger, IPAddress ip);
        void sendNTPpacket();
        bool ntpResponseHandle();
        bool loop(const uint32_t currentMillis);
        uint32_t getCurrentTime();

    private:
        static const int NTP_PACKET_SIZE = 48;  // NTP time stamp is in the first 48 bytes of the message
//        static const uint32_t intervalNTPStd = 15 * 1000; // ms
        static const uint32_t intervalNTPStd = 7L * 60L * 1000L; // ms
        uint32_t intervalNTPreq = 1000;
        uint32_t prevNTPreq = 0;
        uint32_t lastNTPResponse = 0;
        Logger* logger;
        IPAddress ip;
        uint32_t UNIXTime = 0;
        byte NTPBuffer[NTP_PACKET_SIZE]; // buffer to hold incoming and outgoing packets
        WiFiUDP UDP;
};

#endif
