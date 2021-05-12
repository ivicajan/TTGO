// standard includes
#include "SD.h"
#include "FS.h"
#include "SPI.h"
#include <SoftwareSerial.h>
#include <TinyGPS++.h>

// Temperature Sensor Header
#include <Wire.h>
#include "Adafruit_MCP9808.h"
Adafruit_MCP9808 tempsensor = Adafruit_MCP9808();  // Create the MCP9808 temperature sensor object

// include library, include base class, make path known
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

// ESP GPIO 33 -> GPS RX , ESP GPIO 32 -> GPS TX
static const int RXPin = 33, TXPin = 32;
static const uint32_t GPSBaud = 9600;

#define BUTTON_PIN 39

GxIO_Class io(SPI, /*CS=5*/ ELINK_SS, /*DC=*/ ELINK_DC, /*RST=*/ ELINK_RESET);
GxEPD_Class display(io, /*RST=*/ ELINK_RESET, /*BUSY=*/ ELINK_BUSY);

TinyGPSPlus gps;
SoftwareSerial ss(RXPin, TXPin);

char msg[20];
bool sdOK;
String csvOutStr = "";
String hour,minute,second,year,month,day,tTime,tDate;
String dataMessage;
int readingID = 0;

/********** MAIN SETUP *************/

void setup() {
  Serial.begin(115200);
  Serial.println("Setup...");

//-------------- ePaper
  SPI.begin(SPI_CLK, SPI_MISO, SPI_MOSI, ELINK_SS);
  display.init(); // enable diagnostic output on Serial
  display.setRotation(1);
  display.fillScreen(GxEPD_WHITE);
  display.setTextColor(GxEPD_BLACK);  
  display.setFont(&FreeMonoBold12pt7b);
  display.setCursor(10, 15);
  display.println("Setup...");
  display.setFont(&FreeMonoBold9pt7b);

// Initialize SD card
  Serial.println("SD Card ...");  
  SPIClass spi = SPIClass(VSPI);
  spi.begin(SDCARD_CLK, SDCARD_MISO, SDCARD_MOSI, SDCARD_SS);
  if (!SD.begin(SDCARD_SS,spi,80000000)) {
    Serial.println("Card Mount Failed");
    display.println("Card Mount Failed");
    sdOK = false;
    } 
  else {
    sdOK = true;
    uint8_t cardType = SD.cardType();
    if (cardType == CARD_NONE) {
      Serial.println("No SD card attached");
      display.println("No SD card attached");
      return;
    }
    Serial.print("SD Card Type: ");
    display.print("SD Card Type: ");
    if (cardType == CARD_MMC) {
      Serial.println("MMC");
      display.println("MMC");
      } 
    else if (cardType == CARD_SD) {
      Serial.println("SD");
      display.println("SD");
    } 
    else if (cardType == CARD_SDHC) {
      Serial.println("SDHC");
      display.println("SDHC");
    } 
    else {
      Serial.println("UNKNOWN");
      display.println("UNKNOWN");
    }

    uint64_t cardSize = SD.cardSize() / (1024 * 1024);
    Serial.printf("SD Card Size: %llu MB\n", cardSize);
    char sdSize[20];
    sprintf(sdSize,"%lluMB", cardSize);
    display.println("SD Card Size: " + String(sdSize));
    // If the temp_GPS_data.txt file doesn't exist
    // Create a file on the SD card and write the data labels
    File file = SD.open("/temp_GPS_data.txt");
    if (!file) {
      Serial.println("File /temp_GPS_data.txt doens't exist, creating new");
      display.println("creating tmp_GPS_data.txt");
      writeFile(SD, "/temp_GPS_data.txt", "Reading ID, Date/Hour, GPS lon, GPS lat, GPS speed, Temperature \r\n");
    }
    else {
      Serial.println("File /temp_GPS_data.txt already exists");  
      display.println("temp_GPS_data.txt OK");  
    }
    file.close();
  }

// init temp sensors 
  // Get and Publish Temperature
  Serial.println(".. sense temperature");
  int cnt = 0;
  while (!tempsensor.begin(0x18)) {
    Serial.println("Couldn't find MCP9808!");
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
  snprintf (msg, 5, "%lf", c);
  display.print("Temp = " + String(msg));
  Serial.println(String(msg));
  display.updateWindow(0, 0,  249,  127, true);

// init GPS 
  ss.begin(GPSBaud);
  if (ss.available() > 0)
  {
  gps.encode(ss.read());
  hour = String(gps.time.hour());
  minute = String(gps.time.minute());
  second = String(gps.time.second());
  year = String(gps.date.year());
  month = String(gps.date.month());
  day = String(gps.date.day());      
  if (hour.length() == 1)
  {
     hour = "0" + hour;     
  }
  if (minute.length() == 1)
  {
     minute = "0" + minute;     
  }
  if (second.length() == 1)
  {
     second = "0" + second;     
  } 
  if (month.length() == 1)
  {
     month = "0" + month;     
  }  
  if (day.length() == 1)
  {
     day = "0" + day;     
  }
  tDate = year + "-" + month + "-" + day;
  tTime = hour + ":" + minute + ":" + second;
  String tLocation = String(gps.location.lng(), 6) + "," + String(gps.location.lat(), 6) + "," + String(gps.location.age());
  String tDateTime = tDate + " " + tTime;
  String tSpeed = String(gps.speed.kmph(),1);
  float c = tempsensor.readTempC();
  String tTemp = String(c,1);
//  snprintf (msg, 20, "%lf", c);
  csvOutStr = tDateTime + "," + tLocation + "," + tSpeed + "," + tTemp + "\n";
  Serial.println(csvOutStr);
  display.setRotation(1);
  display.fillScreen(GxEPD_WHITE);
  display.setTextColor(GxEPD_BLACK);  
  display.setFont(&FreeMonoBold12pt7b);
  display.setCursor(5, 5);
  display.print(tDateTime.c_str());
  display.setCursor(40, 65);
  display.setFont(&FreeMonoBold24pt7b);
  display.print(tSpeed);
  display.setFont(&FreeMonoBold12pt7b);
  display.setCursor(160, 65);
  display.print("km/h");
  display.setFont(&FreeMonoBold9pt7b);
  display.setCursor(60, 90);
  display.print(tLocation);
  display.setCursor(20, 110);
  display.print(tTemp.c_str());
  display.updateWindow(0, 0,  249,  127, true);        
  } else {
    Serial.println("GPS not available");
  }

  delay(2000);
  display.updateWindow(0, 0,  249,  127, true);
  delay(500);

}

/********** MAIN LOOP */
void loop() {
 if (ss.available() > 0)
  {
    gps.encode(ss.read());
    hour = String(gps.time.hour());
    minute = String(gps.time.minute());
    second = String(gps.time.second());
    year = String(gps.date.year());
    month = String(gps.date.month());
    day = String(gps.date.day());      
    if (hour.length() == 1)
    {
       hour = "0" + hour;     
    }
    if (minute.length() == 1)
    {
       minute = "0" + minute;     
    }
    if (second.length() == 1)
    {
       second = "0" + second;     
    } 
    if (month.length() == 1)
    {
       month = "0" + month;     
    }  
    if (day.length() == 1)
    {
       day = "0" + day;     
    }
    tDate = year + "-" + month + "-" + day;
    tTime = hour + ":" + minute + ":" + second;
    String tLocation = String(gps.location.lng(), 7) + "," + String(gps.location.lat(), 7) + "," + String(gps.satellites.value());
    String tDateTime = tDate + " " + tTime;
    String tSpeed = String(gps.speed.kmph(),0);
    float c = tempsensor.readTempC();
    String tTemp = String(c,0);
    csvOutStr = tDateTime + "," + tLocation + "," + tSpeed + "," + tTemp + "\n";
    Serial.println(csvOutStr);
    display.setRotation(1);
    display.fillScreen(GxEPD_WHITE);
    display.setTextColor(GxEPD_BLACK);  
    display.setFont(&FreeMonoBold9pt7b);
    display.setCursor(15, 13);
    display.print(tDateTime.c_str());
    display.setCursor(5, 60);
    display.setFont(&FreeMonoBold24pt7b);
    display.print(tSpeed);
//    display.print(String(120));
    display.setFont(&FreeMonoBold9pt7b);
    display.setCursor(90, 60);
    display.print("km/h");
    display.setCursor(145, 60);
    display.setFont(&FreeMonoBold24pt7b);      
    display.print(tTemp.c_str());
//    display.print(String(120));
    display.setFont(&FreeMonoBold12pt7b);
    display.setCursor(230, 60);
    display.print("C");
    display.setFont(&FreeMonoBold9pt7b);
    display.setCursor(5, 100);
    display.print(tLocation);
    display.setCursor(20, 110);
    display.setFont(&FreeMonoBold12pt7b);
//    client.publish(TOPIC.c_str(), msg);  
    display.updateWindow(0, 0,  249,  127, true);    
    if (sdOK) {
//    logSDCard(); 
    // Increment readingID on every new reading
    readingID++;
    }    
  } else {
    Serial.println("GPS not available");
  }
  delay(2000);
}


// Write the sensor readings on the SD card
void logSDCard() {
  dataMessage = String(readingID) + " , " + String(csvOutStr) + "\r\n";
  Serial.print("Save data: ");
  Serial.println(dataMessage);
  appendFile(SD, "/temp_GPS_data.txt", dataMessage.c_str());
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
