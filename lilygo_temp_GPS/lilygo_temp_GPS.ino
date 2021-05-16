// standard includes
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

// ESP GPIO 33 -> GPS RX , ESP GPIO 32 -> GPS TX
static const int RXPin = 33, TXPin = 32;
static const uint32_t GPSBaud = 9600;

#define BUTTON_PIN 39

GxIO_Class io(SPI, /*CS=5*/ ELINK_SS, /*DC=*/ ELINK_DC, /*RST=*/ ELINK_RESET);
GxEPD_Class display(io, /*RST=*/ ELINK_RESET, /*BUSY=*/ ELINK_BUSY);

TinyGPSPlus gps;
HardwareSerial ss(1);

#include <Time.h>
const int time_offset = 8*3600;  // Local Time (AWST)
byte last_second, Second, Minute, Hour, Day, Month;
int Year;
double last_lng = NULL;
double last_lat = NULL;
int total_distance;
char msg[20];
bool sdOK;
String csvOutStr = "";
String tTemp, tTime, tDate, tDateTime;
String tmonth, tday, thour, tminute, tsecond;
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
    sdOK = true;
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
  display.updateWindow(0, 0,  249,  127, true);
  Serial.println(String(msg));

// init GPS 
  ss.begin(GPSBaud, SERIAL_8N1, RXPin, TXPin);
  total_distance = 0;
//  SerialGPSDecode(ss, gps);
  delay(500);

}

/********** MAIN LOOP */
void loop() {
  // read temp sensor
  float c = tempsensor.readTempC();
  tTemp = String(c,0);
  delay(50);
  
  // read GPS data
  SerialGPSDecode(ss, gps);
  delay(500);
  
  // log data to SD card    
  //if (sdOK) {
  //  logSDCard(); 
  // Increment readingID on every new reading
  //  readingID++;
  //}
}


//////////////////////////////////////////////////////////////////////

void SerialGPSDecode(Stream &mySerial, TinyGPSPlus &myGPS) {
    unsigned long start = millis();
    String tDist,tTDist;
    display.setRotation(1);
    display.fillScreen(GxEPD_WHITE);
    display.setTextColor(GxEPD_BLACK);
    do
    {
     while (ss.available() > 0)
     gps.encode(ss.read());
    } while (millis() - start < 500);
      if (gps.date.isValid()) 
        {
          Minute = gps.time.minute();
          Second = gps.time.second();
          Hour   = gps.time.hour();
          Day   = gps.date.day();
          Month = gps.date.month();
          Year  = gps.date.year();
          // set current UTC time
          setTime(Hour, Minute, Second, Day, Month, Year);
          // add the offset to get local time
          adjustTime(time_offset);
          if (month()<10) {
            tmonth = "0" + String(month());
          } else {
            tmonth = String(month());
          }
          if (day()<10) {
            tday = "0" + String(day());
          } else {
            tday = String(day());
          }
          if (hour()<10) {
            thour = "0" + String(hour());
          } else {
            thour = String(hour());
          }
          if (minute()<10) {
            tminute = "0" + String(minute());
          } else {
            tminute = String(minute());
          }
          if (second()<10) {
            tsecond = "0" + String(second());
          } else {
            tsecond = String(second());
          }
          tDate = String(year()) + "-" + tmonth + "-" + tday;
          tTime = thour + ":" + tminute + ":" + tsecond;
          tDateTime = tDate + " " + tTime;
          String tLocation = String(gps.location.lng(),6) + "," + String(gps.location.lat(),6);
          String tSpeed = String(gps.speed.kmph(),0);
          String tAge = "Age=" + String(gps.location.age());
          String tSat = "Sat=" + String(gps.satellites.value());
          if ((last_lat != NULL) && gps.location.isValid()){
            int distance = gps.distanceBetween(gps.location.lat(),gps.location.lng(),last_lat,last_lng);  // in meters
            Serial.println("distance =" + String(distance));
            tDist = "D=" + String(distance);
            if ((distance > 2) && (gps.location.age()<1000)) {
            total_distance += distance;
            } else {
            tDist = "D= ?";  
            }
          }
          if (gps.location.isValid()){
          last_lng = gps.location.lng();
          last_lat = gps.location.lat();
          last_second = gps.time.second();
          }
          tTDist= "TD=" + String(total_distance);
          Serial.println("total distance =" + String(total_distance));
          csvOutStr = tDateTime + "," + tLocation + "," + tTemp + "," + tSpeed + "\n";
          Serial.println(csvOutStr); 
          display.setFont(&FreeMonoBold9pt7b);
          display.setCursor(15, 13);
          display.print(tDateTime.c_str());
          display.setCursor(5, 60);
          display.setFont(&FreeMonoBold24pt7b);
          display.print(tSpeed);
          display.setFont(&FreeMonoBold9pt7b);
          display.setCursor(90, 60);
          display.print("km/h");
          display.setCursor(145, 60);
          display.setFont(&FreeMonoBold24pt7b);      
          display.print(tTemp.c_str());
          display.setFont(&FreeMonoBold12pt7b);
          display.setCursor(230, 60);
          display.print("C");
          display.setFont(&FreeMonoBold9pt7b);
          display.setCursor(5, 90);
  //          display.print(tLocation);
          display.print(tDist);
          display.setCursor(80, 90);
  //          display.print(tSatInfo);
          display.print(tTDist);
          display.setCursor(5, 110);
          display.print(tSat);
          display.setCursor(80, 110);
          display.print(tAge);
        } else {
          Serial.println(" Bad GPS Signal");
          display.setCursor(145, 60);
          display.setFont(&FreeMonoBold24pt7b);      
          display.print(tTemp.c_str());
          display.setFont(&FreeMonoBold12pt7b);
          display.setCursor(230, 60);
          display.print("C");
        }
     display.updateWindow(0, 0,  249,  127, true);
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
