// 0. Includes
#include <WiFi.h>
#include <PubSubClient.h>
#include "time.h"

// A. WiFI Setup
const char* ssid = "naia_2g";
const char* password = "";
const char* ntpServer = "pool.ntp.org";
const long  gmtOffset_sec = 8*3600;
const long  daylightOffset_sec = 0;
char datestring[19];
bool wifiOK;

// B. MTQQ Setup: PubSubClient
String mqtt_server = "192.168.1.101";  // piHub

WiFiClient espClient;
PubSubClient client(espClient);
long lastMsg = 0;
char msg[20];
String TOPIC = "dnevna_soba";
String TOPICwait = "LabLily";
String CLIENTID = "LabLily";

// C. Temperature Sensor Header
#include <Wire.h>
#include "Adafruit_MCP9808.h"
Adafruit_MCP9808 tempsensor = Adafruit_MCP9808();  // Create the MCP9808 temperature sensor object

// D. PINS setup
//const int relayPIN = 1;

// E. Debug output setup
String debugOut = "Serial";
int debugSetup = 0;

// F. Deep Sleep Header
#define uS_TO_S_FACTOR 1000000  /* Conversion factor for micro seconds to seconds */
RTC_DATA_ATTR int bootCount = 0;
RTC_DATA_ATTR int sleepTime = 600; /* Time ESP32 will go to sleep (in seconds) */

// include library, include base class, make path known
#include <GxEPD.h>
#include "SD.h"
#include "SPI.h"

#include <GxGDEH0213B73/GxGDEH0213B73.h>  // 2.13" b/w newer panel

// FreeFonts from Adafruit_GFX
#include <Fonts/FreeMonoBold9pt7b.h>
#include <Fonts/FreeMonoBold12pt7b.h>
#include <Fonts/FreeMonoBold18pt7b.h>
#include <Fonts/FreeMonoBold24pt7b.h>

#include <GxIO/GxIO_SPI/GxIO_SPI.h>
#include <GxIO/GxIO.h>

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


GxIO_Class io(SPI, /*CS=5*/ ELINK_SS, /*DC=*/ ELINK_DC, /*RST=*/ ELINK_RESET);
GxEPD_Class display(io, /*RST=*/ ELINK_RESET, /*BUSY=*/ ELINK_BUSY);

SPIClass sdSPI(VSPI);

const char *skuNum = "JDM SKU:H239";
bool sdOK = false;

/********** MAIN SETUP */
void setup() {
  printDebug("In Setup...");
  btStop();

  // Increment boot number and print it every reboot
  ++bootCount;
  printDebug(".. boot count: " + String(bootCount));

  //-------------- ePaper
  SPI.begin(SPI_CLK, SPI_MISO, SPI_MOSI, ELINK_SS);
  display.init(); // enable diagnostic output on Serial

  // INIT Wifi
  WiFi.begin(ssid, password);
  WiFi.mode(WIFI_STA);
  int tryCount = 0; int maxTries = 10;
  int totalCount = 0; int maxTotalTry = 10;
  while ((WiFi.status() != WL_CONNECTED) && (totalCount < maxTotalTry)) {
    delay(500);
    Serial.print(".");
    tryCount += 1;
    if (tryCount > maxTries) {
      // esp_sleep_enable_timer_wakeup(10 * uS_TO_S_FACTOR);
      esp_sleep_enable_timer_wakeup(60 * uS_TO_S_FACTOR);
      printDebug(".. going to sleep for 1 min to try to reconnect to WiFi ");
      delay(20);
      wifiOK = false;
      totalCount += 1;
      esp_deep_sleep_start();
    }
  }
if (totalCount < maxTotalTry) {
  wifiOK = true;
  Serial.println("");
  Serial.println("WiFi connected");
  Serial.println("IP address: ");
  Serial.println(WiFi.localIP());
  Serial.println("getting NTP time");
  configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);
  printLocalTime();
  
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
    TOPIC = "dnevna_soba/temp";
    TOPICwait = "dnevna_soba/listen";
  }
  if (String(macAdd) == "30AEA4250034") {
    TOPIC = "gwen/temp/unknown";
    TOPICwait = "gwen/listening/temp/unknown";
  }
  if (String(macAdd) == String("7C9EBDFB05E0")) {
    TOPIC = "LabLily";
    TOPICwait = "LabLily/listen";
  }
  if (String(macAdd) == "30AEA4250060") {
    TOPIC = "gwen/temp/upperroof";
    TOPICwait = "gwen/listening/temp/upperroof";
  }
  if (String(macAdd) == "3C71BF1E0EDC") {
    TOPIC = "gwen/temp/jkbedroom";
    TOPICwait = "gwen/listening/temp/jkbedroom";
  }
  if (String(macAdd) == "30AEA424FFCC") {
    TOPIC = "gwen/temp/alyssa";
    TOPICwait = "gwen/listening/temp/alyssa";
  }
  if (String(macAdd) == "30AEA424FFE4") {
    TOPIC = "gwen/temp/outsideback";
    TOPICwait = "gwen/listening/temp/outsideback";
  }
  if (String(macAdd) == "30AEA424F7B0") {
    TOPIC = "gwen/temp/lowerroof";
    TOPICwait = "gwen/listening/temp/lowerroof";
  }
  if (String(macAdd) == "B4E62DB28981") {
    TOPIC = "gwen/temp/den";
    TOPICwait = "gwen/listening/temp/den";
  }

  // MQTT:  configure the MQTT server with IPaddress and port
  client.setServer(mqtt_server.c_str(), 1883);
  client.setCallback(receivedCallback); // callback for subscribed topic
  if (!client.connected()) {  // if client was disconnected then try to reconnect again
    mqttconnect();
  }
  
    // Do this to enable to receive and respond to any messages
    printDebug("Checking for messages");
    client.publish(TOPICwait.c_str(), "yes");
    for (int i = 0;i<1000;i++ ){
      client.loop(); // listen for incomming subscribed topic-process-invoke receivedCallback
      delay(10);
    }
    client.publish(TOPICwait.c_str(), "no");
    printDebug("Done .. checking for messages");
 
}
  // Get and Publish Temperature
  printDebug(".. sense temperature");
  int cnt = 0;
  while (!tempsensor.begin(0x18)) {
    printDebug("Couldn't find MCP9808!");
    cnt++;
    if (cnt > 10) {
      return;
    }
    delay(1000);
  }
  tempsensor.shutdown_wake(0);
  delay(250);
  float c = tempsensor.readTempC();
  c = tempsensor.readTempC();
  snprintf (msg, 20, "%lf", c);

  msg[4]='\0';
  display.setRotation(1);
  display.fillScreen(GxEPD_WHITE);
  display.setTextColor(GxEPD_BLACK);  
  display.setFont(&FreeMonoBold12pt7b);
  display.setCursor(10, 15);
  display.print(TOPIC.c_str());
  display.setCursor(40, 65);
  display.setFont(&FreeMonoBold24pt7b);
  display.print(msg);
  display.setFont(&FreeMonoBold12pt7b);
  display.setCursor(160, 65);
  display.print("degC");
  if (wifiOK) {
  display.setFont(&FreeMonoBold9pt7b);
  display.setCursor(60, 90);
  display.print(ssid);
  display.setCursor(20, 110);
  display.print(datestring);
  client.publish(TOPIC.c_str(), msg);
  }  
  display.updateWindow(0, 0,  249,  127, true);
  delay(500);


  esp_sleep_enable_timer_wakeup(sleepTime * uS_TO_S_FACTOR);
  printDebug(".. going to sleep now");
  delay(20);
  esp_deep_sleep_start();

}

