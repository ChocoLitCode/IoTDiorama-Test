#ifndef DOORSYSTEM_H
#define DOORSYSTEM_H

#include <Arduino.h>
#include <ESP32Servo.h>
#include <ESPAsyncWebServer.h>

// Pin Declarations
const int touch1 = 2;
const int touch2 = 4;
const int accessLED = 14;
const int intruderLED = 27;
const int buzzer = 16;
const int servo = 23;

// Counters/timers
extern int failAttempts;
extern unsigned long touchStart;
extern bool doorOpen;


// Functions
void setDoorPins();
void startDoor(AsyncWebSocket& ws);
void unlockDoor(AsyncWebSocket& ws);
void lockDoor(AsyncWebSocket& ws);

#endif
