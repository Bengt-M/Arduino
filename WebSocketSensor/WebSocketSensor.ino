#include <ESP8266WiFi.h>
#include <ESP8266WiFiMulti.h>
#include <Wire.h>
#include <ArduinoJson.h>
#include <ESP8266HTTPClient.h>

#include "Timer.h"

//--- include this with your ssid and password
#include "Password.h"

ADC_MODE(ADC_VCC); //vcc read-mode

ESP8266WiFiMulti wifiMulti;        // Create an instance of the ESP8266WiFiMulti class, called 'wifiMulti'
uint8_t buf[8] = {0};
IPAddress timeServerIP(10, 45, 77, 1);       // NTP server address
//static const char* NTPServerName = "time.nist.gov";

static const uint8_t pin = 2; // physical pin D4  //TODO: Detta är fel pinne. Jag vill ha den dioden är kopplad till
uint8_t pinValue = 0;
static const uint8_t address = 0xB8 >> 1;
float temperature = -99.0;
float humidity = -99.0;
uint32_t interval = 100; // ms
boolean sleeping = true;
static const uint32_t intervalTempHumidRead = 3000; // ms
uint32_t prevTempHumidRead = 0; // ms

StaticJsonBuffer<80> doc;
JsonObject& root = doc.createObject();

Timer timer;

/*__________________________________________________________SETUP__________________________________________________________*/

void setup()
{
    Serial.begin(115200);        // Start the Serial communication to send messages to the computer
    while (!Serial) {
        delay(1);
    }
    Serial.println("setup()");
    // prepare pin TODO: Check this with i2c running
    pinMode(pin, OUTPUT);
    digitalWrite(pin, pinValue);

    // Start a Wi-Fi access point, and try to connect to some given access points. Then wait for either an AP or STA connection
    WiFi.persistent(true);
    wifiMulti.addAP(WiFiNetwork, Password);    // add Wi-Fi networks you want to connect to (see Password.h)

    Wire.begin();
    // Here is a way to get the IP from DNS. I use a NTP in my router and knows its IP always
    //    if (!WiFi.hostByName(NTPServerName, timeServerIP)) { // Get the IP address of the NTP server
    //        Serial.println("DNS lookup failed. Rebooting.");
    //        Serial.flush();
    //        ESP.reset();
    timer.init(timeServerIP);
    root.printTo(Serial);
    Serial.println();

    // TODO: Should do all measurements before waiting for WiFi to start
    Serial.print("Connecting");
    while (wifiMulti.run() != WL_CONNECTED) {       // Wait for the Wi-Fi to connect
        delay(250);
        Serial.print('.');
    }
    Serial.println();
    Serial.print("Connected to ");
    Serial.println(WiFi.SSID());                    // Tell us what network we're connected to
    Serial.print("IP address: ");
    Serial.print(WiFi.localIP());                   // Send the IP address of the ESP8266 to the computer
    Serial.println();

    timer.sendNTPpacket();               // Send an NTP request
    pinMode(0, INPUT_PULLUP);
}

/*__________________________________________________________LOOP__________________________________________________________*/

void loop()
{
    /*___________ TempHumid sensor ___________*/
    uint32_t currentMillis = millis();
    if ((currentMillis - prevTempHumidRead) >= interval) {
        if (sleeping) {
            tempHumidWakeup();
            interval = 2;
            sleeping = false;
        } else {
            tempHumidHandle();
            interval = intervalTempHumidRead;
            sleeping = true;
        }
        pinValue = !pinValue;
        digitalWrite(pin, pinValue);
        prevTempHumidRead = currentMillis;
    }

    if (timer.loop(currentMillis)) {
        if (humidity > 0.0) {
            //TODO: detta verkar faktiskt funka. Måste lista ut var det ska ligga
            if (WiFi.status() == WL_CONNECTED) {
                HTTPClient http;
                float vcc = (float)ESP.getVcc() * 1.096 / 1000.0;
                Serial.print("vcc=");
                Serial.println(vcc);
                http.begin(LOGSERVER); // define in Password.h
                http.addHeader("Content-Type", "application/x-www-form-urlencoded");
                String s = String("time=") + String(timer.getCurrentTime())
                           + "&t=" + String(temperature, 2)
                           + "&h=" + String(humidity, 1)
                           + "&vcc=" + String(vcc, 3)
                           + "\n";
                Serial.print(s);

                int httpCode = http.POST(s);
                String payload = http.getString();
                Serial.print("http response = ");
                Serial.println(httpCode);
                //Serial.println(payload);
                http.end();

                Serial.println("------------ I'm going into deep sleep now --------------");
                ESP.deepSleep(5 * 60 * 1000000);
            }
        }
    }
}


/*__________________________________________________________ HANDLERS__________________________________________________________*/

uint16_t CRC16(uint8_t* ptr, uint8_t length)
{
    uint16_t crc = 0xFFFF;
    uint8_t s = 0x00;

    while (length--) {
        crc ^= *ptr++;
        for (s = 0; s < 8; s++) {
            if ((crc & 0x01) != 0) {
                crc >>= 1;
                crc ^= 0xA001;
            } else {
                crc >>= 1;
            }
        }
    }
    return crc;
}

void tempHumidHandle()
{
    switch (tempHumidRead()) {
        case 2:
            Serial.println("Sensor CRC failed");
            break;
        case 1:
            Serial.println("Sensor offline");
            break;
        case 0:
            root["t"] = temperature;
            root["h"] = humidity;
            String output;
            root.printTo(output);
            Serial.println(output);
            break;
    }
}

int tempHumidWakeup()
{
    Wire.beginTransmission(address);
    Wire.endTransmission();
    Wire.beginTransmission(address);
    Wire.write((uint8_t)0x03);
    Wire.write((uint8_t)0x00);
    Wire.write((uint8_t)0x04);
    return Wire.endTransmission();
}

int tempHumidRead()
{
    Wire.requestFrom(address, (uint8_t)0x08);
    for (int i = 0; i < 0x08; i++) {
        buf[i] = Wire.read();
    }
    // CRC check
    unsigned int Rcrc = buf[7] << 8;
    Rcrc += buf[6];
    if (Rcrc == CRC16(buf, 6)) {
        float local_t;
        float local_h;
        unsigned int s_temperature = ((buf[4] & 0x7F) << 8) + buf[5];
        local_t = s_temperature / 10.0;
        local_t = ((buf[4] & 0x80) >> 7) == 1 ? -local_t : local_t;
        unsigned int s_humidity = (buf[2] << 8) + buf[3];
        local_h = s_humidity / 10.0;
        if (humidity < -10.0) {
            temperature = local_t; // no filter first time
            humidity =  local_h;
        } else {
            temperature = 0.7 * temperature + 0.3 * local_t; // low pass filter
            humidity = 0.7 * humidity + 0.3 * local_h;
        }
        return 0;
    }
    return 2;
}