/********** MAIN LOOP */
void loop() {

// never get here as it is sleeping
}

void printLocalTime()
{
  struct tm timeinfo;
  if(!getLocalTime(&timeinfo)){
    Serial.println("Failed to obtain time");
    return;
  }
  strftime(datestring, 19, "%y/%m/%d %H:%M:%S", &timeinfo);
  Serial.println(datestring);
}

/* MQTT Message Recd Callback */
void receivedCallback(char* topic, byte* payload, unsigned int length) {
  printDebug("Message received: " + String(topic));
  printDebug("payload: ");
  for (int i = 0; i < length; i++) {
    Serial.print((char)payload[i]);
  }
  payload[length] = '\0'; // Make payload a string by NULL terminating it.
  int pwmVal = atoi((char *)payload);
  printDebug("Payload String is" + String(pwmVal));
  sleepTime = pwmVal;
}


/* MQTT Connect */
void mqttconnect() {
  /* Loop until reconnected */
  int tryCount = 0; int maxTries = 10;
  while (!client.connected()) {
    printDebug("MQTT connecting ...");
    /* client ID */
    String clientId = CLIENTID;
    /* connect now */
    if (client.connect(clientId.c_str(), NULL, NULL, NULL, NULL, NULL, NULL, false)) {
      printDebug("connected");
      /*
        // subscribe topic with default QoS 0
        String updateTOPIC = String(TOPIC) + String("/intervalSec");
        char *cstr = new char[updateTOPIC.length() + 1];
        strcpy(cstr, updateTOPIC.c_str());
        printDebug(cstr);
        client.subscribe(cstr,1);
      */
    } else {
      printDebug("failed, status code =" + String(client.state()));
      printDebug("try again in 5 seconds");
      delay(5000); /* Wait 5 seconds before retrying */
    }
    tryCount += 1;
    if (tryCount > maxTries) {
      esp_sleep_enable_timer_wakeup(5 * 60 * uS_TO_S_FACTOR);
      printDebug(".. going to sleep for 5 min to try to reconnect to MQTT "); // this avoids flatening the battery
      delay(20);
      esp_deep_sleep_start();
    }
  }
}

/* Print to Debug/Serial */
void printDebug(String txt) {
  if (debugSetup == 0) {
    if (debugOut == "Serial") {
      Serial.begin(115200);
      delay(1000); //Take some time to open up the Serial Monitor
    }
    debugSetup = 1;
  }
  if (debugOut == "Serial") {
    Serial.println(txt);
  } else {
    Serial.println(txt);
  }
}


String ipToString(IPAddress ip) {
  String s = "";
  for (int i = 0; i < 4; i++)
    s += i  ? "." + String(ip[i]) : String(ip[i]);
  return s;
}
