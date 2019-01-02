#ifndef Sensor_h
#define Sensor_h

#include <Arduino.h>


class Sensor
{
    public:
        Sensor();
        char temperatureMin[8];
        char temperatureCurrent[8];
        char temperatureMax[8];
        char humidityMin[8];
        char humidityCurrent[8];
        char humidityMax[8];
        const int wakeup();
        const int read();
        const int age();
        void reset();
    private:
        const uint16_t CRC16(const uint8_t* ptr, uint8_t length);
        void mkStr();
        uint32_t lastGoodRead = 0;
        float temperatureMn = 99;
        float temperature = -99;
        float temperatureMx = -99;
        float humidityMn = 99;
        float humidity = -99;
        float humidityMx = -99;
        uint8_t buf[8] = {0};
        static const uint8_t address = 0xB8 >> 1;

};


#endif // Sensor_h
