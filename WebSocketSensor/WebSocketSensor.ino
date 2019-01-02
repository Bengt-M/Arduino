#include <ezTime.h>
#include <ESP8266WiFi.h>
#include <ESP8266WiFiMulti.h>
#include <ArduinoOTA.h>
#include <ESP8266WebServer.h>
#include <WebSocketsServer.h>
//#include <ArduinoJson.h>
#include <FS.h>
#include "Logger.h"
#include "Sensor.h"
#include <U8g2lib.h>

//--- include this with your ssid and password
#include "Password.h"

ADC_MODE(ADC_VCC); //vcc read-mode

ESP8266WiFiMulti wifiMulti;        // Create an instance of the ESP8266WiFiMulti class, called 'wifiMulti'
ESP8266WebServer server(80);       // create a web server on port 80
WebSocketsServer webSocket(81);    // create a websocket server on port 81
File fsUploadFile;                 // a File variable to temporarily store the received file

uint32_t interval = 100; // ms
boolean sleeping = true;
static const uint32_t intervalTempHumidRead = 6000; // ms
uint32_t prevTempHumidRead = 0; // ms
Sensor sensor;
char output[100];

Logger logger;
U8G2_SH1106_128X64_NONAME_2_SW_I2C
u8g2(U8G2_R0, /* SCL=*/ 5, /* SDA=*/ 4, /* reset=*/ U8X8_PIN_NONE);   // ESP32 Thing, pure SW emulated I2C
int secondcount = 1;
char bufferDayName[20];
char bufferDate[20];
char bufferAge[20];
Timezone myLocalTime;
const char minStr[] = "min";
const char maxStr[] = "max";

/*__________________________________________________________SETUP__________________________________________________________*/

void setup()
{
    Serial.begin(115200);        // Start the Serial communication to send messages to the computer
    while (!Serial) {
        delay(1);
    }
    Serial.println("setup()");
    startWiFi();                 // Start a Wi-Fi access point, and try to connect to some given access points. Then wait for either an AP or STA connection
    startOTA();                  // Start the OTA service
    startSPIFFS();               // Start the SPIFFS and list all contents
    startWebSocket();            // Start a WebSocket server
    startServer();               // Start a HTTP server with a file read handler and an upload handler
    Serial.println();
    pinMode(2, OUTPUT);          // GPIO2 (= pin D4) = blue LED
    digitalWrite(2, 1);          // inverted, i.e. light on when 0

    u8g2.begin();

    setInterval(7 * 60);
    setDebug(INFO);
    setServer(NTPServerName);
    myLocalTime.setPosix("CET-1CEST,M3.5.0,M10.5.0/3");
    waitForSync();
    Serial.println(minStr);
}

/*__________________________________________________________LOOP__________________________________________________________*/

inline void draw()
{
    u8g2.setFont(u8g2_font_t0_11_tf);
    u8g2.drawStr(0, 10, bufferDayName);
    u8g2.drawStr(0, 21, bufferDate);
    u8g2.drawStr(0, 50, sensor.temperatureMin);
    u8g2.drawStr(104, 50, sensor.temperatureMax);
    u8g2.drawStr(0, 64, sensor.humidityMin);
    u8g2.drawStr(58, 64, sensor.humidityCurrent);
    u8g2.drawStr(104, 64, sensor.humidityMax);
    u8g2.setFont(u8g2_font_fub20_tn);
    u8g2.drawStr(36, 50, sensor.temperatureCurrent);
    u8g2.setFont(u8g2_font_5x7_tr);
    u8g2.drawStr(0, 38, minStr);
    u8g2.drawStr(104, 38, maxStr);
    u8g2.drawStr(120, 6, bufferAge);
    u8g2.drawFrame(31, 26, 128 - 2 * 31, 28);
}


void loop()
{
    uint32_t currentMillis = millis();
    events();
    webSocket.loop();             // constantly check for websocket events
    server.handleClient();        // run the server
    ArduinoOTA.handle();          // listen for OTA events

    if (secondChanged()) {
        String str = myLocalTime.dateTime("Y-m-d H:i:s");
        memset(bufferDate, 0, 20);
        memcpy(bufferDate, str.c_str(), str.length());
        str = myLocalTime.dateTime("l");
        memset(bufferDayName, 0, 20);
        memcpy(bufferDayName, str.c_str(), str.length());

        yield();
        Serial.print("Sensor.age =");
        Serial.println(sensor.age());
        sprintf(bufferAge, "%d", sensor.age());
        yield();

        // picture loop
        u8g2.firstPage();
        do {
            draw();
        } while (u8g2.nextPage());
        secondcount--;
    }

    /*___________ TempHumid sensor ___________*/
    if ((currentMillis - prevTempHumidRead) >= interval) {
        if (sleeping) {
            digitalWrite(2, 0);
            sensor.wakeup();
            interval = 2;
            sleeping = false;
        } else {
            handleSensorData();
            interval = intervalTempHumidRead;
            sleeping = true;
            digitalWrite(2, 1);
        }
        prevTempHumidRead = currentMillis;
    }

    if (secondcount <= 0) {
        secondcount = 7 * 60;
        logger.addLogData(UTC.now(), sensor.temperatureCurrent, sensor.humidityCurrent);
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
    });
    ArduinoOTA.onEnd([]() {
        Serial.println("End");
        printSPIFFS();
    });
    ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
        Serial.printf("Progress: % u % % \r", (progress / (total / 100)));
    });
    ArduinoOTA.onError([](ota_error_t error) {
        Serial.printf("Error[ % u]: ", error);
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
    server.on(" / edit.html",  HTTP_POST, []() {  // If a POST request is sent to the /edit.html address,
        server.send(200, "text / plain", "");
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
        server.send(404, "text / plain", "404: File Not Found");
    }
}

bool handleFileRead(String path)   // send the right file to the client (if it exists)
{
    Serial.println("handleFileRead: " + path);
    if (path.endsWith(" / ")) path += "index.html";              // If a folder is requested, send the index file
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
        path = upload.filename;
        if (!path.startsWith(" / ")) {
            path = " / " + path;
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
            server.sendHeader("Location", " / success.html");     // Redirect the client to the success page
            server.send(303);
        } else {
            server.send(500, "text / plain", "500: couldn't create file");
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
            }
            break;
        case WStype_TEXT:                     // if new text data is received
            Serial.printf("[%u] get Text: %s\n", num, payload);
            if (payload[0] == 'R') {          // the browser sends an R to reset min and max
                sensor.reset();
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


void handleSensorData()
{
    switch (sensor.read()) {
        case 2:
            Serial.println("Sensor CRC failed");
            break;
        case 1:
            Serial.println("Sensor offline");
            break;
        case 0:
            sprintf(output, "{\"t\":\"%s\",\"tmn\":\"%s\",\"tmx\":\"%s\",\"h\":\"%s\",\"hmn\":\"%s\",\"hmx\":\"%s\"}",
                    sensor.temperatureCurrent, sensor.temperatureMin, sensor.temperatureMax,
                    sensor.humidityCurrent, sensor.humidityMin, sensor.humidityMax);
            Serial.print("output ");
            Serial.println(output);
            // logger.addLogData(UNIXTime, temperature, humidity); // uncomment for testing filling logger quicker
            webSocket.broadcastTXT(output);
            break;
    }
}
