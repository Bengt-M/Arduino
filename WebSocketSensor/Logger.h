#ifndef Logger_h
#define Logger_h

#include <Arduino.h>
#include <ArduinoJson.h>
#include <FS.h>

class Logger
{
    private:
        StaticJsonBuffer<23000> loggerdoc;
        JsonObject& loggerroot = loggerdoc.createObject();
        JsonArray& data = loggerroot.createNestedArray("data");
        JsonArray& col = loggerroot.createNestedArray("col");

    public:
        void init();
        void getAllData(String* output);
        void addLogData(uint32_t currentTime, float t, float h);
        void readLoggerFile();
        void writeLoggerFile();
};

#endif // Logger_h
