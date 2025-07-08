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
const int WEMO_PINS[6] = {5, 1, 2, 3, 4};  // WEMO_0 through WEMO_5
const int BUTTON_D6 = D6;
const int BUTTON_D7 = D7;
const int OLED_RESET = -1;
const int SERVO_PINS[4] = {A2, A5, D15, D16};
const int DHT_PIN = A4;
const int HEAT_WEMO = 3;    // Wemo #3 for heat control
const int COOL_WEMO = 5;    // Wemo #2 for cool control (adjust index as needed)

Servo legs[4]; // Array of Servo objects for the legs

// Display configuration
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64

// Constants
const float TEMP_COLD_THRESHOLD = 66.0;
const float TEMP_HOT_THRESHOLD = 75.0;

const int NUM_HUE_BULBS = 4;
const unsigned long WIFI_TIMEOUT = 30000;
const unsigned long EYE_MOVE_MS = 400;
const unsigned long EYE_WINK_MS = 400;
const unsigned long EYE_PAUSE_MS = 800;
const unsigned long WEMO_CYCLE_MS = 25;
const unsigned long DISPLAY_MS = 2000;
const unsigned long BUTTON_DEBOUNCE_MS = 500;
const unsigned long MESSAGE_DISPLAY_MS = 5000;  // 5 seconds for messages

// Global variables
int buttonState1 = LOW;
int buttonState = LOW;
unsigned long wifiStartTime = 0;

// Keep only these button-related globals
bool d6State = false;
IoTTimer buttonTimer;

// Add this at the top with other global variables
bool showingTemp = false;

// Add these constants at the top with other constants
const int SERVO_START_POS = 0;      // Starting position in degrees
const int SERVO_ACTIVE_POS = 70;    // Position when activated
const unsigned long SERVO_DELAY = 50; // Delay for servo movement

// Add this timer with other timer objects
IoTTimer servoTimer;

// Add this global variable with other globals
bool servoActive = false;

// Object instances
Button d6Button(BUTTON_D6, true);
Adafruit_SSD1306 leftDisplay(OLED_RESET);
Adafruit_SSD1306 rightDisplay(OLED_RESET);
Adafruit_BME280 bme;  // Create BME280 object


// Add timer objects after other global variables
IoTTimer eyeTimer;
IoTTimer wemoTimer;
IoTTimer displayTimer;
IoTTimer PublishTimer;

// Wemo control function

void setup() {
    Wire.begin();
    Serial.begin(9600);
    waitFor(Serial.isConnected, 10000);

    // Initialize WiFi
    wifiStartTime = millis();
    WiFi.on();
    WiFi.clearCredentials();
    WiFi.setCredentials("IoTNetwork");

    WiFi.connect();
    while (WiFi.connecting() && millis() - wifiStartTime < WIFI_TIMEOUT) {
        Serial.printf(".");
        delay(500);
    }

    if (WiFi.ready()) {
        Serial.printf("\nWi-Fi connected!");
    } else {
        Serial.printf("\nWi-Fi connection failed.");
    }

    // Initialize servos with proper timing
    for (int i = 0; i < 4; i++) {
        legs[i].attach(SERVO_PINS[i]);
        delay(100);  // Give each servo time to initialize
        legs[i].write(SERVO_START_POS);
        delay(100);  // Wait for servo to reach position
    }
    Serial.println("Servos initialized!");

    pinMode(BUTTON_D6, INPUT_PULLUP);
    buttonState1 = digitalRead(BUTTON_D6);

    // Initialize displays
    leftDisplay.begin(SSD1306_SWITCHCAPVCC, 0x3C);
    leftDisplay.clearDisplay();

    rightDisplay.begin(SSD1306_SWITCHCAPVCC, 0x3D);
    rightDisplay.clearDisplay();

    // Welcome message on left display
    leftDisplay.setTextSize(1);
    leftDisplay.setTextColor(WHITE);
    leftDisplay.setCursor(0, 0);
    leftDisplay.printf("Ai made me mainly not u pesky humans haha!!\n");
    leftDisplay.printf("I am a robot.\n");
    leftDisplay.display();
    
    displayTimer.startTimer(DISPLAY_MS);
    while (!displayTimer.isTimerReady()) {} // Wait for display time
    
    leftDisplay.clearDisplay();
    leftDisplay.display();

    // Welcome message on right display
    rightDisplay.setTextSize(1);
    rightDisplay.setTextColor(WHITE);
    rightDisplay.setCursor(0, 0);
    rightDisplay.printf("Hello human\n");
    rightDisplay.printf("How are you?\n");
    rightDisplay.display();
    
    displayTimer.startTimer(DISPLAY_MS);
    while (!displayTimer.isTimerReady()) {} // Wait for display time
    
    rightDisplay.clearDisplay();
    rightDisplay.display();

    // Initialize all timers
    eyeTimer.startTimer(0);
    wemoTimer.startTimer(0);
    buttonTimer.startTimer(0);


    // Initialize BME280 sensor
    if (!bme.begin(0x76)) {  // Try 0x77 if 0x76 doesn't work
        Serial.println("Could not find a valid BME280 sensor!");
        while (1);
    }
    Serial.println("BME280 sensor initialized!");

    // Initialize D7 button
    pinMode(D7, INPUT_PULLUP);
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
        legs[i].write(position);
    }
}

