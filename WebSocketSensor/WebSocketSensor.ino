#include <ESP8266WiFi.h>
#include <ESP8266WiFiMulti.h>
#include <ArduinoOTA.h>
#include <ESP8266WebServer.h>
#include <FS.h>
#include <WebSocketsServer.h>
#include <Wire.h>
#include <WiFiUdp.h>
#include <ArduinoJson.h>

//--- include this with your ssid and password
#include "Password.h"

ESP8266WiFiMulti wifiMulti;        // Create an instance of the ESP8266WiFiMulti class, called 'wifiMulti'
ESP8266WebServer server(80);       // create a web server on port 80
WebSocketsServer webSocket(81);    // create a websocket server on port 81
File fsUploadFile;                 // a File variable to temporarily store the received file
bool powerOnStatus = false;        // The power relay is turned off on startup
uint8_t buf[8] = {0};
WiFiUDP UDP;
IPAddress timeServerIP(10, 45, 77, 1);       // NTP server address
static const char *NTPServerName = "time.nist.gov";
static const int NTP_PACKET_SIZE = 48;  // NTP time stamp is in the first 48 bytes of the message
byte NTPBuffer[NTP_PACKET_SIZE]; // buffer to hold incoming and outgoing packets

static const char *OTAName = "ESP8266";   // A name and a password for the OTA service TODO: move to Password.h
static const char *OTAPassword = "esp8266"; // TODO: move to Password.h
static const char *mdnsName = "esp8266";  // Domain name for the mDNS responder
static const uint8_t pin = 2;

static const uint8_t address = 0xB8 >> 1;
float t = 20.0;
float h = 40.0;
uint32_t interval = 100; // ms
boolean sleeping = true;
static const uint32_t intervalTempHumidRead = 3000; // ms
uint32_t prevTempHumidRead = 0; // ms
uint32_t UNIXTime = 0;

//static const uint32_t intervalNTPStd = 5 * 1000; // ms
static const uint32_t intervalNTPStd = 15 * 60 * 1000; // ms
uint32_t intervalNTP = 1000;
uint32_t prevNTP = 0;
uint32_t lastNTPResponse = 0;
uint32_t currentTime = 0;

StaticJsonBuffer<80> doc;
JsonObject& root = doc.createObject();

StaticJsonBuffer<23000> loggerdoc;
JsonObject& loggerroot = loggerdoc.createObject();
JsonArray& data = loggerroot.createNestedArray("data");

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
    startUDP();
    // Here is a way to get the IP from DNS. I use a NTP in my router and knows its IP always
    //    if (!WiFi.hostByName(NTPServerName, timeServerIP)) { // Get the IP address of the NTP server
    //        Serial.println("DNS lookup failed. Rebooting.");
    //        Serial.flush();
    //        ESP.reset();
    Serial.print("Time server IP:\t");
    Serial.println(timeServerIP);
    root.printTo(Serial);
    Serial.println();
    sendNTPpacket(timeServerIP);               // Send an NTP request
    // create the json structure
    JsonArray& col = loggerroot.createNestedArray("col");
    col.add("Time");
    col.add("Temperature");
    col.add("Humidity");
    // read saved logger data from file
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

    /*___________ send ntp request ___________*/
    if (currentMillis - prevNTP > intervalNTP) { // If time has passed since last NTP request
        Serial.print(millis());
        Serial.println("\tSending NTP request ...");
        sendNTPpacket(timeServerIP);               // Send an NTP request
        intervalNTP = intervalNTPStd;
        prevNTP = currentMillis;
    }

    /*___________ handle ntp response (if any) ___________*/
    ntpResponseHandle(currentMillis);
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
    webSocket.onEvent(webSocketEvent);          // if there's an incomming websocket message, go to function 'webSocketEvent'
    Serial.println("WebSocket server started");
}

void startServer()   // Start a HTTP server with a file read handler and an upload handler
{
    server.on("/edit.html",  HTTP_POST, []() {  // If a POST request is sent to the /edit.html address,
        server.send(200, "text/plain", "");
    }, handleFileUpload);                       // go to 'handleFileUpload'
    server.onNotFound(handleNotFound);          // if someone requests any other file or page, go to function 'handleNotFound' and check if the file exists
    server.begin();                             // start the HTTP server
    Serial.println("HTTP server started.");
}

