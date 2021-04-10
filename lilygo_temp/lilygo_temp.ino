#include "mcp9808.h"
#include "SD.h"
#include "SPI.h"
// #include "SPIFFS.h"
#include <WiFi.h>
#include <PubSubClient.h>

#include <GxFont_GFX.h>
#include <GxEPD.h>
#include <GxIO/GxIO_SPI/GxIO_SPI.h>
#include <GxIO/GxIO.h>
#include <GxGDEH0213B73/GxGDEH0213B73.h>  // 2.13" b/w newer panel
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
const char* ssid = "ssid";
const char* password = "password";

// Add your MQTT Broker IP address, example:
const char* mqtt_server = "192.168.1.101";

WiFiClient espClient;
PubSubClient client(espClient);


void setup_wifi() {
  delay(10);
  // We start by connecting to a WiFi network
  Serial.println();
  Serial.print("Connecting to ");
  Serial.println(ssid);

  WiFi.begin(ssid, password);

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  Serial.println("");
  Serial.println("WiFi connected");
  Serial.println("IP address: ");
  Serial.println(WiFi.localIP());
}

void testWiFi()
{
    WiFi.mode(WIFI_STA);
    WiFi.disconnect();
    // WiFi.scanNetworks will return the number of networks found
    int n = WiFi.scanNetworks();

    Serial.println("scan done");
    if (n == 0) {
        Serial.println("no networks found");
    } else {
        Serial.print(n);
        Serial.println(" networks found");
        for (int i = 0; i < n; ++i) {
            // Print SSID and RSSI for each network found
            Serial.print(i + 1);
            Serial.print(": ");
            Serial.print(WiFi.SSID(i));
            Serial.print(" (");
            Serial.print(WiFi.RSSI(i));
            Serial.print(")");
            Serial.println((WiFi.encryptionType(i) == WIFI_AUTH_OPEN) ? " " : "*");
            delay(10);
        }
    }
    Serial.println("");
}

void writeData2Flash (){
  file = SD.open(csvFileName, FILE_APPEND);
  if (!file) {
    Serial.println("There was an error opening the file for writing");
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
  Serial.println(__FILE__);
  SPI.begin(SPI_CLK, SPI_MISO, SPI_MOSI, ELINK_SS);
  display.init(); // enable diagnostic output on Serial
  ts.setResolution(3);
  display.setRotation(1);
  display.fillScreen(GxEPD_WHITE);

  Serial.print("SD Card status: ");
  sdSPI.begin(SDCARD_CLK, SDCARD_MISO, SDCARD_MOSI, SDCARD_SS);
  if (!SD.begin(SDCARD_SS, sdSPI)){
  sdOK = false;
  Serial.println("No SD CARD available");
  } else {
  sdOK = true;
  Serial.println("SD CARD available");
  }

// G. SPIFFS to write data to onboard Flash
  if (!SPIFFS.begin(true)) {
    Serial.println("An Error has occurred while mounting SPIFFS - need to add retry");
    while (1);
  }

  csvFileName="/temp_data.csv";

// test WIFi and print available networks
  testWiFi();
  
//  display.drawExampleBitmap(Bdash_bits, 30, 30, 32, 32, GxEPD_BLACK);
//  delay(5000);
  display.setTextColor(GxEPD_BLACK);
  display.setFont(&FreeMonoBold12pt7b);
  display.setCursor(10,10);
  display.println(temp_string);
  display.fillScreen(GxEPD_WHITE);
  display.update();
}

void loop()
{  

  previousTemperature = temperature;
  temperature = ts.getTemperature();

  if(previousTemperature!=temperature)
  {
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
  writeData2Flash();
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
  display.setFont(&FreeMonoBold12pt7b);
  display.setCursor(20, 20);
  display.print("Temp:");
  display.setCursor(30, 80);
  display.setFont(&FreeMonoBold24pt7b);
  display.print(temperatureString);
  display.setCursor(180, 80);
  display.setFont(&FreeMonoBold12pt7b);
  display.print("degC");
  display.updateWindow(10, 10,  240,  100, true);
}
