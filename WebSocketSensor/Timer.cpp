#include "Timer.h"

void Timer::init(Logger* logger, IPAddress ip)
{
    this->logger = logger;
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
    return UNIXTime + (millis() - lastNTPResponse) / 1000;
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
        uint32_t NTPTime = (NTPBuffer[40] << 24) | (NTPBuffer[41] << 16) | (NTPBuffer[42] << 8) | NTPBuffer[43];
        // Convert NTP time to a UNIX timestamp:
        // Unix time starts on Jan 1 1970. That's 2208988800 seconds in NTP time:
        const uint32_t seventyYears = 2208988800UL;
        // subtract seventy years:
        uint32_t UNIXTimeN = NTPTime - seventyYears;
        if (UNIXTimeN > UNIXTime) {
            UNIXTime = UNIXTimeN;
            Serial.print(millis());
            Serial.print("\tNTP response:\t");
            Serial.println(UNIXTime);
            lastNTPResponse = millis();
            response = true;
        } else {
            Serial.println("dublicate UNIXTime");
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


inline int getSeconds(uint32_t UNIXTime)
{
    return UNIXTime % 60;
}

inline int getMinutes(uint32_t UNIXTime)
{
    return UNIXTime / 60 % 60;
}

inline int getHours(uint32_t UNIXTime)
{
    return UNIXTime / 3600 % 24;
}
