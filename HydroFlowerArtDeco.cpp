/* 
 * Project My hydro Flower Art deco
 * Author: Maximo Regalado
 * Date: 2025-07-17
 * For comprehensive documentation and examples, please visit:
 * https://docs.particle.io/firmware/best-practices/firmware-template/
 */

// Include Particle Device OS APIs
#include "Particle.h"

// Let Device OS manage the connection to the Particle Cloud
SYSTEM_MODE(AUTOMATIC);

// Run the application and system concurrently in separate threads
SYSTEM_THREAD(ENABLED);

// Show system, cloud connectivity, and application logs over USB
// View logs with CLI using 'particle serial monitor --follow'
SerialLogHandler logHandler(LOG_LEVEL_INFO);

#include "Adafruit_BME280.h"
#include "Adafruit_SSD1306.h"
#include "Adafruit_GFX.h"
#include "neopixel.h"
#include "Air_Quality_Sensor.h"
#include <Adafruit_MQTT.h>
#include "Adafruit_MQTT/Adafruit_MQTT_SPARK.h"
#include "Adafruit_MQTT/Adafruit_MQTT.h"



// Color definitions for NeoPixels
#define red     0xFF0000
#define orange  0xFF8000
#define yellow  0xFFFF00
#define green   0x00FF00
#define blue    0x0000FF
#define black   0x000000
#define WHITE   1  // For OLED display (SSD1306 uses 1 for white)



//ADAFRUIT.IO
TCPClient TheClient; 
// Setup the MQTT client class by passing in the WiFi client and MQTT server and login details. 
Adafruit_MQTT_SPARK mqtt(&TheClient,AIO_SERVER,AIO_SERVERPORT,AIO_USERNAME,AIO_KEY); 
/****************************** Feeds ***************************************/ 
// Setup Feeds to publish or subscribe 
Adafruit_MQTT_Subscribe WaterButton = Adafruit_MQTT_Subscribe(&mqtt, AIO_USERNAME "/feeds/waterbutton"); 
Adafruit_MQTT_Publish TEMP = Adafruit_MQTT_Publish(&mqtt, AIO_USERNAME "/feeds/temperature");
Adafruit_MQTT_Publish HUMIDITY = Adafruit_MQTT_Publish(&mqtt, AIO_USERNAME "/feeds/humidity");
Adafruit_MQTT_Publish MOISTURE = Adafruit_MQTT_Publish(&mqtt, AIO_USERNAME "/feeds/moisture");
Adafruit_MQTT_Publish AIRQUALITY = Adafruit_MQTT_Publish(&mqtt, AIO_USERNAME "/feeds/airquality");
/************Declare Functions and Variables*************/
int buttonState;
unsigned long publishTime;
void MQTT_connect();
bool MQTT_ping();

//MOISTURE SENSOR
int soilMoist= A1;  // Changed to analog pin for moisture sensor
int moistureReads;


//TIME
String dateTime, timeOnly;
unsigned int lastTime;
unsigned int lastPublish;

//BME SENSOR
Adafruit_BME280 bme;
bool status;
const int hexAddress = 0x76;
unsigned int currentTime;
unsigned int lastSecond;
float tempC;
float tempF;
float humidRH;
const byte PERCENT = 37;
const byte DEGREE  = 167;

//AIR QUALITY SENSOR
AirQualitySensor sensor (A0);
int quality;

//OLED DISPLAY
#define SCREEN_WIDTH  128
#define SCREEN_HEIGHT 32
#define OLED_RESET -1
Adafruit_SSD1306 display(OLED_RESET);

//WATER PUMP
const int WATER_PUMP = D16;
unsigned int currentTimeWater;
unsigned int lastSecondWater;

//NEOPIXEL
const int PIXELCOUNT = 15;
int brightness = 50;
int i;
int j;
Adafruit_NeoPixel pixel(PIXELCOUNT, SPI1, WS2812B);

void setup() {
//SERIAL MONITOR
  Serial.begin(9600);
  waitFor(Serial.isConnected,10000);

//ADAFRUIT.IO
 Serial.printf("Connecting to Internet \n");
 WiFi.connect();
 while(WiFi.connecting()) {
   Serial.printf(".");
 }
 Serial.printf("\n Connected!!!!!! \n");
  // Setup MQTT subscription
   mqtt.subscribe(&WaterButton);
//DATE 
Time.zone(-7);
Particle.syncTime();

//MOISTURE
// No pinMode needed for analog pins

//BME 
status = bme.begin (hexAddress); 
  if (status== false){
    Serial.printf("BME280 at address 0x%02x failed to start", hexAddress);
  }

//AIR QUALITY
Serial.println("Waiting sensor to init...");
if (sensor.init()) {
  Serial.println("Sensor ready.");
}
else {
    Serial.println("Sensor ERROR!");
}
//OLED
display.begin(SSD1306_SWITCHCAPVCC, 0x3D);
display.clearDisplay();

//WATER PUMP
pinMode (WATER_PUMP, OUTPUT);

//NEOPIXELS
pixel.begin ();
pixel.setBrightness(brightness);
pixel.show();
pixel.clear();
pixel.show();
}

