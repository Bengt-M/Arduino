#include "Sensor.h"
#include <Wire.h>


Sensor::Sensor()
{
    Wire.begin(9, 10); // GPIO for SDA, SCL
}


const uint16_t Sensor::CRC16(const uint8_t* ptr, uint8_t length)
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


void Sensor::mkStr()
{
//    Serial.println("Sensor::mkStr()");
    int val = (10.0 * temperature);
    sprintf(temperatureCurrent, "%d.%d", val / 10, val % 10);
//    Serial.println(temperatureCurrent);
//    Serial.println(temperature);
    val = (10.0 * temperatureMn);
    sprintf(temperatureMin, "%d.%d", val / 10, val % 10);
    val = (10.0 * temperatureMx);
    sprintf(temperatureMax, "%d.%d", val / 10, val % 10);
    val = (10.0 * humidity);
    sprintf(humidityCurrent, "%d", val / 10);
    val = (10.0 * humidityMn);
    sprintf(humidityMin, "%d", val / 10);
    val = (10.0 * humidityMx);
    sprintf(humidityMax, "%d", val / 10);
}


void Sensor::reset()
{
    Serial.println("Sensor::reset()");
    temperatureMn = temperature;
    temperatureMx = temperature;
    humidityMn = humidity;
    humidityMx = humidity;
    mkStr();
}


const int Sensor::wakeup()
{
    Wire.beginTransmission(address);
    Wire.endTransmission();
    Wire.beginTransmission(address);
    Wire.write((uint8_t)0x03);
    Wire.write((uint8_t)0x00);
    Wire.write((uint8_t)0x04);
    return Wire.endTransmission();
}


const int Sensor::age()
{
    return (millis() - lastGoodRead) / 1000;
}


const int Sensor::read()
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
//        Serial.print("Sensor temp = ");
//        Serial.println(local_t);
        if (humidity < -10.0) {
            temperature = local_t; // no filter first time
            humidity =  local_h;
        } else {
            temperature = 0.7 * temperature + 0.3 * local_t; // low pass filter
            humidity = 0.7 * humidity + 0.3 * local_h;
        }
        if (temperature > temperatureMx) {
            temperatureMx = temperature;
        }
        if (temperature < temperatureMn) {
            temperatureMn = temperature;
        }
        if (humidity > humidityMx) {
            humidityMx = humidity;
        }
        if (humidity < humidityMn) {
            humidityMn = humidity;
        }
//        Serial.print("Sensor temp2 = ");
//        Serial.println(temperature);
        mkStr();
        lastGoodRead = millis();
        return 0;
    }
    return 2;
}
