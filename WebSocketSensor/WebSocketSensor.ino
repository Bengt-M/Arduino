#include <ESP8266WiFi.h>
#include <ESP8266WiFiMulti.h>
#include <ArduinoOTA.h>
#include <ESP8266WebServer.h>
#include <WebSocketsServer.h>
#include <Wire.h>
#include <ArduinoJson.h>
#include <ESP8266HTTPClient.h>

#include "Logger.h"
//todo: factor out time and sensor
#include "Timer.h"

//--- include this with your ssid and password
#include "Password.h"

ESP8266WiFiMulti wifiMulti;        // Create an instance of the ESP8266WiFiMulti class, called 'wifiMulti'
ESP8266WebServer server(80);       // create a web server on port 80
WebSocketsServer webSocket(81);    // create a websocket server on port 81
File fsUploadFile;                 // a File variable to temporarily store the received file
bool powerOnStatus = false;        // The power relay is turned off on startup
uint8_t buf[8] = {0};
IPAddress timeServerIP(10, 45, 77, 1);       // NTP server address
//static const char* NTPServerName = "time.nist.gov";

static const uint8_t pin = 2; // physical pin D4

static const uint8_t address = 0xB8 >> 1;
float temperature = 20.0;
float humidity = 40.0;
uint32_t interval = 100; // ms
boolean sleeping = true;
static const uint32_t intervalTempHumidRead = 3000; // ms
uint32_t prevTempHumidRead = 0; // ms

StaticJsonBuffer<80> doc;
JsonObject& root = doc.createObject();

Logger logger;
Timer timer;

/*__________________________________________________________SETUP__________________________________________________________*/

void setup()
{
    Serial.begin(115200);        // Start the Serial communication to send messages to the computer
    while (!Serial) {
        delay(1);
    }
    Serial.println();
    // prepare pin TODO: Check this with i2c running
    pinMode(pin, OUTPUT);
    digitalWrite(pin, 0);
    startWiFi();                 // Start a Wi-Fi access point, and try to connect to some given access points. Then wait for either an AP or STA connection
    startOTA();                  // Start the OTA service
    startSPIFFS();               // Start the SPIFFS and list all contents
    startWebSocket();            // Start a WebSocket server
    startServer();               // Start a HTTP server with a file read handler and an upload handler
    Wire.begin();
    // Here is a way to get the IP from DNS. I use a NTP in my router and knows its IP always
    //    if (!WiFi.hostByName(NTPServerName, timeServerIP)) { // Get the IP address of the NTP server
    //        Serial.println("DNS lookup failed. Rebooting.");
    //        Serial.flush();
    //        ESP.reset();
    timer.init(&logger, timeServerIP);
    root.printTo(Serial);
    Serial.println();
    logger.init();
    timer.sendNTPpacket();               // Send an NTP request
}

/*__________________________________________________________LOOP__________________________________________________________*/

void loop()
{
    webSocket.loop();             // constantly check for websocket events
    server.handleClient();        // run the server
    ArduinoOTA.handle();          // listen for OTA events

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
        prevTempHumidRead = currentMillis;
    }

    if (timer.loop(currentMillis)) {
        logger.addLogData(timer.getCurrentTime(), temperature, humidity);

        //TODO: detta verkar faktiskt funka. Måste lista ut var det ska ligga
        if (WiFi.status() == WL_CONNECTED) {
            HTTPClient http;
            http.begin(LOGSERVER); // define in Password.h
            http.addHeader("Content-Type", "application/x-www-form-urlencoded");
            String s = String("time=") + String(timer.getCurrentTime()) + "&t=" + String(temperature, 2) + "&h=" + String(humidity, 1) + "\n";
            Serial.print(s);

            int httpCode = http.POST(s);
            String payload = http.getString();
            Serial.print("http response = ");
            Serial.println(httpCode);
            //Serial.println(payload);
            http.end();
        }
    }
}

/*__________________________________________________________SETUP_FUNCTIONS__________________________________________________________*/

void startWiFi()   // Start try to connect to some given access points. Then wait for connection
{
    WiFi.persistent(true);
    wifiMulti.addAP(WiFiNetwork, Password);    // add Wi-Fi networks you want to connect to (see Password.h)
    Serial.println("Connecting");
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
}

