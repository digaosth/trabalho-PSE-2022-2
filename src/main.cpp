//

#include <Arduino.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <esp_ipc.h>
#include <WiFi.h>
#include <ESPmDNS.h>
#include <ArduinoOTA.h>
#include <SPIFFS.h>
#include <WebServer.h>
#include <ESP32SSDP.h>
#include <ESP32TimerInterrupt.h>

#include "webpage.h"

/* #region DEFINES */

const char *HOST = "esp32";            // Host name to mDNS and SSDP
const char *SSID = "BLINK_CASA_RODRIGO";           // Wifi Network SSID
const char *PASSWORD = "uevd1787"; // Wifi Network Password

const long TIMEOUT = 2000; // Define timeout time in milliseconds (example: 2000ms = 2s)

#define LED_PIN 2          // Status LED
#define PIN_DETECT 4
#define TEMP_1_PIN A6
#define TEMP_2_PIN A7

#define VCC 3.3
#define RESISTENCIA_TEMP_1 3420
#define CONSTANTE_A_TEMP_1 0.001129148
#define CONSTANTE_B_TEMP_1 0.000234125
#define CONSTANTE_C_TEMP_1 0.0000000876741

#define FILTRO 40

#define BUFF_SIZE 1024

/* #endregion */

/* #region GLOBAL VARIABLES */
char buffer[BUFF_SIZE+1];

uint16_t
    indexFiltro = 0
;

float
    leituraVcc,
    leituraVcc_Aux,
    leituraVcc_Array[FILTRO],
    leituraVcc2,
    resistencia,
    resistencia2,
    invTemperaturaK,
    invTemperaturaK2,
    temperatura,
    temperatura2,
    setpoint = 20.0
;

volatile int
    count_detected
;

volatile unsigned long
    micros_count,
    micros_count_ant,
    micros_dif,
    micro_rising_edge,
    tempo_pulso,
    tempo_atraso_total
;

/* #endregion */

/* #region DECLARATIONS */
WebServer server(80);
TaskHandle_t _handleTaskWebServer;
TaskHandle_t _handleTaskBlink;
TaskHandle_t _handleTaskAnalogReadings;
hw_timer_t *timer_saida_1 = NULL;
File UploadFile;

portMUX_TYPE mux = portMUX_INITIALIZER_UNLOCKED;

/* #endregion */

void IRAM_ATTR ISR_detect_zero()
{
  portENTER_CRITICAL_ISR(&mux);
  if ((micros() - micros_count) > (2 * tempo_pulso))
  {
    count_detected++;
    micros_count = micros();
    // timerAlarmDisable(timer_saida_1);
    // timerAlarmEnable(timer_saida_1);
  }
  portEXIT_CRITICAL_ISR(&mux);
}

void TaskBlink(void *arg) // Task to handle LED blinking
{
    while (1)
    {
        digitalWrite(LED_PIN, HIGH);
        vTaskDelay(500);
        digitalWrite(LED_PIN, LOW);
        vTaskDelay(500);
    }
    Serial.println("TaskBlink Ended");
    vTaskDelete(NULL);
}

void TaskAnalogReadings(void *arg) // Task to handle Analog Readings
{
    while (1)
    {
        leituraVcc_Array[indexFiltro] = analogRead(TEMP_1_PIN) / 4095.0 * VCC;
        indexFiltro++;
        if (indexFiltro>=FILTRO) indexFiltro = 0;
        leituraVcc_Aux=0;
        for (int i=0; i<FILTRO; i++) leituraVcc_Aux += leituraVcc_Array[i];
        leituraVcc = leituraVcc_Aux / FILTRO;
        resistencia = RESISTENCIA_TEMP_1 * leituraVcc / (VCC - leituraVcc);
        invTemperaturaK = CONSTANTE_A_TEMP_1 + CONSTANTE_B_TEMP_1 * log(resistencia) + CONSTANTE_C_TEMP_1 * pow(log(resistencia), 3);
        if (invTemperaturaK != 0.0) temperatura = -273.15 + 1.0 / invTemperaturaK;

        vTaskDelay(50);
    }
    Serial.println("TaskAnalogReadings Ended");
    vTaskDelete(NULL);
}

void TaskWebServer(void *arg) // Task to run Web Server
{
    while (1)
    {
        server.handleClient();
        vTaskDelay(10);
    }
    Serial.println("TaskWebServer Ended");
    vTaskDelete(NULL);
}