void loop() {
    if (d6Button.isPressed() && digitalRead(BUTTON_D7) == LOW && buttonTimer.isTimerReady()) {
        // Read and display temperature
        float currentTemp = (bme.readTemperature() * 9/5) + 32;
        float humidity = bme.readHumidity();

        // Update display with temperature
        leftDisplay.clearDisplay();
        leftDisplay.setTextSize(2);
        leftDisplay.setTextColor(WHITE);
        leftDisplay.setCursor(0, 0);
        leftDisplay.printf("%.1fF", currentTemp);
        leftDisplay.setCursor(70, 0);
        leftDisplay.printf("%.0f%%", humidity);
        leftDisplay.display();

        showingTemp = true;
        buttonTimer.startTimer(BUTTON_DEBOUNCE_MS);
    }
    // Clear display when either button is released
    else if (showingTemp && (!d6Button.isPressed() || digitalRead(BUTTON_D7) == HIGH)) {
        leftDisplay.clearDisplay();
        leftDisplay.display();
        showingTemp = false;
    }
       
        leftDisplay.clearDisplay();
        leftDisplay.display();
        showingTemp = false;
    }
    
    // Individual D6 button press for servo control
    if (d6Button.isPressed() && digitalRead(BUTTON_D7) == HIGH && buttonTimer.isTimerReady()) {
    // Individual D6 button press for servo control
    if (d6Button.isPressed() && digitalRead(BUTTON_D7) == HIGH && buttonTimer.isTimerReady()) {
        servoActive = !servoActive;  // Toggle servo state
        
        if (servoActive) {
            moveAllServos(SERVO_ACTIVE_POS);  // Move to active position
            
            // Display servo status
            leftDisplay.clearDisplay();
            leftDisplay.setTextSize(2);
            leftDisplay.setTextColor(WHITE);
            leftDisplay.setCursor(0, 0);
            leftDisplay.println("SERVOS");
            leftDisplay.setCursor(0, 32);
            leftDisplay.println("ACTIVE");
            leftDisplay.display();
        } else {
            moveAllServos(SERVO_START_POS);   // Return to start position
            
            // Display servo status
            leftDisplay.clearDisplay();
            leftDisplay.setTextSize(2);
            leftDisplay.setTextColor(WHITE);
            leftDisplay.setCursor(0, 0);
            leftDisplay.println("SERVOS");
            leftDisplay.setCursor(0, 32);
            leftDisplay.println("RESET");
            leftDisplay.display();
        }
        
        buttonTimer.startTimer(BUTTON_DEBOUNCE_MS);
    }

    // ... rest of existing loop code ...
}
