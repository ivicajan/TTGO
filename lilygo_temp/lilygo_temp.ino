#include "mcp9808.h"
#include "SD.h"
#include "SPI.h"
// #include "SPIFFS.h"
#include <WiFi.h>
#include <PubSubClient.h>
#include "time.h"

#include <GxFont_GFX.h>
#include <GxEPD.h>
#include <GxIO/GxIO_SPI/GxIO_SPI.h>
#include <GxIO/GxIO.h>
#include <GxGDEH0213B73/GxGDEH0213B73.h>  // 2.13" b/w newer panel

#include GxEPD_BitmapExamples

// FreeFonts from Adafruit_GFX
#include <Fonts/FreeMonoBold9pt7b.h>
#include <Fonts/FreeMonoBold12pt7b.h>
#include <Fonts/FreeMonoBold18pt7b.h>
#include <Fonts/FreeMonoBold24pt7b.h>

#define SPI_MOSI 23
#define SPI_MISO -1
#define SPI_CLK 18

#define ELINK_SS 5
#define ELINK_BUSY 4
#define ELINK_RESET 16
#define ELINK_DC 17

#define SDCARD_SS 13
#define SDCARD_CLK 14
#define SDCARD_MOSI 15
#define SDCARD_MISO 2

#define BUTTON_PIN 39
#define nSamplesFileWrite  300      // Number of samples to store in memory before file write

MCP9808 ts(24);
WiFiClient espClient;
PubSubClient client(espClient);

GxIO_Class io(SPI, /*CS=5*/ ELINK_SS, /*DC=*/ ELINK_DC, /*RST=*/ ELINK_RESET);
GxEPD_Class display(io, /*RST=*/ ELINK_RESET, /*BUSY=*/ ELINK_BUSY);

float previousTemperature = -100.0;
float temperature = 0;
const char *temp_string = "Temperature \n reading setup....";

String csvOutStr = "";                // Buffer for output file
String lastFileWrite = "";
String csvFileName="/temp_data.csv";
File file;                            // Data file for the SPIFFS output
int nsamples;                         // Counter for the number of samples gathered
bool sdOK = false;

// Replace the next variables with your SSID/Password combination
bool wifiOK = true;
const char* ssid = "naia_2g";
const char* password = "";
const char* ntpServer = "pool.ntp.org";
const long  gmtOffset_sec = 8*3600;
const long  daylightOffset_sec = 0;
char datestring[19];

// B. MTQQ Setup: PubSubClient
String mqtt_server = "192.168.1.101";  // piHub
long lastMsg = 0;
char msg[20];
String TOPIC = "LilyGo";
String TOPICwait = "LilyGo";
String CLIENTID = "LilyGo";

/////////////////////////////////////////////////////////////////////
void setup()
{
  Serial.begin(115200);
  
  // MCP9808 resolution for temp sensor  
  ts.setResolution(3);

  // DISPLAY INIT
  SPI.begin(SPI_CLK, SPI_MISO, SPI_MOSI, ELINK_SS);
  display.init();
  display.setRotation(1);
  display.fillScreen(GxEPD_WHITE);
  display.setTextColor(GxEPD_BLACK);
  display.setFont(&FreeMonoBold9pt7b);
  
  // SD Card setup
  display.setCursor(5, 10);
  display.println("SD Card status: ");
  Serial.println("SD Card status: ");
  SPI.begin(SDCARD_CLK, SDCARD_MISO, SDCARD_MOSI, SDCARD_SS);
  if (!SD.begin(SDCARD_SS, SPI)){
  Serial.println("No SD CARD available");
  display.println("No SD CARD available");
  } else {
  sdOK = true;
  Serial.println("SD CARD available");
  display.println("SD CARD available");
  }
  display.updateWindow(0, 0,  249,  127, true);
  delay(1000);
  
  // test WIFi and print available networks
  display.fillScreen(GxEPD_WHITE);
  testWiFi();
  delay(1000);

  // Connect to local network
  display.fillScreen(GxEPD_WHITE);
  setup_wifi();
  delay(1000);

  //get the time from NTP server using wifi
  if (wifiOK) {
  configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);
  printLocalTime();
  }

  // MQTT - set topic based on MAC address
  byte buf[6];
  char macAdd[12];
  WiFi.macAddress(buf);
  sprintf(macAdd, "%02X%02X%02X%02X%02X%02X", buf[0], buf[1], buf[2], buf[3], buf[4], buf[5]);
  CLIENTID = macAdd;
  printDebug("Client ID set to: " + String(macAdd));
  if (String(macAdd) == "30AEA421E24C") {
    TOPIC = "gwen/test/test";
    TOPICwait = "gwen/listening/test";
  }
  if (String(macAdd) == String("F008D1C930BC")) {
    TOPIC = "OtherLily";
    TOPICwait = "OtherLily/listen";
  }  
  
  // MQTT:  configure the MQTT server with IPaddress and port
  client.setServer(mqtt_server.c_str(), 1883);
  client.setCallback(receivedCallback); // callback for subscribed topic
  if (!client.connected()) {  // if client was disconnected then try to reconnect again
    mqttconnect();
  }
}

//////////////////////////////////////////////////////////////////