void handleRoot()
{
    server.send(200, "text/html", webpageCode);
}

void SPIFFS_file_download(String filename)
{
    if (SPIFFS.exists("/" + filename))
    {
        File download = SPIFFS.open("/" + filename, FILE_READ);
        filename.replace(" ", "%20");
        server.sendHeader("Content-Type", "text/text");
        server.sendHeader("Content-Disposition", "attachment; filename=" + filename);
        server.sendHeader("Connection", "close");
        server.streamFile(download, "application/octet-stream");
        download.close();
    }
    else
    {
        String temp;
        temp = "<html>\
        <head>\
          <title>ESP32 Demo</title>\
          <style>\
            body { background-color: #cccccc; font-family: Arial, Helvetica, Sans-Serif; Color: #000088; }\
          </style>\
        </head>\
        <body>\
          <h3>File does not exist</h3>\
          <a href='/download'>[Back]</a><br><br>\
        </body>\
      </html>";
        server.send(200, "text/html", temp);
    }
}

void handleJSON()
{
    String results_json = "{ \"temp_1\": " + String(temperatura, 1) +
                         "," + "\"resistencia_1\": " + String(resistencia, 0) +
                         "," + "\"vin_1\": " + String(leituraVcc, 3) +
                         " }";

    server.send(200, "aplication/json", results_json);
}

void File_Download()
{
    if (server.args() > 0)
    { // Arguments were received
        if (server.hasArg("download"))
            SPIFFS_file_download(server.arg(0));
    }
    else
    {
        String temp;
        temp = "<html>\
        <head>\
          <title>ESP32 Demo</title>\
          <style>\
            body { background-color: #cccccc; font-family: Arial, Helvetica, Sans-Serif; Color: #000088; }\
          </style>\
        </head>\
        <body>\
          <h3>Files in ESP32: </h3> \n";

        File root = SPIFFS.open("/");
        File file = root.openNextFile();
        while (file)
        {
            temp += "<p>" + String(file.name()) + "</p>\n";
            file = root.openNextFile();
        }

        temp += "<h3>Enter filename to download</h3>\
          <FORM action='/download' method='post'>\
          <input type='text' name='download' value=''><br>\
          <type='submit' name='download' value=''><br>\
          <a href='/'>[Back]</a>\
        </body>\
      </html>";
        server.send(200, "text/html", temp);
    }
}

void File_Upload()
{
    String temp;
    temp = "<html>\
      <head>\
        <title>ESP32 Demo</title>\
        <style>\
          body { background-color: #cccccc; font-family: Arial, Helvetica, Sans-Serif; Color: #000088; }\
        </style>\
      </head>\
      <body>\
        <h3>Select File to Upload</h3>\
        <p>\
          <FORM action='/fupload' method='post' enctype='multipart/form-data'>\
          <input class='buttons' style='width:40%' type='file' name='fupload' id = 'fupload' value=''><br>\
          <br><button class='buttons' style='width:10%' type='submit'>Upload File</button><br>\
          <a href='/'>[Back]</a><br><br>\
        </p>\
      </body>\
    </html>";
    server.send(200, "text/html", temp);
}

void handleFileUpload()
{
    HTTPUpload &uploadfile = server.upload(); // See https://github.com/esp8266/Arduino/tree/master/libraries/ESP8266WebServer/srcv
                                              // For further information on 'status' structure, there are other reasons such as a failed transfer that could be used
    if (uploadfile.status == UPLOAD_FILE_START)
    {
        String filename = uploadfile.filename;
        if (!filename.startsWith("/"))
            filename = "/" + filename;
        Serial.print("Upload File Name: ");
        Serial.println(filename);
        SPIFFS.remove(filename);                        // Remove a previous version, otherwise data is appended the file again
        UploadFile = SPIFFS.open(filename, FILE_WRITE); // Open the file for writing in SPIFFS (create it, if doesn't exist)
        filename = String();
    }
    else if (uploadfile.status == UPLOAD_FILE_WRITE)
    {
        if (UploadFile)
            UploadFile.write(uploadfile.buf, uploadfile.currentSize); // Write the received bytes to the file
    }
    else if (uploadfile.status == UPLOAD_FILE_END)
    {
        if (UploadFile) // If the file was successfully created
        {
            UploadFile.close(); // Close the file again
            Serial.print("Upload Size: ");
            Serial.println(uploadfile.totalSize);

            String temp;
            temp = "<html>\
          <head>\
            <title>ESP32 Demo</title>\
            <style>\
              body { background-color: #cccccc; font-family: Arial, Helvetica, Sans-Serif; Color: #000088; }\
            </style>\
          </head>\
          <body>\
            <h3>File was successfully uploaded</h3>\
            <h3>Uploaded File Name: " +
                   uploadfile.filename + "</h3>\
            <h3>File Size: " +
                   String(uploadfile.totalSize) + " bytes</h3><br>\
            <a href='/'>[Back]</a><br><br>\
          </body>\
        </html>";
            server.send(200, "text/html", temp);
        }
        else
        {
            String temp;
            temp = "<html>\
          <head>\
            <title>ESP32 Demo</title>\
            <style>\
              body { background-color: #cccccc; font-family: Arial, Helvetica, Sans-Serif; Color: #000088; }\
            </style>\
          </head>\
          <body>\
            <h3>File upload failed</h3>\
            <a href='/'>[Back]</a><br><br>\
          </body>\
        </html>";
            server.send(200, "text/html", temp);
        }
    }
}

