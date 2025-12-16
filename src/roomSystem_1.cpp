#include "roomSystem_1.h"
#include "lcd.h"

// Pin Declarations
const int ldr = 34;
const int ldrLED1 = 25;
const int ldrLED2 = 33;
const int ldrLED3 = 32;

const int ldrThreshold = 4000;

// Manual control
bool room1_override = false;       // true if web overrides
bool room1_manualTarget = false;   // manual target ON/OFF
bool room1_state = false;// Actual LED state

// Sequential LED state
static int ledStage = 0;
static unsigned long lastStepTime = 0;
const unsigned long stepDelay = 300;
static bool turningOn = false;

// Extern for greeting state
extern bool greetingActive;

void setRoomOne() {
    pinMode(ldr, INPUT);
    pinMode(ldrLED1, OUTPUT);
    pinMode(ldrLED2, OUTPUT);
    pinMode(ldrLED3, OUTPUT);

    digitalWrite(ldrLED1, LOW);
    digitalWrite(ldrLED2, LOW);
    digitalWrite(ldrLED3, LOW);
}

void startRoomOne(void (*notify)(float,float)) {
    static bool lastState = false;
    static unsigned long lastNotifyTime = 0;
    unsigned long now = millis();

    int LDRvalue = analogRead(ldr);
    // Use manual target if override active, else sensor
    bool targetOn = room1_override ? room1_manualTarget : (LDRvalue > ldrThreshold);

    // Start/stop sequence
    if (targetOn && !turningOn) {
        turningOn = true;
        ledStage = 0;
        lastStepTime = now;
    } else if (!targetOn && turningOn) {
        turningOn = false;
        ledStage = 2;
        lastStepTime = now;
    }

    // Sequential LED control
    if (now - lastStepTime >= stepDelay) {
        lastStepTime = now;

        if (turningOn) {
            if (ledStage == 0) digitalWrite(ldrLED1, HIGH);
            else if (ledStage == 1) digitalWrite(ldrLED2, HIGH);
            else if (ledStage == 2) digitalWrite(ldrLED3, HIGH);

            ledStage++;
            if (ledStage > 2) ledStage = 2;

            // Skip LCD update if greeting is active
            if(!greetingActive) {
                lcd.setCursor(16,0);
                lcd.print("ON ");
            }
        } else {
            if (ledStage == 2) digitalWrite(ldrLED3, LOW);
            else if (ledStage == 1) digitalWrite(ldrLED2, LOW);
            else if (ledStage == 0) digitalWrite(ldrLED1, LOW);

            if (ledStage > 0) ledStage--;
            
            // Skip LCD update if greeting is active
            if(!greetingActive) {
                lcd.setCursor(16,0);
                lcd.print("OFF");
            }
        }
    }

    // Update actual state
    room1_state = digitalRead(ldrLED1) || digitalRead(ldrLED2) || digitalRead(ldrLED3);

    // Notify WebSocket if changed
    if (room1_state != lastState && now - lastNotifyTime > 200) {
        if (notify) notify(NAN, NAN);
        lastState = room1_state;
        lastNotifyTime = now;
    }
}
