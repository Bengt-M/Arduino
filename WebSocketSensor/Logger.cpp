#include <ArduinoJson.h>
#include "Logger.h"
#include <ESP8266WiFi.h>
#include <ESP8266HTTPClient.h>
//--- include this with your ssid and password
#include "Password.h"
#include <WiFiClient.h>

WiFiClient wifiClient;

void Logger::addLogData(uint32_t currentTime, char* t, char* h)
{
  if (h[0] > 0) {

    if (WiFi.status() == WL_CONNECTED) {
      HTTPClient http;
      WiFiClient client;
      http.begin(client, LOGSERVER);
      float vcc = (float)ESP.getVcc() * 1.096 / 1000.0;
      char __attribute__((aligned(4))) output[128];
      char __attribute__((aligned(4))) fbuf[8];

      // create the string to send as the body json
      strcat(output, "{\"t\":\"");
      strcat(output, t);
      strcat(output, "\",\"h\":\"");
      strcat(output, h);
      strcat(output, "\",\"vcc\":\"");
      dtostrf(vcc, 3, 3, fbuf);
      strcat(output, fbuf);
      strcat(output, "\"}\n");

      Serial.print("Uploading log ");
      Serial.print(output);

      http.addHeader("Content-Type", "application/json");
      //Serial.print("Sending log ");
      int httpResponseCode = http.POST((const unsigned char*)output, strlen(output));
//      Serial.println(httpResponseCode);
//      Serial.println();
      // Serial.println(http.getString());
    }
  }
}