void loop()
{  

  previousTemperature = temperature;
  temperature = ts.getTemperature();

  // update temperature readings if different from the old value 
  if(previousTemperature!=temperature)
  {
    if (wifiOK) {
    printLocalTime();
    }
    showPartialUpdate(temperature);
    String savePacket = String(nsamples) + "," + String(temperature) + "\n";
    csvOutStr += savePacket;
    nsamples += 1;
    
  }
  
  // only write after collecting a good number of samples
  if (nsamples > nSamplesFileWrite) {  
    Serial.println("Writing data on the SD card");
  //  writeData2SDcard();
    nsamples = 0;
    }
    delay(1000);
}


//////////////////////////////////////////////////////////////////
// 
//     LOCAL FUNCTIONS
//

void printLocalTime()
{
  struct tm timeinfo;
  if(!getLocalTime(&timeinfo)){
    Serial.println("Failed to obtain time");
    return;
  }
  strftime(datestring,19, "%Y/%m/%d %H:%M:%S", &timeinfo);
  Serial.println(datestring);
}

void setup_wifi() {
  delay(10);
  // We start by connecting to a WiFi network
  Serial.println();
  Serial.print("Connecting to ");
  Serial.println(ssid);
  display.setRotation(1);
  display.fillScreen(GxEPD_WHITE);
  display.setTextColor(GxEPD_BLACK);
  display.setFont(&FreeMonoBold9pt7b);
  display.setCursor(5, 10);
  display.println("Connecting to: ");
  display.println(ssid);
  display.updateWindow(0, 0,  249,  127, true);
  WiFi.begin(ssid, password);
  int i = 0;
  while ((WiFi.status() != WL_CONNECTED) & (i < 10)) {
    delay(500);
    i++;
    Serial.print(".");
    display.print(".");
    display.updateWindow(0, 0,  249,  127, true);
    wifiOK = false;
  }
  if (wifiOK) {
  Serial.println("");
  Serial.println("WiFi connected");
  Serial.println("IP address: ");
  Serial.println(WiFi.localIP());
  display.println("\n");
  display.println("WiFi connected");
  display.println("IP address: ");
  display.println(WiFi.localIP());
  } else {
  Serial.println("");
  Serial.println("WiFi not connected");
  display.println("\n");
  display.println("WiFi not connected"); 
  }
  display.updateWindow(0, 0,  249,  127, true);
  delay(1000);
}

void testWiFi()
{
    WiFi.mode(WIFI_STA);
    WiFi.disconnect();
    // WiFi.scanNetworks will return the number of networks found
    int n = WiFi.scanNetworks();
    Serial.println("scan done");
    display.setRotation(1);
    display.fillScreen(GxEPD_WHITE);
    display.setTextColor(GxEPD_BLACK);
    display.setFont(&FreeMonoBold9pt7b);
    display.setCursor(5, 10);
    display.println("scan done");
    if (n == 0) {
        Serial.println("no networks found");
    } else {
        Serial.print(n);
        Serial.println(" networks found");
        display.print(n);
        display.println(" networks found");
        for (int i = 0; i < n; ++i) {
            // Print SSID and RSSI for each network found
            Serial.print(i + 1);
            Serial.print(": ");
            Serial.print(WiFi.SSID(i));
            Serial.print(" (");
            Serial.print(WiFi.RSSI(i));
            Serial.print(")");
            Serial.println((WiFi.encryptionType(i) == WIFI_AUTH_OPEN) ? " " : "*");
            display.print(i + 1);
            display.print(": ");
            display.print(WiFi.SSID(i));
            display.print(" (");
            display.print(WiFi.RSSI(i));
            display.print(")");
            display.println((WiFi.encryptionType(i) == WIFI_AUTH_OPEN) ? " " : "*");
            delay(10);
        }
    }
    Serial.println("");
    display.println("");
    display.updateWindow(0, 0,  249,  127, true);
    delay(1000);
}

void writeData2SDcard (){
  file = SD.open(csvFileName, FILE_APPEND);
  display.setRotation(1);
  display.fillScreen(GxEPD_WHITE);
  display.setTextColor(GxEPD_BLACK);
  display.setFont(&FreeMonoBold9pt7b);
  display.setCursor(10, 10);
  if (!file) {
    Serial.println("There was an error opening the file for writing");
    display.println("There was an error opening the file for writing");
    lastFileWrite = "FAILED OPEN";
  } else {
    if (file.println(csvOutStr)) {
      file.close();
      csvOutStr = ""; nsamples = 0;
      lastFileWrite = String(nsamples);  
    } else {
      lastFileWrite = "FAILED WRITE";
    }
  }
  display.updateWindow(0, 0,  249,  127, true);
  delay(1000);
}

void showPartialUpdate(float temperature)
{
  Serial.println("Updating display ...");
  String temperatureString = String(temperature,2);
  Serial.println(temperatureString);
  display.setRotation(1);
  display.fillScreen(GxEPD_WHITE);
  display.setTextColor(GxEPD_BLACK);
  display.setCursor(20, 40);
  display.setFont(&FreeMonoBold24pt7b);
  display.print(temperatureString);
  display.setCursor(170, 40);
  display.setFont(&FreeMonoBold12pt7b);
  display.print("degC");
  if (wifiOK) {
  display.setCursor(20, 70);
  display.print(ssid);
  display.setCursor(20, 90);
  display.setFont(&FreeMonoBold9pt7b);
  display.print(datestring);
  }
//  display.updateWindow(0, 0, GxEPD_WIDTH, GxEPD_HEIGHT, true);
  display.updateWindow(0, 0,  249,  127, true);
}
