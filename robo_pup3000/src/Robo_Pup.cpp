/* 
 * Project myProject
 * Author: Your Name
 * Date: 
 * For comprehensive documentation and examples, please visit:
 * https://docs.particle.io/firmware/best-practices/firmware-template/
 */

// Include Particle Device OS APIs
#include "Particle.h"
#include "Adafruit_GFX.h"
#include "Adafruit_SSD1306.h"
#include "Button.h"
#include "application.h"
#include "IoTTimer.h"
#include "hue.h"
#include "Adafruit_BME280.h"
#include "Wire.h"
#include "Wemo.h"  // Add this line


// System configuration
SYSTEM_MODE(SEMI_AUTOMATIC);

// Pin definitions
const int BUTTON_D6 = D6;
const int BUTTON_D7 = D7;
const int OLED_RESET = -1;
const int SERVO_PINS[4] = {A2, A5, D15, D16};

// Display configuration
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64

// Constants and pin definitions
const int WEMO_COOL_1 = 3;  // First cooling Wemo
const int WEMO_COOL_2 = 1;  // Second cooling Wemo
const int WEMO_HEAT_1 = 5;  // First heating Wemo
const int WEMO_HEAT_2 = 4;  // Second heating Wemo
const float TEMP_COLD_THRESHOLD = 66.0;
const float TEMP_HOT_THRESHOLD = 75.0;
const int NUM_HUE_BULBS = 4;
const int HUE_BRIGHTNESS = 255;
const int HUE_SATURATION = 255;
const int SERVO_START_POS = 0;
const int SERVO_ACTIVE_POS = 70;
const unsigned long WIFI_TIMEOUT = 30000;
const unsigned long EYE_MOVE_MS = 800;    // Doubled from 400 to slow movement
const unsigned long EYE_WINK_MS = 400;    // Keep wink duration the same
const unsigned long EYE_PAUSE_MS = 1200;  // Increased pause time for center position
const unsigned long WEMO_CYCLE_MS = 25;
const unsigned long DISPLAY_MS = 2000;
const unsigned long BUTTON_DEBOUNCE_MS = 500;
const unsigned long MESSAGE_DISPLAY_MS = 5000;  // 5 seconds for messages

// Global state variables
bool coolOn = false;
bool heatOn = false;
bool showingTemp = false;
bool servoActive = false;
bool isWinking = false;
float currentTemp = 0.0;  // Variable to store current temperature
int eyeIndex = 0;  // Current position index for eye movement

// Add this timer with other timer objects
IoTTimer servoTimer;

// Object instances
Button d6Button(BUTTON_D6);
Button d7Button(BUTTON_D7);
Servo legs[4];  // Add Servo array declaration
Adafruit_SSD1306 leftDisplay(OLED_RESET);
Adafruit_SSD1306 rightDisplay(OLED_RESET);
Adafruit_BME280 bme;  // Create BME280 object

// Global variables
int buttonState1 = LOW;
int buttonState = LOW;
unsigned long wifiStartTime = 0;

// Function prototypes
void displayStatus(const char* mode, bool isOn);
void displayTemperature();
void moveAllServos(int position);
void drawEye(Adafruit_SSD1306 &display, int pupilX, int pupilY, bool wink);
void printBMEReadings();

// Add timer objects after other global variables
IoTTimer eyeTimer;
IoTTimer wemoTimer;
IoTTimer displayTimer;
IoTTimer PublishTimer;
IoTTimer buttonTimer;
IoTTimer tempCheckTimer;

// Wemo control function

