/* 
 * Project myProject
 * Author: Maximo Regalado
 * Date: 
 * For comprehensive documentation and examples, please visit:
 * https://docs.particle.io/firmware/best-practices/firmware-template/
 */

// Include Particle Device OS APIs
#include "Particle.h"
#include "Adafruit_GFX.h"
#include "Adafruit_SSD1306.h"
#include "Wemo.h"
#include "IoTTimer.h"

SYSTEM_MODE(SEMI_AUTOMATIC);

// Pin definitions
const int BUTTON_PIN = D6;
const int BUTTON_PIN_D7 = D7;
const int OLED_RESET = -1;
const int SERVO1 = A2;
const int SERVO2 = A5;
const int SERVO3 = D15;
const int SERVO4 = D16;
const int SERVOPINS[4] = {A2, A5, D15, D16};

// Display dimensions
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64

// Global variables
int buttonState1 = LOW;
int buttonState2 = LOW;
unsigned long wifiStartTime = 0;
const unsigned long wifiTimeout = 30000; // 30 seconds
int currentHue = 0; // Track current hue for color cycling

// Object instances
Adafruit_SSD1306 leftDisplay(OLED_RESET);
Adafruit_SSD1306 rightDisplay(OLED_RESET);
Servo legs[4];
IoTTimer PublishTimer;
IoTTimer HueTimer;

// Color structure for RGB values
struct Color {
    int r, g, b;
};

// Function to convert HSV to RGB
Color HSVtoRGB(int h, int s, int v) {
    Color rgb;
    float c = v * s / 255.0;
    float x = c * (1 - abs((h / 60) % 2 - 1));
    float m = v - c;
    
    float r, g, b;
    
    if (h >= 0 && h < 60) {
        r = c; g = x; b = 0;
    } else if (h >= 60 && h < 120) {
        r = x; g = c; b = 0;
    } else if (h >= 120 && h < 180) {
        r = 0; g = c; b = x;
    } else if (h >= 180 && h < 240) {
        r = 0; g = x; b = c;
    } else if (h >= 240 && h < 300) {
        r = x; g = 0; b = c;
    } else {
        r = c; g = 0; b = x;
    }
    
    rgb.r = (int)((r + m) * 255);
    rgb.g = (int)((g + m) * 255);
    rgb.b = (int)((b + m) * 255);
    
    return rgb;
}

const unsigned char SNOWFLAKE[] = {
    0x00, 0x80, 0x00, 0x00,
    0x00, 0x80, 0x00, 0x00,
    0x00, 0x80, 0x00, 0x00,
    0x44, 0x92, 0x11, 0x00,
    0x02, 0x9C, 0x20, 0x00,
    0x01, 0x80, 0x40, 0x00,
    0x7F, 0xFF, 0xFE, 0x00,
    0x01, 0x80, 0x40, 0x00,
    0x02, 0x9C, 0x20, 0x00,
    0x44, 0x92, 0x11, 0x00,
    0x00, 0x80, 0x00, 0x00,
    0x00, 0x80, 0x00, 0x00,
    0x00, 0x80, 0x00, 0x00
};

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
    while (WiFi.connecting() && millis() - wifiStartTime < wifiTimeout) {
        Serial.printf(".");
        delay(500);
    }

    if (WiFi.ready()) {
        Serial.printf("\nWi-Fi connected!");
    } else {
        Serial.printf("\nWi-Fi connection failed.");
    }

    // Initialize pins
    pinMode(BUTTON_PIN, INPUT_PULLUP);
    pinMode(BUTTON_PIN_D7, INPUT_PULLUP);
    pinMode(BUTTON_ON, INPUT_PULLUP);
    
    // Attach all servos
    for (int i = 0; i < 4; i++) {
        legs[i].attach(SERVOPINS[i]);
        legs[i].write(0); // Start at 0 degrees
    }

    // Initialize displays
    leftDisplay.begin(SSD1306_SWITCHCAPVCC, 0x3C);
    leftDisplay.clearDisplay();

    rightDisplay.begin(SSD1306_SWITCHCAPVCC, 0x3D);
    rightDisplay.clearDisplay();

    // Welcome message on left display
    leftDisplay.setTextSize(1);
    leftDisplay.setTextColor(WHITE);
    leftDisplay.setCursor(0, 0);
    leftDisplay.printf("Hello, world!\n");
    leftDisplay.printf("Maximo Regalado\n");
    leftDisplay.printf("D6: Wemo Control\n");
    leftDisplay.printf("D7: Hue Control\n");
    leftDisplay.display();
    delay(2000);
    leftDisplay.clearDisplay();
    leftDisplay.display();

    // Welcome message on right display
    rightDisplay.setTextSize(1);
    rightDisplay.setTextColor(WHITE);
    rightDisplay.setCursor(0, 0);
    rightDisplay.printf("Robot Ready!\n");
    rightDisplay.printf("Eyes Active\n");
    rightDisplay.printf("Servos Ready\n");
    rightDisplay.printf("WiFi Connected\n");
    rightDisplay.display();
    delay(2000);
    rightDisplay.clearDisplay();
    rightDisplay.display();

    // Initialize timers
    PublishTimer.startTimer(0);
    HueTimer.startTimer(0);
}

