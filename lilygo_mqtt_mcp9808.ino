#include <WiFi.h>
#include <PubSubClient.h>
#include "time.h"
#include "SD.h"
#include "FS.h"
#include "SPI.h"

#include "esp_adc_cal.h"
#define BATT_PIN 35
#define BATTV_MAX    4.1     // maximum voltage of battery
#define BATTV_MIN    3.2     // what we regard as an empty battery
#define BATTV_LOW    3.4     // voltage considered to be low battery

// WiFI Setup
const char* ssid = "change to yours";
const char* password = "change to yours";
const char* ntpServer = "pool.ntp.org";
const long  gmtOffset_sec = 8*3600;  // in my case, dirty hack
const long  daylightOffset_sec = 0;  // in my case
char datestring[19];
bool wifiOK;
bool sdOK;


// MTQQ Setup: PubSubClient
const char* mqtt_server = "192.168.1.101";   // my rapi with mosquitto

WiFiClient espClient;
PubSubClient client(espClient);

long lastMsg = 0;
char tempString[20];
String TOPIC = "dnevna_soba";
String TOPICwait = "listen";
String CLIENTID = "esp32_dnevna";
String dataMessage;

// MCP9808
#include <Wire.h>
#include "Adafruit_MCP9808.h"
Adafruit_MCP9808 tempsensor = Adafruit_MCP9808();  // Create the MCP9808 temperature sensor object

// Deep Sleep Header
#define uS_TO_S_FACTOR 1000000  /* Conversion factor for micro seconds to seconds */
RTC_DATA_ATTR int bootCount = 0;
RTC_DATA_ATTR int sleepTime = 600; /* Time ESP32 will go to sleep (in seconds) */

// Save reading number on RTC memory
RTC_DATA_ATTR int readingID = 0;

// ePaper display  LilyGo T5 with 2.13 display
#include <GxEPD.h>
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

float temperature = 0;
int vref = 1100;

//********** MAIN SETUP ***********
// all is happening here, not using loop as ESP32 is goint to sleep mode 
void setup() {

  // start terminal for serial outputs for debugging
  Serial.begin(115200);
  while (!Serial); //waits for serial terminal to be open, necessary in newer arduino boards.

  // Increment boot number and print it every reboot
  ++bootCount;
  Serial.println(".. boot count: " + String(bootCount));

  // Get battery levels
  esp_adc_cal_characteristics_t adc_chars;
  esp_adc_cal_value_t val_type = esp_adc_cal_characterize((adc_unit_t)ADC_UNIT_1, 
  (adc_atten_t)ADC_ATTEN_DB_2_5, (adc_bits_width_t)ADC_WIDTH_BIT_12, 1100, &adc_chars);
  float measurement = (float) analogRead(35);
  float battery_voltage = (measurement / 4095.0) * 7.05;   
  String voltage = "Voltage: " + String(battery_voltage) + "V";
  uint8_t battpc = (((battery_voltage - BATTV_MIN) / (BATTV_MAX - BATTV_MIN)) * 100);
  String batt = "Battery " + String(battpc) +"%";
  Serial.println(voltage);
  Serial.println(batt);

  // Start MCP9808 temp sensor
  Serial.println("Starting ... MCP9808"); 
  if (!tempsensor.begin(0x18)) {
    Serial.println("Couldn't find MCP9808! Check your connections and verify the address is correct.");
    while (1);
  }
    
  Serial.println("Found MCP9808!");
  tempsensor.setResolution(3); // sets the resolution mode of reading, the modes are defined in the table bellow:
    // Mode Resolution SampleTime
    //  0    0.5°C       30 ms
    //  1    0.25°C      65 ms
    //  2    0.125°C     130 ms
    //  3    0.0625°C    250 ms
  delay(100);
  Serial.println("Wake up MCP9808.... "); // wake up MCP9808 - power consumption ~200 mikro Ampere
  tempsensor.wake();   // wake up, ready to read!
  // Read and print out the temperature
  temperature = tempsensor.readTempC();
  dtostrf(temperature, 1, 2, tempString);
  Serial.print("Temp: "); 
  Serial.println(tempString);   
  Serial.println("Shutting down MCP9808.... ");
  tempsensor.shutdown_wake(1); // shutdown MSP9808 - power consumption ~0.1 mikro Ampere, stops temperature sampling
  delay(20);
  
  // INIT Wifi and kill BT
  btStop();  // kill BT
  delay(100);
  Serial.println();
  Serial.print("Connecting to ");
  Serial.println(ssid);

  WiFi.begin(ssid, password);

  int count=0;
  while (WiFi.status() != WL_CONNECTED) {
    delay(100);
    count++;
    Serial.print(".");
    /* I had some cases where the esp was stuck in connection to wifi mode */
    if (count > 100) {
      Serial.println();
      Serial.println("something bad happened, trying to reset");
      ESP.restart();
      wifiOK = false;
    }
  }
 if (WiFi.status() == WL_CONNECTED) { 
  wifiOK = true;
  Serial.println("***************************");
  Serial.println("WiFi connected");
  Serial.println("IP address: ");
  Serial.println(WiFi.localIP());
  Serial.println("***************************");
  }
  delay(20);

  // Get current date/time if connected
  if(wifiOK){  
    Serial.println("Getting NTP time");
    configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);
    delay(100);
    printLocalTime();
   } else {
    Serial.println("No wifi and no date/time"); 
   }
  delay(20);

  // Initialize SD card if you want, skipping for now