void setup() {
    Wire.begin();
    Serial.begin(9600);
    waitFor(Serial.isConnected, 10000);

    // Initialize WiFi
    WiFi.on();
    WiFi.setCredentials("IoTNetwork");
    WiFi.connect();
    
    // Initialize servos with verification
    for (int i = 0; i < 4; i++) {
        if (legs[i].attach(SERVO_PINS[i])) {
            Serial.printf("Servo %d attached to pin %d\n", i, SERVO_PINS[i]);
            legs[i].write(SERVO_START_POS);
            delay(100);  // Wait for servo to reach position
        } else {
            Serial.printf("Failed to attach servo %d\n", i);
            // Retry attachment
            legs[i].attach(SERVO_PINS[i]);
        }
    }

    // Initialize displays without welcome messages
    leftDisplay.begin(SSD1306_SWITCHCAPVCC, 0x3C);
    leftDisplay.clearDisplay();
    leftDisplay.display();

    rightDisplay.begin(SSD1306_SWITCHCAPVCC, 0x3D);
    rightDisplay.clearDisplay();
    rightDisplay.display();

    // Initialize buttons
    pinMode(BUTTON_D6, INPUT_PULLUP);
    pinMode(BUTTON_D7, INPUT_PULLUP);

    // Initialize BME280
    bme.begin(0x76);

    // Start timers immediately
    eyeTimer.startTimer(0);
    wemoTimer.startTimer(0);
    buttonTimer.startTimer(0);
    tempCheckTimer.startTimer(0);
}

void drawEye(Adafruit_SSD1306 &display, int pupilX, int pupilY, bool wink) {
    display.clearDisplay();

    // Draw eye white (large circle)
    display.fillCircle(SCREEN_WIDTH/2, SCREEN_HEIGHT/2, 28, WHITE);

    // Draw pupil (black circle)
    if (!wink) {
        display.fillCircle(SCREEN_WIDTH/2 + pupilX, SCREEN_HEIGHT/2 + pupilY, 10, BLACK);
    }

    // Draw eyelid for wink
    if (wink) {
        display.fillRect(SCREEN_WIDTH/2 - 28, SCREEN_HEIGHT/2 - 5, 56, 10, BLACK);
    }

    display.display();
}