void startUDP()
{
    Serial.println("Starting UDP");
    UDP.begin(123);                          // Start listening for UDP messages on port 123
    Serial.print("Local port:\t");
    Serial.println(UDP.localPort());
    Serial.println();
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
    if (SPIFFS.exists(pathWithGz) || SPIFFS.exists(path)) {    // If the file exists, either as a compressed archive, or normal
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
        Serial.print("handleFileUpload Name: "); Serial.println(path);
        fsUploadFile = SPIFFS.open(path, "w");            // Open the file for writing in SPIFFS (create if it doesn't exist)
        path = String();
    } else if (upload.status == UPLOAD_FILE_WRITE) {
        if (fsUploadFile) {
            fsUploadFile.write(upload.buf, upload.currentSize); // Write the received bytes to the file
        }
    } else if (upload.status == UPLOAD_FILE_END) {
        if (fsUploadFile) {                                   // If the file was successfully created
            fsUploadFile.close();                               // Close the file again
            Serial.print("handleFileUpload Size: "); Serial.println(upload.totalSize);
            server.sendHeader("Location", "/success.html");     // Redirect the client to the success page
            server.send(303);
        } else {
            server.send(500, "text/plain", "500: couldn't create file");
        }
    }
}

void webSocketEvent(uint8_t num, WStype_t type, uint8_t *payload, size_t lenght)    // When a WebSocket message is received
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
            } else if (payload[0] == 'q') { // query for all logger data
                String output;
                loggerroot.printTo(output);
                // Serial.println(output);
                webSocket.sendTXT(num, output);
            }
            break;
    }
}

/*__________________________________________________________HELPER_FUNCTIONS__________________________________________________________*/

void printSPIFFS()
{
    Serial.println("SPIFFS contents:");
    Dir dir = SPIFFS.openDir("/");

    while (dir.next()) {                      // List the file system contents
        String fileName = dir.fileName();
        size_t fileSize = dir.fileSize();
        Serial.printf("\tFS File: %s, size: %s\r\n", fileName.c_str(), formatBytes(fileSize).c_str());
    }
    // TODO: also print free space
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
    root["t"] = t;
    root["h"] = h;
    root["p"] = on;
    root.printTo(Serial);
    String output;
    root.printTo(output);
    webSocket.broadcastTXT(output);
}

uint16_t CRC16(uint8_t *ptr, uint8_t length)
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

void sendNTPpacket(IPAddress& address)
{
    memset(NTPBuffer, 0, NTP_PACKET_SIZE);  // set all bytes in the buffer to 0
    NTPBuffer[0] = 0b11100011;              // Initialize values needed to form NTP request
    UDP.beginPacket(address, 123);          // NTP requests are to port 123
    UDP.write(NTPBuffer, NTP_PACKET_SIZE);
    UDP.endPacket();
}

void ntpResponseHandle(uint32_t currentMillis)
{
    if (UDP.parsePacket() > 0) { // If there's data
        UDP.read(NTPBuffer, NTP_PACKET_SIZE); // read the packet into the buffer
        // Combine the 4 timestamp bytes into one 32-bit number
        uint32_t NTPTime = (NTPBuffer[40] << 24) | (NTPBuffer[41] << 16) | (NTPBuffer[42] << 8) | NTPBuffer[43];
        // Convert NTP time to a UNIX timestamp:
        // Unix time starts on Jan 1 1970. That's 2208988800 seconds in NTP time:
        const uint32_t seventyYears = 2208988800UL;
        // subtract seventy years:
        UNIXTime = NTPTime - seventyYears;
        Serial.print(millis());
        Serial.print("\tNTP response:\t");
        Serial.println(UNIXTime);
        lastNTPResponse = currentMillis;
        addLogData();
    } else if ((currentMillis - lastNTPResponse) > 3600000) {
        Serial.println("More than 1 hour since last NTP response. Rebooting.");
        Serial.flush();
        ESP.reset();
    }
    currentTime = UNIXTime + (currentMillis - lastNTPResponse) / 1000;
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
            root["t"] = t;
            root["h"] = h;
            root["p"] = powerOnStatus;
            String output;
            root.printTo(output);
            // addLogData(); // uncomment for testing filling logger quicker
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
        unsigned int temperature = ((buf[4] & 0x7F) << 8) + buf[5];
        local_t = temperature / 10.0;
        local_t = ((buf[4] & 0x80) >> 7) == 1 ? -local_t : local_t;
        unsigned int humidity = (buf[2] << 8) + buf[3];
        local_h = humidity / 10.0;
        t = 0.7 * t + 0.3 * local_t; // low pass filter
        h = 0.7 * h + 0.3 * local_h;
        return 0;
    }
    return 2;
}

void addLogData()
{
    if (UNIXTime > 0) {
        if (data.size() > 237) { // max num log points, tune together with json buffer
            data.remove(0);      // recycle
        }
        JsonArray& point = data.createNestedArray();
        currentTime = UNIXTime + (millis() - lastNTPResponse) / 1000;
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