void startOTA()   // Start the OTA service
{
    ArduinoOTA.setHostname(OTAName);
    ArduinoOTA.setPassword(OTAPassword);
    ArduinoOTA.setPort(8266);
    ArduinoOTA.onStart([]() {
        Serial.println("Start");
        powerOn(false);
    });
    ArduinoOTA.onEnd([]() {
        Serial.println("End");
        printSPIFFS();
    });
    ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
        Serial.printf("Progress: %u%%\r", (progress / (total / 100)));
    });
    ArduinoOTA.onError([](ota_error_t error) {
        Serial.printf("Error[%u]: ", error);
        switch (error) {
            case OTA_AUTH_ERROR:
                Serial.println("Auth Failed");
                break;
            case OTA_BEGIN_ERROR:
                Serial.println("Begin Failed");
                break;
            case OTA_CONNECT_ERROR:
                Serial.println("Connect Failed");
                break;
            case OTA_RECEIVE_ERROR:
                Serial.println("Receive Failed");
                break;
            case OTA_END_ERROR:
                Serial.println("End Failed");
                break;
        }
    });
    ArduinoOTA.begin();
    Serial.println("OTA ready");
}

void startSPIFFS()   // Start the SPIFFS and list all contents
{
    SPIFFS.begin();                             // Start the SPI Flash File System (SPIFFS)
    Serial.println("SPIFFS started");
    printSPIFFS();
}

void startWebSocket()   // Start a WebSocket server
{
    webSocket.begin();                          // start the websocket server
    webSocket.onEvent(
        webSocketEvent);          // if there's an incomming websocket message, go to function 'webSocketEvent'
    Serial.println("WebSocket server started");
}

void startServer()   // Start a HTTP server with a file read handler and an upload handler
{
    server.on("/edit.html",  HTTP_POST, []() {  // If a POST request is sent to the /edit.html address,
        server.send(200, "text/plain", "");
    }, handleFileUpload);                       // go to 'handleFileUpload'
    server.onNotFound(
        handleNotFound);          // if someone requests any other file or page, go to function 'handleNotFound' and check if the file exists
    server.begin();                             // start the HTTP server
    Serial.println("HTTP server started.");
}

/*__________________________________________________________SERVER_HANDLERS__________________________________________________________*/

void handleNotFound()   // if the requested file or page doesn't exist, return a 404 not found error
{
    if (!handleFileRead(server.uri())) {        // check if the file exists in the flash memory (SPIFFS), if so, send it
        server.send(404, "text/plain", "404: File Not Found");
    }
}

bool handleFileRead(String path)   // send the right file to the client (if it exists)
{
    Serial.println("handleFileRead: " + path);
    if (path.endsWith("/")) path += "index.html";              // If a folder is requested, send the index file
    String contentType = getContentType(path);                 // Get the MIME type
    String pathWithGz = path + ".gz";
    if (SPIFFS.exists(pathWithGz)
        || SPIFFS.exists(path)) {    // If the file exists, either as a compressed archive, or normal
        if (SPIFFS.exists(pathWithGz)) {                       // If there's a compressed version available
            path += ".gz";                                     // Use the compressed verion
        }
        File file = SPIFFS.open(path, "r");                    // Open the file
        size_t sent = server.streamFile(file, contentType);    // Send it to the client
        file.close();                                          // Close the file again
        Serial.println(String("\tSent file: ") + path);
        powerOn();
        return true;
    }
    Serial.println(String("\tFile Not Found: ") + path);   // If the file doesn't exist, return false
    return false;
}

void handleFileUpload()   // upload a new file to the SPIFFS
{
    HTTPUpload& upload = server.upload();
    String path;

    if (upload.status == UPLOAD_FILE_START) {
        powerOn(false);
        path = upload.filename;
        if (!path.startsWith("/")) {
            path = "/" + path;
        }
        if (!path.endsWith(".gz")) {                         // The file server always prefers a compressed version of a file
            String pathWithGz = path + ".gz";                // So if an uploaded file is not compressed, the existing compressed
            if (SPIFFS.exists(pathWithGz)) {
                SPIFFS.remove(pathWithGz);                   // version of that file must be deleted (if it exists)
            }
        }
        Serial.print("handleFileUpload Name: ");
        Serial.println(path);
        fsUploadFile = SPIFFS.open(path, "w");            // Open the file for writing in SPIFFS (create if it doesn't exist)
        path = String();
    } else if (upload.status == UPLOAD_FILE_WRITE) {
        if (fsUploadFile) {
            fsUploadFile.write(upload.buf, upload.currentSize); // Write the received bytes to the file
        }
    } else if (upload.status == UPLOAD_FILE_END) {
        if (fsUploadFile) {                                   // If the file was successfully created
            fsUploadFile.close();                               // Close the file again
            Serial.print("handleFileUpload Size: ");
            Serial.println(upload.totalSize);
            server.sendHeader("Location", "/success.html");     // Redirect the client to the success page
            server.send(303);
        } else {
            server.send(500, "text/plain", "500: couldn't create file");
        }
    }
}