// Move all servos to a given position
void moveAllServos(int position) {
    for (int i = 0; i < 4; i++) {
        if (legs[i].attached()) {
            Serial.printf("Moving servo %d to position %d\n", i, position);
            legs[i].write(position);
            delay(100);  // Allow time for movement
        } else {
            Serial.printf("Servo %d not attached, attempting to reattach\n", i);
            legs[i].attach(SERVO_PINS[i]);
        }
    }
}
void loop() {
    // Eye movement pattern
    const int eyePositions[][2] = {
        {0, 0},    // center
        {-8, -8},  // up-left
        {0, -10},  // up
        {8, -8},   // up-right
        {10, 0},   // right
        {8, 8},    // down-right
        {0, 10},   // down
        {-8, 8},   // down-left
        {-10, 0}   // left
    };
    
    // Random winking
    if (eyeTimer.isTimerReady()) {
        // Check if eyes are at center position
        if (eyeIndex == 0) {
            // 50% chance to wink one eye when centered
            if (random(100) < 50) {
                drawEye(leftDisplay, 0, 0, false);
                drawEye(rightDisplay, 0, 0, true);
                eyeTimer.startTimer(EYE_WINK_MS);
            } else {
                drawEye(leftDisplay, eyePositions[eyeIndex][0], eyePositions[eyeIndex][1], false);
                drawEye(rightDisplay, eyePositions[eyeIndex][0], eyePositions[eyeIndex][1], false);
                eyeIndex = (eyeIndex + 1) % 9;
                eyeTimer.startTimer(EYE_PAUSE_MS);
            }
        } else {
            drawEye(leftDisplay, eyePositions[eyeIndex][0], eyePositions[eyeIndex][1], false);
            drawEye(rightDisplay, eyePositions[eyeIndex][0], eyePositions[eyeIndex][1], false);
            eyeIndex = (eyeIndex + 1) % 9;
            eyeTimer.startTimer(EYE_MOVE_MS);
        }
    }
    
    // D6 Button - Cool Control
    if (d6Button.isClicked()) {
        coolOn = !coolOn;
        TCPClient client;
        if (coolOn) {
            // Turn on both cooling Wemos
            switchON(WEMO_COOL_1);
            delay(50);  // Small delay between commands
            switchON(WEMO_COOL_2);
        } else {
            // Turn off both cooling Wemos
            switchOFF(WEMO_COOL_1);
            delay(50);
            switchOFF(WEMO_COOL_2);
        }
        
        // Control all Hue bulbs
        for (int i = 1; i <= NUM_HUE_BULBS; i++) {
            setHue(i, coolOn, HueBlue, HUE_BRIGHTNESS, HUE_SATURATION);
            delay(50);
        }
        displayStatus("COOL", coolOn);
    }
    
    // D7 Button - Heat Control
    if (d7Button.isClicked()) {
        heatOn = !heatOn;
        TCPClient client;
        if (heatOn) {
            // Turn on both heating Wemos
            switchON(WEMO_HEAT_1);
            delay(50);
            switchON(WEMO_HEAT_2);
        } else {
            // Turn off both heating Wemos
            switchOFF(WEMO_HEAT_1);
            delay(50);
            switchOFF(WEMO_HEAT_2);
        }
        
        // Control all Hue bulbs
        for (int i = 1; i <= NUM_HUE_BULBS; i++) {
            setHue(i, heatOn, HueRed, HUE_BRIGHTNESS, HUE_SATURATION);
            delay(50);
        }
        displayStatus("HEAT", heatOn);
    }
    
    // Temperature check and auto control
    if (tempCheckTimer.isTimerReady()) {
        float currentTemp = (bme.readTemperature() * 9/5) + 32;
        
        // Print current temperature for debugging
        Serial.printf("Current temp: %.1f°F (Cold threshold: %.1f°F)\n", 
                     currentTemp, TEMP_COLD_THRESHOLD);
        
        // Auto heating with proper error checking
        if (currentTemp < TEMP_COLD_THRESHOLD && !heatOn) {
            heatOn = true;
            Serial.println("Auto Heat activated!");
            
            // Create single TCP client for both Wemos
            TCPClient client;
            
            // Turn on first heating Wemo
            switchON(WEMO_HEAT_1);
            Serial.printf("WEMO_HEAT_1 (Pin %d) turned ON\n", WEMO_HEAT_1);
            delay(100);  // Reduced delay between commands
            
            // Turn on second heating Wemo
            switchON(WEMO_HEAT_2);
            Serial.printf("WEMO_HEAT_2 (Pin %d) turned ON\n", WEMO_HEAT_2);
            
            // Control Hue bulbs
            for (int i = 1; i <= NUM_HUE_BULBS; i++) {
                setHue(i, true, HueRed, HUE_BRIGHTNESS, HUE_SATURATION);
                delay(50);
            }
            
            // Update display
            displayStatus("AUTO HEAT", true);
        }
        
        tempCheckTimer.startTimer(3000);
    }
}

// Helper Functions
void displayStatus(const char* mode, bool isOn) {
    leftDisplay.clearDisplay();
    leftDisplay.setTextSize(2);
    leftDisplay.setTextColor(WHITE);
    leftDisplay.setCursor(0, 0);
    leftDisplay.println(mode);
    leftDisplay.setCursor(0, 32);
    leftDisplay.println(isOn ? "ON" : "OFF");
   
}
// Function to display temperature and humidity on the left display
void displayTemperature() {
    float currentTemp = (bme.readTemperature() * 9/5) + 32;
    float humidity = bme.readHumidity();
    leftDisplay.clearDisplay();
    leftDisplay.setTextSize(2);
    leftDisplay.setTextColor(WHITE);
    leftDisplay.setCursor(0, 0);
    leftDisplay.printf("%.1fF", currentTemp);
    leftDisplay.setCursor(70, 0);
    leftDisplay.printf("%.0f%%", humidity);
    leftDisplay.display();
}

// Add this function with your other helper functions
void printBMEReadings() {
    float temp = (bme.readTemperature() * 9/5) + 32;
    float humidity = bme.readHumidity();
    float pressure = bme.readPressure() / 100.0F;
    
    Serial.printf("Temperature: %.1f°F\n", temp);
    Serial.printf("Humidity: %.0f%%\n", humidity);
    Serial.printf("Pressure: %.1f hPa\n", pressure);
    Serial.println("------------------------");
}