void drawColoredEye(Adafruit_SSD1306 &display, int pupilX, int pupilY, bool wink, Color eyeColor) {
    display.clearDisplay();

    // Draw eye white (large circle)
    display.fillCircle(SCREEN_WIDTH/2, SCREEN_HEIGHT/2, 28, WHITE);

    // Draw colored iris (medium circle) - simulate color with patterns
    int brightness = (eyeColor.r + eyeColor.g + eyeColor.b) / 3;
    if (brightness > 128) {
        // Light color - draw with white outline
        display.drawCircle(SCREEN_WIDTH/2 + pupilX, SCREEN_HEIGHT/2 + pupilY, 15, WHITE);
        display.drawCircle(SCREEN_WIDTH/2 + pupilX, SCREEN_HEIGHT/2 + pupilY, 14, WHITE);
    } else {
        // Dark color - fill with black
        display.fillCircle(SCREEN_WIDTH/2 + pupilX, SCREEN_HEIGHT/2 + pupilY, 15, BLACK);
    }

    // Draw pupil (black circle)
    if (!wink) {
        display.fillCircle(SCREEN_WIDTH/2 + pupilX, SCREEN_HEIGHT/2 + pupilY, 8, BLACK);
    }

    // Draw eyelid for wink
    if (wink) {
        display.fillRect(SCREEN_WIDTH/2 - 28, SCREEN_HEIGHT/2 - 5, 56, 10, BLACK);
    }

    display.display();
}

void displayHueInfo(Color currentColor) {
    leftDisplay.clearDisplay();
    leftDisplay.setTextSize(1);
    leftDisplay.setTextColor(WHITE);
    leftDisplay.setCursor(0, 0);
    leftDisplay.printf("Current Hue: %d\n", currentHue);
    leftDisplay.printf("RGB: %d,%d,%d\n", currentColor.r, currentColor.g, currentColor.b);
    leftDisplay.printf("Press D7 for\n");
    leftDisplay.printf("next color\n");
    leftDisplay.display();
}

void strobeBlue(int bulb, int count, int delay_ms) {
    for(int i = 0; i < count; i++) {
        setHue(bulb, true, 0x0000FF, 255, 255);  // Bright blue
        delay(delay_ms);
        setHue(bulb, true, 0x0000FF, 32, 255);   // Dim blue
        delay(delay_ms);
    }
}

void displaySnowflake(Adafruit_SSD1306 &display) {
    display.clearDisplay();
    display.drawBitmap(
        (SCREEN_WIDTH - 32) / 2,
        (SCREEN_HEIGHT - 32) / 2,
        SNOWFLAKE,
        32, 32, WHITE
    );
    display.display();
}