void loop() {
  MQTT_connect();
  MQTT_ping();
 
 //ADAFRUIT

// this is our 'wait for incoming subscription packets' busy subloop 
   Adafruit_MQTT_Subscribe *subscription;
   while ((subscription = mqtt.readSubscription(1000))) {
     if (subscription == &WaterButton) {
       buttonState = atof((char *)WaterButton.lastread);
       Serial.printf("Button State: %d\n", buttonState);
 
       if (buttonState==1){
       digitalWrite(WATER_PUMP, HIGH);
       }else {
       digitalWrite(WATER_PUMP,LOW);
       }
     }
    }

    if(millis()-lastPublish > 1200) { //publishing every half hour
      lastPublish=millis();
    if (mqtt.Update()) {
      Serial.println("Publishing sensor data...");
      TEMP.publish (tempF);
      HUMIDITY.publish (humidRH);
      MOISTURE.publish (moistureReads);
    }
  }

  //TIME
dateTime = Time.timeStr ();
timeOnly = dateTime. substring (11,19);

//MOISTURE - Read every loop
moistureReads = analogRead(soilMoist);

//BME - Read every loop
tempC = bme.readTemperature();
humidRH = bme.readHumidity();
tempF = (tempC*9/5)+32;

//AIR QUALITY - Read every loop
int quality = sensor.slope();

// Print sensor data every 30 minutes
if(millis()-lastTime>1200) {
  lastTime = millis();
  Serial.printf("Date and Time is %s\n",dateTime.c_str());
  Serial.printf("Time is %s\n",timeOnly.c_str());
  Serial.printf("Moisture is %i\n", moistureReads);
  Serial.printf("Temp: %.2f%c\n ", tempF,DEGREE); 
  Serial.printf("Humi: %.2f%c\n",humidRH,PERCENT);
  Serial.print("Sensor value: ");
  Serial.println(sensor.getValue());
}

// Air quality LED display (runs every loop)
if (quality == AirQualitySensor::FORCE_SIGNAL) {
  Serial.println("High pollution!");
  AIRQUALITY.publish("High pollution! ");
  for(i=0;i<60;i++){
    pixel.setPixelColor(i,red);
    pixel.show();
      delay(50);
  }
  pixel.show();
  pixel.clear(); 
  pixel.show(); 
}
else if (quality == AirQualitySensor::HIGH_POLLUTION) {
  Serial.println("High pollution!");
  AIRQUALITY.publish("High pollution!");
  for(i=0;i<60;i++){
    pixel.setPixelColor(i,orange);
    pixel.show();
    delay(50);
}
pixel.show();
pixel.clear();  
pixel.show();
}
else if (quality == AirQualitySensor::LOW_POLLUTION) {
  Serial.println("Low pollution!");
  AIRQUALITY.publish("Low pollution!");
  for(i=0;i<60;i++){
    pixel.setPixelColor(i,yellow);
    pixel.show();
    delay(50);
}
pixel.show();
pixel.clear();
pixel.show(); 
}
else if (quality == AirQualitySensor::FRESH_AIR) {
  Serial.println("Fresh air."); 
  AIRQUALITY.publish("Fresh air");
  for(i=0;i<60;i++){
    pixel.setPixelColor(i,green);
    pixel.show();
    delay(50);
}
pixel.show();
pixel.clear();
pixel.show();
} 

//WATER PUMP - Check moisture and control pump
if (moistureReads>3000){
    digitalWrite(WATER_PUMP,HIGH);
    pixel.setPixelColor(60,blue);
    pixel.show();
    Serial.printf("Pump ON\n");
    delay(500); 
    digitalWrite(WATER_PUMP,LOW);
    Serial.printf("Pump OFF:\n");
      pixel.setPixelColor(60,black);
      pixel.show();
}

//OLED
display.clearDisplay();
display.setTextSize(1);
display.setTextColor(WHITE);
display.setCursor(0,0);
display.printf("Time: %s\n",timeOnly.c_str());
display.setCursor(0,8);
display.printf("Moisture: %i\n", moistureReads);
display.setCursor(0,16);
display.printf("Temp: %.1f%c Hum: %.1f%c\n",tempF,DEGREE,humidRH,PERCENT);
display.setCursor(0,24);
display.printf("My Hydro Flower");
display.display();

} // End of loop() function

void MQTT_connect() {
  int8_t ret;
 
  // Return if already connected.
  if (mqtt.connected()) {
    return;
  }
 
  Serial.print("Connecting to MQTT... ");
 
  while ((ret = mqtt.connect()) != 0) { // connect will return 0 for connected
       Serial.printf("Error Code %s\n",mqtt.connectErrorString(ret));
       Serial.printf("Retrying MQTT connection in 5 seconds...\n");
       mqtt.disconnect();
       delay(5000);  // wait 5 seconds and try again
  }
  Serial.printf("MQTT Connected!\n");
}

bool MQTT_ping() {
  static unsigned int last;
  bool pingStatus = true;  // Initialize to true

  if ((millis()-last)>60000) {
      Serial.printf("Pinging MQTT \n");
      pingStatus = mqtt.ping();
      if(!pingStatus) {
        Serial.printf("Disconnecting \n");
        mqtt.disconnect();
      }
      last = millis();
  }
  return pingStatus;
}
