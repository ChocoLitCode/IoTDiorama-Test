#include "doorSystem.h"

// Pin Declarations
const int touch1 = 2;
const int touch2 = 4;
const int accessLED = 14;
const int intruderLED = 27;
const int buzzer = 16;
const int servo = 23;

// create servo object to control a servo
Servo myservo;

int failAttempts = 0;
unsigned long touchStart = 0;
const unsigned long TOUCH_HOLD_MS = 3000UL;
bool doorOpen = false;

// LED timeout variables
static unsigned long ledTimerStart = 0;
static bool ledTimerActive = false;
const unsigned long LED_TIMEOUT = 5000;  // 5 seconds

void setDoorPins() {
    pinMode(touch1, INPUT);
    pinMode(touch2, INPUT);
    pinMode(accessLED, OUTPUT);
    pinMode(intruderLED, OUTPUT);
    pinMode(buzzer, OUTPUT);

    digitalWrite(accessLED, LOW);
    digitalWrite(intruderLED, LOW);
    digitalWrite(buzzer, LOW);

    // Standard 50hz servo
  myservo.setPeriodHertz(50);
  myservo.attach(servo,500,2400); // attaches the servo on pin 23 to the servo object
    myservo.write(90); // Initial position
    Serial.println("Door System Ready");
}

// Variables to track touch sensor state for beep feedback
static bool touch1WasHigh = false;
static bool touch2WasHigh = false;

void unlockDoor(AsyncWebSocket& ws) {
    Serial.println("Access Granted");
    
    // Single beep (active buzzer)
    digitalWrite(buzzer, HIGH);
    delay(200);
    digitalWrite(buzzer, LOW);

    digitalWrite(accessLED, HIGH);
    digitalWrite(intruderLED, LOW);

    failAttempts = 0;
    touchStart = 0;
    doorOpen = true;
    myservo.write(0); // Rotate servo to open position
    
    // Start LED timer
    ledTimerStart = millis();
    ledTimerActive = true;

    ws.textAll("{\"alert\":\"Access Granted\"}");
    ws.textAll("{\"door\":\"UNLOCKED\"}");
}

void intruderAlert(AsyncWebSocket& ws) {
    
    // Three loud beeps (active buzzer)
    for (int i = 0; i < 3; i++) { 
        digitalWrite(buzzer, HIGH);
        delay(300);
        digitalWrite(buzzer, LOW);
        delay(300);
    }

    digitalWrite(accessLED, LOW);
    digitalWrite(intruderLED, HIGH);

    failAttempts = 0;
    touchStart = 0;
    
    // Start LED timer
    ledTimerStart = millis();
    ledTimerActive = true;

    ws.textAll("{\"alert\":\"Access Denied\"}");
}

void lockDoor(AsyncWebSocket& ws) {
    
    
    doorOpen = false;
    myservo.write(90); // Rotate servo to closed position
    
    ws.textAll("{\"door\":\"LOCKED\"}");
}

void startDoor(AsyncWebSocket& ws) {
    // Check if LED timer has expired
    if (ledTimerActive && (millis() - ledTimerStart >= LED_TIMEOUT)) {
        digitalWrite(accessLED, LOW);
        digitalWrite(intruderLED, LOW);
        ledTimerActive = false;
    }

    // Disable touch sensors while LEDs are active (during access granted/denied display)
    if (ledTimerActive) {
        return;  // Don't process touch sensors while showing result
    }

    int t1 = digitalRead(touch1);
    int t2 = digitalRead(touch2);
    bool bothTouched = (t1 == HIGH && t2 == HIGH);

    // Short beep when touch sensor is activated (like phone keypad)
    if (t1 == HIGH && !touch1WasHigh) {
        digitalWrite(buzzer, HIGH);
        delay(50);  // Very short beep
        digitalWrite(buzzer, LOW);
    }
    if (t2 == HIGH && !touch2WasHigh) {
        digitalWrite(buzzer, HIGH);
        delay(50);  // Very short beep
        digitalWrite(buzzer, LOW);
    }
    
    // Update previous states
    touch1WasHigh = (t1 == HIGH);
    touch2WasHigh = (t2 == HIGH);

    if (bothTouched) {
        if (touchStart == 0) touchStart = millis();
        if (millis() - touchStart >= TOUCH_HOLD_MS) {
            unlockDoor(ws);
        }
    } else {
        if (touchStart != 0) {
            failAttempts++;
            // Send failed attempt notification
            ws.textAll("{\"alert\":\"Failed Attempt\"}");
            
            if (failAttempts >= 3) intruderAlert(ws);
            touchStart = 0;
        }
    }
}
