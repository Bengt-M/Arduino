#ifndef Timer_h
#define Timer_h

#include <Arduino.h>
#include <WiFiUdp.h>


class Timer
{
    public:
        void init(IPAddress ip);
        void sendNTPpacket();
        bool ntpResponseHandle();
        bool loop(const uint32_t currentMillis);
        uint32_t getCurrentTime();

    private:
        static const int NTP_PACKET_SIZE = 48;  // NTP time stamp is in the first 48 bytes of the message
        static const uint32_t intervalNTPStd = 30 * 1000; // ms
        //        static const uint32_t intervalNTPStd = 7L * 60L * 1000L; // ms
        uint32_t intervalNTPreq = intervalNTPStd;
        uint32_t prevNTPreq = 0;
        uint32_t lastNTPResponse = 0;
        IPAddress ip;
        uint32_t NTPTime;
        byte NTPBuffer[NTP_PACKET_SIZE]; // buffer to hold incoming and outgoing packets
        WiFiUDP UDP;
};

#endif