void handleNotFound()
{
    String message = "File Not Found\n\n";
    message += "URI: ";
    message += server.uri();
    message += "\nMethod: ";
    message += (server.method() == HTTP_GET) ? "GET" : "POST";
    message += "\nArguments: ";
    message += server.args();
    message += "\n";

    for (uint8_t i = 0; i < server.args(); i++)
    {
        message += " " + server.argName(i) + ": " + server.arg(i) + "\n";
    }

    server.send(404, "text/plain", message);
}

void setup()
{
    vTaskDelay(1000);
    // Blink 3 sec to indicate setup initilization and wait circuits initialize
    pinMode(LED_PIN, OUTPUT); // Configure LED pin
    digitalWrite(LED_PIN, HIGH);
    vTaskDelay(2000);
    digitalWrite(LED_PIN, LOW);

    // Comfigure Serial
    Serial.begin(115200);

    Serial.println("\n/*** Initializing Setup ***/");

    Serial.print("\nInicializing SPIFFS... ");
    if (!SPIFFS.begin(true))
    {
        Serial.println("An Error has occurred while mounting SPIFFS");
        return;
    }
    Serial.println("OK");

    Serial.print("\nConfiguring digital pins... ");
    pinMode(PIN_DETECT, INPUT);
    Serial.println("Ok");


    Serial.print("\nVerifing number of tasks running: ");
    Serial.println(uxTaskGetNumberOfTasks());

    Serial.println("\nInitializing Wifi");

    if (WiFi.status() != WL_CONNECTED)
    {
        Serial.print(".Trying to connect");

        WiFi.begin(SSID, PASSWORD);

        while (WiFi.status() != WL_CONNECTED)
        {
            vTaskDelay(150);
            digitalWrite(LED_PIN, HIGH);
            vTaskDelay(50);
            digitalWrite(LED_PIN, LOW);
            Serial.print(".");
        }
        Serial.println(" Connected");
    }
    else
    {
        Serial.println(".Already Connected");
    }

    Serial.print(".SSID: ");
    Serial.println(WiFi.SSID());
    Serial.print(".IP ADDRESS: ");
    Serial.println(WiFi.localIP());
    Serial.print(".MAC ADDRESS: ");
    Serial.println(WiFi.macAddress());

    Serial.print("\nInitializing OTA... ");
    ArduinoOTA.onStart([]()
                       {
                           vTaskDelete(_handleTaskWebServer);
                           vTaskDelete(_handleTaskBlink);
                           vTaskDelete(_handleTaskAnalogReadings);
                           String type;
                           if (ArduinoOTA.getCommand() == U_FLASH)
                           {
                               type = "sketch";
                           }
                           else
                           { // U_FS
                               type = "filesystem";
                           }
                           // NOTE: if updating FS this would be the place to unmount FS using FS.end()
                           Serial.println("Start updating " + type);
                       });
    ArduinoOTA.onEnd([]()
                     { Serial.println("\nEnd"); });
    ArduinoOTA.onProgress([](unsigned int progress, unsigned int total)
                          {
                              digitalWrite(LED_PIN, HIGH);
                              Serial.printf("Progress: %u%%\r", (progress / (total / 100)));
                              digitalWrite(LED_PIN, LOW);
                          });
    ArduinoOTA.onError([](ota_error_t error)
                       {
                           Serial.printf("Error[%u]: ", error);
                           if (error == OTA_AUTH_ERROR)
                           {
                               Serial.println("Auth Failed");
                           }
                           else if (error == OTA_BEGIN_ERROR)
                           {
                               Serial.println("Begin Failed");
                           }
                           else if (error == OTA_CONNECT_ERROR)
                           {
                               Serial.println("Connect Failed");
                           }
                           else if (error == OTA_RECEIVE_ERROR)
                           {
                               Serial.println("Receive Failed");
                           }
                           else if (error == OTA_END_ERROR)
                           {
                               Serial.println("End Failed");
                           }
                       });
    ArduinoOTA.setHostname(HOST);
    ArduinoOTA.begin();
    Serial.println("OK");
    Serial.printf(".Hostname: %s.local\n", ArduinoOTA.getHostname());

    Serial.print("\nInitializing Web Server...");
    server.begin();
    Serial.println("OK");

    Serial.print("\nStarting Web Server...");
    server.on("/", handleRoot);
    server.on("/readTEMP1", handleJSON);
    server.on("/download", File_Download);
    server.on("/upload", File_Upload);
    server.on(
        "/fupload", HTTP_POST, []()
        { server.send(200); },
        handleFileUpload);
    server.onNotFound(handleNotFound);
    server.on("/description.xml", HTTP_GET, []()
              { SSDP.schema(server.client()); });
    server.begin();
    Serial.println("OK");

    Serial.print("\nStarting task to handle Web Server...");
    xTaskCreatePinnedToCore(TaskWebServer,
                            "TaskWebServer",
                            4096,
                            NULL,
                            4,
                            &_handleTaskWebServer,
                            APP_CPU_NUM);
    Serial.println("OK");

    Serial.print("Starting SSDP... ");
    SSDP.setSchemaURL("description.xml");
    SSDP.setHTTPPort(80);
    SSDP.setName(HOST);
    SSDP.setSerialNumber("001");
    SSDP.setURL("//" + String(HOST));
    SSDP.setModelName("BIT - Brew-It-Yourself");
    SSDP.setManufacturer("Rodrigo");
    SSDP.setManufacturerURL("mailto:digaosth@gmail.com");
    SSDP.setDeviceType("upnp:rootdevice"); //to appear as root device
    SSDP.begin();
    Serial.println("OK");

    Serial.print("\nStarting task to handle Led Blinking...");
    xTaskCreatePinnedToCore(TaskBlink,
                            "TaskBlink",
                            2048,
                            NULL,
                            4,
                            &_handleTaskBlink,
                            APP_CPU_NUM);
    Serial.println("OK");

    Serial.print("\nStarting task to handle Analog Readings...");
    xTaskCreatePinnedToCore(TaskAnalogReadings,
                            "TaskAnalogReadings",
                            2048,
                            NULL,
                            5,
                            &_handleTaskAnalogReadings,
                            APP_CPU_NUM);
    Serial.println("OK");

    Serial.println("Initializing Zero Cross Detector... ");

    //while(digitalRead(PIN_DETECT));
    //while(!digitalRead(PIN_DETECT));
    micro_rising_edge = micros ();
    //while(digitalRead(PIN_DETECT));
    tempo_pulso = micros()-micro_rising_edge;

    Serial.print("Tempo de duracao do pulso na entrada ");
    Serial.println(tempo_pulso);
    Serial.println("\n/*** Setup Completed ***/\n");

    Serial.print("Initializing Interrupts");
    attachInterrupt(digitalPinToInterrupt(PIN_DETECT), ISR_detect_zero, RISING);
    Serial.println("OK");

    // Blink 4 times to indicate setup completed
    digitalWrite(LED_PIN, LOW);
    vTaskDelay(500);
    digitalWrite(LED_PIN, HIGH);
    vTaskDelay(100);
    digitalWrite(LED_PIN, LOW);
    vTaskDelay(100);
    digitalWrite(LED_PIN, HIGH);
    vTaskDelay(100);
    digitalWrite(LED_PIN, LOW);
    vTaskDelay(100);
    digitalWrite(LED_PIN, HIGH);
    vTaskDelay(100);
    digitalWrite(LED_PIN, LOW);
    vTaskDelay(100);
    digitalWrite(LED_PIN, HIGH);
    vTaskDelay(100);
    digitalWrite(LED_PIN, LOW);
}

void loop()
{
    ArduinoOTA.handle(); // Function here to stop Loop while uploading
    
    vTaskDelay(100);
}