
/**************************************************************************/
/* Development of ESP32 DEV board with MCP9808 temp sensor on specific I2C
 *  SDA and SCL pinout with MQTT client calling to local rapi 
*/
/**************************************************************************/

#include <Wire.h>
#include "Adafruit_MCP9808.h"
#include <WiFi.h>
#include <PubSubClient.h>

// Specific I2C pins for MCP9808 
#define I2C_SDA 16
#define I2C_SCL 17
// Create the MCP9808 temperature sensor object
Adafruit_MCP9808 tempsensor = Adafruit_MCP9808();

#define uS_TO_S_FACTOR 1000000  /* Conversion factor for micro seconds to seconds */
#define TIME_TO_SLEEP  600       /* Time ESP32 will go to sleep (in seconds) */

// define wifi 
const char* ssid = "change to yours";
const char* password = "change to yours";
const char* mqtt_server = "192.168.1.101";  // depending on raspberry pi IP address

WiFiClient espClient;
PubSubClient client(espClient);

float temperature = 0;

// need to put everything inside setup as we use deep sleep 

void setup() {
  // start I2C for MCP9808
  Wire.begin(I2C_SDA, I2C_SCL);
  // start serial for debugging prints
  Serial.begin(115200);
  while (!Serial); //waits for serial terminal to be open, necessary in newer arduino boards.

  Serial.println("Starting ... MCP9808"); 
  if (!tempsensor.begin(0x18,  &Wire)) {
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
  Serial.println("wake up MCP9808.... "); // wake up MCP9808 - power consumption ~200 mikro Ampere
  tempsensor.wake();   // wake up, ready to read!
  // Read and print out the temperature
  temperature = tempsensor.readTempC();
  char tempString[8];
  dtostrf(temperature, 1, 2, tempString);
  Serial.print("Temp: "); 
  Serial.println(tempString);   
  Serial.println("Shutdown MCP9808.... ");
  tempsensor.shutdown_wake(1); // shutdown MSP9808 - power consumption ~0.1 mikro Ampere, stops temperature sampling

  // start WiFi
  delay(10);
  // We start by connecting to a WiFi network
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
    }
  }

  if (WiFi.status() != WL_CONNECTED) {
  Serial.println("***************************");
  Serial.println("WiFi connected");
  Serial.println("IP address: ");
  Serial.println(WiFi.localIP());
  Serial.println("***************************");
}

  // Setup MQTT 
  client.setServer(mqtt_server, 1883);
  if (!client.connected()) {
    reconnect();
  }
  // need that, start MQTT client loop
  client.loop();
  client.publish("deck/temp", tempString);  // publish data        
  delay(100);
  client.unsubscribe("esp32/output");
  client.disconnect();
  // Setup for sleeping
  esp_sleep_enable_timer_wakeup(TIME_TO_SLEEP * uS_TO_S_FACTOR); //go to sleep
  Serial.println("Setup ESP32 to sleep for every " + String(TIME_TO_SLEEP) + " Seconds");
  Serial.println("Going to sleep as normal now.");
  esp_deep_sleep_start();

}

// in case you want to send something back to ESP32
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
  // Feel free to add more if statements to control more GPIOs with MQTT
  // If a message is received on the topic esp32/output, you check if the message is either "on" or "off". 
  // Changes the output state according to the message
  if (String(topic) == "deck/output_temp") {
    Serial.print("Changing deck/output_temp to ");
    if(messageTemp == "on"){
      Serial.println("Change ESP32 on deck to on");
      //digitalWrite(ledPin, HIGH);
    }
    else if(messageTemp == "off"){
      Serial.println("Change ESP32 on deck to off");
      //digitalWrite(ledPin, LOW);
    }
  }
}

// connectiong to MQTT and sending the data
void reconnect() {
  // Loop until we're reconnected
  while (!client.connected()) {
    Serial.print("Attempting MQTT connection...");
    // Attempt to connect
    client.setKeepAlive( 90 );  // timeout, this is important
    if (client.connect("ESP32_deck")) {
      Serial.println("connected");
      // Subscribe
      client.subscribe("deck/temp");
    } else {
      Serial.print("failed, rc=");
      Serial.print(client.state());
      Serial.println(" try again in 5 seconds");
      // Wait 5 seconds before retrying
      delay(5000);
    }
  }
}

// never get to here as sleeping mode
void loop() {

}
