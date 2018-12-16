#ifndef Logger_h
#define Logger_h

#include <Arduino.h>

class Logger
{
    private:

    public:
        void addLogData(uint32_t currentTime, float t, float h);
};

#endif // Logger_h
