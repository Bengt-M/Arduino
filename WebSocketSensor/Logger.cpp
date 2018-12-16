#include "Logger.h"
#include <ESP8266WiFi.h>
#include <ESP8266HTTPClient.h>
//--- include this with your ssid and password
#include "Password.h"

void Logger::addLogData(uint32_t currentTime, float t, float h)
{
    if (h > 0.0) {
        if (WiFi.status() == WL_CONNECTED) {
            HTTPClient http;
            float vcc = (float)ESP.getVcc() * 1.096 / 1000.0;
            http.begin(LOGSERVER); // define in Password.h
            http.addHeader("Content-Type", "application/x-www-form-urlencoded");
            String s = String("time=") + String(currentTime)
                       + "&t=" + String(t, 2)
                       + "&h=" + String(h, 1)
                       + "&vcc=" + String(vcc, 3)
                       + "\n";
            Serial.print("Uploading log ");
            Serial.println(s);

            int httpCode = http.POST(s);
            String payload = http.getString();
            Serial.print("http response = ");
            Serial.println(httpCode);
            //Serial.println(payload);
            http.end();
        }
    }
}