void loop() {
    static int wemoIndex = 0;
    static bool cycling = false;
    static bool moved = false;
    static unsigned long lastEyeUpdate = 0;
    static int eyePosition = 0;
    static bool leftWink = false;
    static bool rightWink = false;
    static unsigned long lastWinkTime = 0;

    // Read button states
    int currentButtonState = digitalRead(BUTTON_PIN);
    int currentButtonStateD7 = digitalRead(BUTTON_PIN_D7);

    // Get current color based on hue
    Color currentColor = HSVtoRGB(currentHue, 255, 255);

    // D6 Button: Wemo Control
    if (currentButtonState == LOW && PublishTimer.isTimerReady() && !cycling) { 
        cycling = true;
        wemoIndex = 0;
        PublishTimer.startTimer(1); // Start immediately
        
        Serial.printf("Starting Wemo cycle...\n");
    }

    // Cycle through each Wemo, one at a time
    if (cycling && PublishTimer.isTimerReady()) {
        // Turn current Wemo ON
        wemoWrite(wemoIndex, true);
        Serial.printf("Wemo# %i is now ON\n", wemoIndex);
        delay(250); // Short ON time

        // Turn current Wemo OFF
        wemoWrite(wemoIndex, false);
        Serial.printf("Wemo# %i is now OFF\n", wemoIndex);

        wemoIndex++;
        if (wemoIndex >= 6) { // We have 6 Wemos (0-5)
            cycling = false; // Done cycling through all Wemos
            PublishTimer.startTimer(500); // Debounce before next cycle allowed
            Serial.printf("Wemo cycle complete!\n");
        } else {
            PublishTimer.startTimer(25); // Wait 25ms before next Wemo
        }
    }

    // D7 Button: Hue Control
    static bool hueButtonPressed = false;
    if (currentButtonStateD7 == LOW && !hueButtonPressed && HueTimer.isTimerReady()) {
        // Toggle heat state
        heatOn = !heatOn;
        
        // Control Wemo 3 (heat)
        wemoWrite(3, heatOn);
        
        // Display heat status
        leftDisplay.clearDisplay();
        leftDisplay.setTextSize(2);
        leftDisplay.setTextColor(WHITE);
        leftDisplay.setCursor(0, 0);
        leftDisplay.println(heatOn ? "HEAT ON" : "HEAT OFF");
        leftDisplay.display();
        
        hueButtonPressed = true;
        HueTimer.startTimer(200);
    }
    
    if (currentButtonStateD7 == HIGH) {
        hueButtonPressed = false;
    }

    // Servo control with D6 button
    if (currentButtonState == LOW && !moved) {
        // Turn on Wemo 5
        wemoWrite(5, HIGH);
        
        // Display snowflake on both displays
        displaySnowflake(leftDisplay);
        displaySnowflake(rightDisplay);
        
        // Strobe blue light
        strobeBlue(1, 3, 200);  // 3 strobes, 200ms each phase
        
        // Keep effect for remaining time
        delay(3000 - (3 * 400));
        
        // Clear displays
        leftDisplay.clearDisplay();
        rightDisplay.clearDisplay();
        leftDisplay.display();
        rightDisplay.display();
        
        moved = true;
    }
    if (currentButtonState == HIGH) {
        moved = false;
    }

    // Handle D6 button to turn ON the Hue bulb
    static bool lastD6State = HIGH;
    bool d6State = digitalRead(BUTTON_ON);
    if (d6State == LOW && lastD6State == HIGH) {
        bulbOn = true;
        Serial.println("D6 pressed: Hue bulb ON");
        setHue(BULB, true, HueRainbow[color], brightness, 255);
        lastColorSent = HueRainbow[color];
        lastBrightnessSent = brightness;
        lastUpdate = millis();
    }
    lastD6State = d6State;

    // Non-blocking eye animation
    if (millis() - lastEyeUpdate > 500) { // Update every 500ms
        // Eye movement positions (relative to center)
        int positions[][2] = {
            {0, 0},    // center
            {-8, -6},  // up-left
            {0, -10},  // up
            {8, -6},   // up-right
            {12, 0},   // right
            {8, 8},    // down-right
            {0, 10},   // down
            {-8, 8},   // down-left
            {-12, 0}   // left
        };
        int numPositions = sizeof(positions)/sizeof(positions[0]);

      /*  // Random winking
        if (millis() - lastWinkTime > 3000) { // Wink every 3 seconds
            if (random(100) < 30) { // 30% chance
                leftWink = (random(100) < 50);
                rightWink = !leftWink; // Alternate winking
                lastWinkTime = millis();
            }
        }
*/
        // Reset winks after 400ms
        if (millis() - lastWinkTime > 400) {
            leftWink = false;
            rightWink = false;
        }

        // Draw eyes with current color
        drawColoredEye(leftDisplay, positions[eyePosition][0], positions[eyePosition][1], leftWink, currentColor);
        drawColoredEye(rightDisplay, positions[eyePosition][0], positions[eyePosition][1], rightWink, currentColor);

        eyePosition = (eyePosition + 1) % numPositions;
        lastEyeUpdate = millis();
    }

    // Status output
    static unsigned long lastStatusUpdate = 0;
    if (millis() - lastStatusUpdate > 2000) { // Every 2 seconds
        Serial.printf("Status - D6: %s, D7: %s, Hue: %d, Cycling: %s\n", 
                     (currentButtonState == LOW ? "PRESSED" : "RELEASED"),
                     (currentButtonStateD7 == LOW ? "PRESSED" : "RELEASED"),
                     currentHue,
                     (cycling ? "YES" : "NO"));
        lastStatusUpdate = millis();
    }
}