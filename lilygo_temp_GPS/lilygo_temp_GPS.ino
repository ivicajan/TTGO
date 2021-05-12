
#include "SD.h"
#include "FS.h"
#include "SPI.h"
#include <HardwareSerial.h>
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

#define MCP_SDA 21
#define MCP_SCL 22

#define GPS_TX 33 // ESP GPIO 33 -> GPS RX
#define GPS_RX 32 // ESP GPIO 32 -> GPS TX
#define GPS_BAUD 9600
#define BUTTON_PIN 39

GxIO_Class io(SPI, /*CS=5*/ ELINK_SS, /*DC=*/ ELINK_DC, /*RST=*/ ELINK_RESET);
GxEPD_Class display(io, /*RST=*/ ELINK_RESET, /*BUSY=*/ ELINK_BUSY);

TinyGPSPlus gps;
SoftwareSerial ss(GPS_TX, GPS_RX);

float temperature ;
char  ctemp[4] ;
bool sdOK;
String dataMessage = "";                // Buffer for output file


/********** MAIN SETUP *************/

void setup() {
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
  display.println("SD Card...");
  
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
    display.println("SD Card Type: ");
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
    Serial.printf("SD Card Size: %lluMB\n", cardSize);
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
    }
    file.close();
  }

// init temp sensors 
  temperature = tempsensor.readTempC();
  sprintf(ctemp, "%3.1f",temperature); 
  Serial.println("temp = " + String(ctemp));

// init GPS 
  ss.begin(GPS_BAUD);
    if (gps.encode(ss))
      displayInfo();

  Serial.println();
  Serial.println(F("Done."));

  delay(2000);
  display.updateWindow(0, 0,  249,  127, true);
  delay(500);

}

/********** MAIN LOOP */
void loop() {
  while (ss.available() > 0)
    if (gps.encode(ss.read()))
      displayInfo();

  if (millis() > 5000 && gps.charsProcessed() < 10)
  {
    Serial.println(F("No GPS detected: check wiring."));
    while(true);
  }
}


// Write the sensor readings on the SD card
void logSDCard() {
//  dataMessage = String(readingID) + " , " + String(datestring) + " , " + 
//                String(msg) + "\r\n";
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


void displayInfo()
{
  Serial.print(F("Location: ")); 
  if (gps.location.isValid())
  {
    Serial.print(gps.location.lat(), 6);
    Serial.print(F(","));
    Serial.print(gps.location.lng(), 6);
  }
  else
  {
    Serial.print(F("INVALID"));
  }

  Serial.print(F("  Date/Time: "));
  if (gps.date.isValid())
  {
    Serial.print(gps.date.month());
    Serial.print(F("/"));
    Serial.print(gps.date.day());
    Serial.print(F("/"));
    Serial.print(gps.date.year());
  }
  else
  {
    Serial.print(F("INVALID"));
  }

  Serial.print(F(" "));
  if (gps.time.isValid())
  {
    if (gps.time.hour() < 10) Serial.print(F("0"));
    Serial.print(gps.time.hour());
    Serial.print(F(":"));
    if (gps.time.minute() < 10) Serial.print(F("0"));
    Serial.print(gps.time.minute());
    Serial.print(F(":"));
    if (gps.time.second() < 10) Serial.print(F("0"));
    Serial.print(gps.time.second());
    Serial.print(F("."));
    if (gps.time.centisecond() < 10) Serial.print(F("0"));
    Serial.print(gps.time.centisecond());
  }
  else
  {
    Serial.print(F("INVALID"));
  }
}