//  Serial.println('SD init');
//  SD_init();
//  delay(20);
//  Serial.println('SD done');
  
  // MQTT:  configure the MQTT server with IPaddress and port
  Serial.println("MQTT init");
  client.setServer(mqtt_server, 1883);
  if (!client.connected()) {  // if client was disconnected then try to reconnect again
    reconnect();
  }
  // need that, start MQTT client loop
  client.loop();
  client.publish("dnevna_soba/temp", tempString);  // publish data        
  delay(100);
  client.unsubscribe("esp32/output");
  client.disconnect();
  delay(20);
  Serial.println("MQTT done");

  //-------------- ePaper
  Serial.println("Display init");
  SPI.begin(SPI_CLK, SPI_MISO, SPI_MOSI, ELINK_SS);
  display.init(); 
  // Display on screen
  display.setRotation(1);
  display.fillScreen(GxEPD_WHITE);
  display.setTextColor(GxEPD_BLACK);  
  display.setFont(&FreeMonoBold12pt7b);
  display.setCursor(10, 15);
  display.print(TOPIC.c_str());
  display.setCursor(40, 65);
  display.setFont(&FreeMonoBold24pt7b);
  display.print(tempString);
  display.setFont(&FreeMonoBold12pt7b);
  display.setCursor(185, 65);
  display.print("C");
  if (wifiOK) {
  display.setFont(&FreeMonoBold9pt7b);
  display.setCursor(60, 90);
  display.print(batt.c_str());
  display.setCursor(20, 110);
  display.print(datestring);
  }  
  display.updateWindow(0, 0,  249,  127, true);
  delay(500);
  Serial.println("Display done");

 //Append the data to file at SD card
//  if (sdOK) {
//  Serial.println('Writing to SD card');  
//  logSDCard(); 
  // Increment readingID on every new reading
//  readingID++;
//  }
//  delay(20);
//  Serial.println('SD write done');

  // Prepare for sleeping  
  Serial.println("Preparing for sleeping");
  esp_sleep_enable_timer_wakeup(sleepTime * uS_TO_S_FACTOR);
  Serial.println(".. going to sleep now");
  delay(20);
  esp_deep_sleep_start();

}
///////////    FUNCTIONS   