void webSocketEvent(uint8_t num, WStype_t type, uint8_t* payload,
                    size_t lenght)    // When a WebSocket message is received
{
    switch (type) {
        case WStype_DISCONNECTED:             // if the websocket is disconnected
            Serial.printf("[%u] Disconnected!\n", num);
            break;
        case WStype_CONNECTED: {              // if a new websocket connection is established
                IPAddress ip = webSocket.remoteIP(num);
                Serial.printf("[%u] Connected from %d.%d.%d.%d url: %s\n", num, ip[0], ip[1], ip[2], ip[3], payload);
                powerOn();
            }
            break;
        case WStype_TEXT:                     // if new text data is received
            Serial.printf("[%u] get Text: %s\n", num, payload);
            if (payload[0] == '1') {          // the browser sends a 1 when the power is enabled
                powerOn(true);
            } else if (payload[0] == '0') {   // ...and a 0 when it is off
                powerOn(false);
            } else if (payload[0] == 'S') {   // Save all logger data to a SPIFFS file
                logger.writeLoggerFile();
            } else if (payload[0] == 'q') { // query for all logger data
                String output;
                logger.getAllData(&output);
                // Serial.println(output);
                webSocket.sendTXT(num, output);
            }
            break;
    }
}

/*__________________________________________________________HELPER_FUNCTIONS__________________________________________________________*/

void printSPIFFS()
{
    FSInfo fs_info;
    Dir dir = SPIFFS.openDir("/");

    Serial.println("SPIFFS contents:");
    while (dir.next()) {                      // List the file system contents
        String fileName = dir.fileName();
        size_t fileSize = dir.fileSize();
        Serial.printf("\tFS File: %s, size: %s\r\n", fileName.c_str(), formatBytes(fileSize).c_str());
    }
    SPIFFS.info(fs_info);
    Serial.printf("Using: %s, of: %s\r\n", formatBytes(fs_info.usedBytes).c_str(), formatBytes(fs_info.totalBytes).c_str());
    Serial.println();
}

String formatBytes(size_t bytes)   // convert sizes in bytes to KB and MB
{
    if (bytes < 1024) {
        return String(bytes) + "B";
    } else if (bytes < (1024 * 1024)) {
        return String(bytes / 1024.0) + "KB";
    } else if (bytes < (1024 * 1024 * 1024)) {
        return String(bytes / 1024.0 / 1024.0) + "MB";
    }
}

String getContentType(String filename)   // determine the filetype of a given filename, based on the extension
{
    if (filename.endsWith(".html")) {
        return "text/html";
    } else if (filename.endsWith(".css")) {
        return "text/css";
    } else if (filename.endsWith(".js")) {
        return "application/javascript";
    } else if (filename.endsWith(".ico"))           {
        return "image/x-icon";
    } else if (filename.endsWith(".gz")) {
        return "application/x-gzip";
    }
    return "text/plain";
}

void powerOn()
{
    powerOn(powerOnStatus); // default is current status
}

void powerOn(boolean on)   // set the pin, read status, send to all connected clients
{
    powerOnStatus = on;
    digitalWrite(pin, on);
    root["t"] = temperature;
    root["h"] = humidity;
    root["p"] = on;
    root.printTo(Serial);
    Serial.println();
    String output;
    root.printTo(output);
    webSocket.broadcastTXT(output);
}

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
            Serial.println("CRC failed");
            break;
        case 1:
            Serial.println("Sensor offline");
            break;
        case 0:
            root["t"] = temperature;
            root["h"] = humidity;
            root["p"] = powerOnStatus;
            String output;
            root.printTo(output);
            //            Serial.println(output);
            // logger.addLogData(UNIXTime, temperature, humidity); // uncomment for testing filling logger quicker
            webSocket.broadcastTXT(output);
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
        temperature = 0.7 * temperature + 0.3 * local_t; // low pass filter
        humidity = 0.7 * humidity + 0.3 * local_h;
        return 0;
    }
    return 2;
}
