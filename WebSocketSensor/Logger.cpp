#include "Logger.h"

void Logger::init()
{
    col.add("Time");
    col.add("Temperature");
    col.add("Humidity");
    readLoggerFile();
}

void Logger::getAllData(String* output)
{
    loggerroot.printTo(*output);
}

void Logger::addLogData(uint32_t currentTime, float t, float h)
{
    if (currentTime > 0) {
        if (data.size() > 237) { // max num log points, tune together with json buffer
            data.remove(0);      // recycle
        }
        JsonArray& point = data.createNestedArray();
        point.add(currentTime);
        point.add(t);
        point.add(h);
        Serial.print("mem\t");
        Serial.println(loggerdoc.size());
        Serial.print("siz\t");
        Serial.println(data.size());
        // loggerroot.printTo(Serial);
        Serial.println();
    }
}

void Logger::readLoggerFile()      // read saved logger data from file
{
    File file = SPIFFS.open("/data.txt", "r");
    while (file.available()) {
        JsonArray& point = data.createNestedArray();
        String str = file.readStringUntil(';');
        if (str.length() == 0) {
            break;
        }
        point.add(str.toInt());
        str = file.readStringUntil(';');
        if (str.length() == 0) {
            break;
        }
        point.add(str.toFloat());
        str = file.readStringUntil('\n');
        if (str.length() == 0) {
            break;
        }
        point.add(str.toFloat());
    }
    file.close();
}

void Logger::writeLoggerFile()
{
    File file = SPIFFS.open("/data.txt", "w");
    if (file) {
        String output;
        for (auto value : data) {
            JsonArray& point = value.as<JsonArray&>();
            file.print(point[0].as<String>());
            file.print(';');
            file.print(point[1].as<String>());
            file.print(';');
            file.println(point[2].as<String>());
        }
        file.close();
    } else {
        Serial.println("Failed to create file");
    }
}
