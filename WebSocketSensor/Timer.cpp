#include "Timer.h"

void Timer::init(IPAddress ip)
{
    this->ip = ip;

    Serial.println("Starting UDP");
    UDP.begin(123);                          // Start listening for UDP messages on port 123
    Serial.print("Local port:\t");
    Serial.println(UDP.localPort());
    Serial.println();
    Serial.print("Time server IP:\t");
    Serial.println(ip);
}

uint32_t Timer::getCurrentTime()
{
    return NTPTime + (millis() - lastNTPResponse) / 1000;
}

void Timer::sendNTPpacket()
{
    memset(NTPBuffer, 0, NTP_PACKET_SIZE);  // set all bytes in the buffer to 0
    NTPBuffer[0] = 0b11100011;              // Initialize values needed to form NTP request
    UDP.beginPacket(ip, 123);          // NTP requests are to port 123
    UDP.write(NTPBuffer, NTP_PACKET_SIZE);
    UDP.endPacket();
}


bool Timer::ntpResponseHandle()
{
    bool response = false;
    if (UDP.parsePacket() > 0) { // If there's data
        UDP.read(NTPBuffer, NTP_PACKET_SIZE); // read the packet into the buffer
        // Combine the 4 timestamp bytes into one 32-bit number
        uint32_t nNTPTime = (NTPBuffer[40] << 24) | (NTPBuffer[41] << 16) | (NTPBuffer[42] << 8) | NTPBuffer[43];
        if (nNTPTime > NTPTime) {
            NTPTime = nNTPTime;
            Serial.print(millis());
            Serial.print("\tNTP response:\t");
            Serial.println(NTPTime);
            lastNTPResponse = millis();
            response = true;
        } else {
            Serial.println("dublicate NtpTime");
        }
    } else if ((millis() - lastNTPResponse) > 3600000) {
        Serial.println("More than 1 hour since last NTP response. Rebooting.");
        Serial.flush();
        ESP.reset();
    }
    return response;
}

bool Timer::loop(const uint32_t currentMillis)
{
    if (currentMillis - prevNTPreq > intervalNTPreq) { // If time has passed since last NTP request
        Serial.print(currentMillis);
        Serial.println("\tSending NTP request ...");
        sendNTPpacket();               // Send an NTP request
        intervalNTPreq = intervalNTPStd;
        prevNTPreq = currentMillis;
    }
    return ntpResponseHandle();
}


inline int getSeconds(uint32_t NTPTime)
{
    return NTPTime % 60;
}

inline int getMinutes(uint32_t NTPTime)
{
    return NTPTime / 60 % 60;
}

inline int getHours(uint32_t NTPTime)
{
    return NTPTime / 3600 % 24;
}