void SD_init() {
  SPIClass spi = SPIClass(VSPI);
  spi.begin(SDCARD_CLK, SDCARD_MISO, SDCARD_MOSI, SDCARD_SS);

  if (!SD.begin(SDCARD_SS,spi,80000000)) {
    Serial.println("Card Mount Failed");
    sdOK = false;
  } 
  else {
  sdOK = true;
  uint8_t cardType = SD.cardType();
  if(cardType == CARD_NONE){
    Serial.println("No SD card attached");
    return;
  }

  Serial.print("SD Card Type: ");
  if(cardType == CARD_MMC){
    Serial.println("MMC");
  } else if(cardType == CARD_SD){
    Serial.println("SDSC");
  } else if(cardType == CARD_SDHC){
    Serial.println("SDHC");
  } else {
    Serial.println("UNKNOWN");
  }

  uint64_t cardSize = SD.cardSize() / (1024 * 1024);
  Serial.printf("SD Card Size: %lluMB\n", cardSize);

  // If the temp_data.txt file doesn't exist
  // Create a file on the SD card and write the data labels
  File file = SD.open("/temp_data.txt");
  if(!file) {
    Serial.println("File doens't exist");
    Serial.println("Creating file...");
    writeFile(SD, "/temp_data.txt", "Reading ID, Date/Hour, Temperature \r\n");
  }
  else {
    Serial.println("File already exists");  
  }
  file.close();
  }

}

//  reconnecting to MQTT server, timout happens from time to time
void reconnect() {
  int cnt=0;
  // Loop until we're reconnected to MQTT server
  while (!client.connected()) {
    Serial.print("Attempting MQTT connection...");
    // Attempt to connect
    client.setKeepAlive( 90 );  // timeout, this is important
    if (client.connect("ESP32_dnevna")) {
      Serial.println("connected");
      // Subscribe
      client.subscribe("dnevna_soba/temp");
    } else {
      if (cnt>30) {
        ESP.reset();
      }
      cnt++;
      Serial.print("failed, rc=");
      Serial.print(client.state());
      Serial.println(" try again in 5 seconds");
      // Wait 5 seconds before retrying
      delay(5000);
    }
  }
}

// in case we want to send something from rapi back on ESP
void callback(char* topic, byte* message, unsigned int length) {
  Serial.print("Message arrived on topic: ");
  Serial.print(topic);
  Serial.print(". Message: ");
  String messageTemp;
  
  for (int i = 0; i < length; i++) {
    Serial.print((char)message[i]);
    messageTemp += (char)message[i];
  }

  Serial.println();
  // If a message is received on the topic esp32/output, you check if the message is either "on" or "off". 
  // Changes the output state according to the message
  if (String(topic) == "dnevna_soba/output_temp") {
    Serial.print("Changing dnevna_soba/output_temp to ");
    if(messageTemp == "on"){
      Serial.println("Change ESP32 in dnevna_soba to on");
      //digitalWrite(ledPin, HIGH);
    }
    else if(messageTemp == "off"){
      Serial.println("Change ESP32 on dnevna_soba to off");
      //digitalWrite(ledPin, LOW);
    }
  }
}

// print local date and time
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


String ipToString(IPAddress ip) {
  String s = "";
  for (int i = 0; i < 4; i++)
    s += i  ? "." + String(ip[i]) : String(ip[i]);
  return s;
}

// Write the sensor readings on the SD card
void logSDCard() {
  dataMessage = String(readingID) + " , " + String(datestring) + " , " + 
                String(tempString) + "\r\n";
  Serial.print("Save data: ");
  Serial.println(dataMessage);
  appendFile(SD, "/temp_data.txt", dataMessage.c_str());
}

// Write to the SD card
void writeFile(fs::FS &fs, const char * path, const char * message) {
  Serial.printf("Writing file: %s\n", path);
  File file = fs.open(path, FILE_WRITE);
  if(!file) {
    Serial.println("Failed to open file for writing");
    return;
  }
  if(file.print(message)) {
    Serial.println("File written");
  } else {
    Serial.println("Write failed");
  }
  file.close();
}

// Append data to the SD card
void appendFile(fs::FS &fs, const char * path, const char * message) {
  Serial.printf("Appending to file: %s\n", path);
  File file = fs.open(path, FILE_APPEND);
  if(!file) {
    Serial.println("Failed to open file for appending");
    return;
  }
  if(file.print(message)) {
    Serial.println("Message appended");
  } else {
    Serial.println("Append failed");
  }
  file.close();
}


/********** MAIN LOOP */
void loop() {
// never get here as it is sleeping
}
