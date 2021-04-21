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

float previousTemperature = -100.0;
float temperature = 0;
String csvOutStr = "";                // Buffer for output file
String lastFileWrite = "";
String csvFileName = "";
File file;                            // Data file for the SPIFFS output
int nsamples;                         // Counter for the number of samples gathered
int myear, mmonth, mday;
int mhour, mminute, msecond;

GxIO_Class io(SPI, /*CS=5*/ ELINK_SS, /*DC=*/ ELINK_DC, /*RST=*/ ELINK_RESET);
GxEPD_Class display(io, /*RST=*/ ELINK_RESET, /*BUSY=*/ ELINK_BUSY);

const char *temp_string = "Temperature \n reading setup....";
bool sdOK = false;
// Replace the next variables with your SSID/Password combination
const char* ssid = "naia_2g";
const char* password = "Malinska";
const char* ntpServer = "pool.ntp.org";
const long  gmtOffset_sec = 8*3600;
const long  daylightOffset_sec = 0;
char datestring[19];

WiFiClient espClient;
PubSubClient client(espClient);
// B. MTQQ Setup: PubSubClient
String mqtt_server = "192.168.1.101";  // piHub
long lastMsg = 0;
char msg[20];
String TOPIC = "LabLily";
String TOPICwait = "LabLily";
String CLIENTID = "LabLily";

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
  display.setCursor(20, 20);
  display.print("Connecting to: ");
  display.setCursor(20, 35);
  display.println(ssid);
  display.setCursor(20, 50);
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
    display.print(".");
  }

  Serial.println("");
  Serial.println("WiFi connected");
  Serial.println("IP address: ");
  Serial.println(WiFi.localIP());
  display.println("\n");
  display.println("WiFi connected");
  display.setCursor(20, 75);
  display.println("IP address: ");
  display.setCursor(20, 90);  
  display.println(WiFi.localIP());
  display.updateWindow(10, 10,  240,  100, true);
}

void testWiFi()
{
    WiFi.mode(WIFI_STA);
    WiFi.disconnect();
    // WiFi.scanNetworks will return the number of networks found
    int n = WiFi.scanNetworks();
    Serial.println("scan done");
    display.setCursor(20, 20);
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
}

void writeData2Flash (){
  file = SD.open(csvFileName, FILE_APPEND);
  if (!file) {
    Serial.println("There was an error opening the file for writing");
    display.println("There was an error opening the file for writing");
    lastFileWrite = "FAILED OPEN";
  } else {
    if (file.println(csvOutStr)) {
      file.close();
      csvOutStr = ""; nsamples = 0;
      //lastFileWrite = String(mhour, DEC) + ":" + String(mminute, DEC) + ":" + String(msecond, DEC);
      lastFileWrite = String(nsamples);  
    } else {
      lastFileWrite = "FAILED WRITE";
    }
  }
}

void setup()
{
  Serial.begin(115200);
  ts.setResolution(3);
  display.init();
  display.setRotation(1);
  display.fillScreen(GxEPD_WHITE);
  display.drawExampleBitmap(BitmapExample1, 0, 0, 200, 100, GxEPD_BLACK);
  delay(1000);
  display.fillScreen(GxEPD_WHITE);
  display.setTextColor(GxEPD_BLACK);
  display.setFont(&FreeMonoBold12pt7b);
  display.setCursor(10, 10);
  display.println("SD Card status: ");
  Serial.println("SD Card status: ");
  SPI.begin(SDCARD_CLK, SDCARD_MISO, SDCARD_MOSI, SDCARD_SS);
  if (!SD.begin(SDCARD_SS, SPI)){
  sdOK = false;
  Serial.println("No SD CARD available");
  display.setCursor(20, 10);
  display.println("No SD CARD available");
  } else {
  sdOK = true;
  Serial.println("SD CARD available");
  display.setCursor(30, 10);
  display.println("SD CARD available");
  delay(1000);
  display.updateWindow(10, 10,  240,  100, true);
  }

// G. SPIFFS to write data to onboard Flash
//  if (!SPIFFS.begin(true)) {
//    Serial.println("An Error has occurred while mounting SPIFFS - need to add retry");
//    while (1);
//  }

  csvFileName="/temp_data.csv";

// test WIFi and print available networks
  display.fillScreen(GxEPD_WHITE);
  testWiFi();
  delay(5000);
  display.fillScreen(GxEPD_WHITE);
  setup_wifi();
  delay(5000);
  display.fillScreen(GxEPD_WHITE);

  //init and get the time
  configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);
  printLocalTime();
  
}

void loop()
{  

  previousTemperature = temperature;
  temperature = ts.getTemperature();

  if(previousTemperature!=temperature)
  {
     printLocalTime();
     showPartialUpdate(temperature);
//    String tDate = String(gps.date.year()) + "-" + String(gps.date.month()) + "-" + String(gps.date.day());
//    String tTime = String(gps.time.hour()) + ":" + String(gps.time.minute()) + ":" + String(gps.time.second());
//    String sendPacket = tDate + "," + tTime + "," + String(temperature) + "\n";
    String sendPacket = String(nsamples) + "," + String(temperature) + "\n";
    csvOutStr += sendPacket;
    nsamples += 1;
    
  }
  Serial.println(String(nsamples));
  if (nsamples > nSamplesFileWrite) {  // only write after collecting a good number of samples
//  writeData2Flash();
    }
    delay(5000);
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
  display.setCursor(20, 70);
  display.print(ssid);
  display.setCursor(20, 90);
  display.setFont(&FreeMonoBold9pt7b);
  display.print(datestring);
  display.updateWindow(10, 10,  240,  100, true);
}
