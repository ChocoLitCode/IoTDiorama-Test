#ifndef DOORSYSTEM_H
#define DOORSYSTEM_H

#include <Arduino.h>
#include <ESP32Servo.h>
#include <ESPAsyncWebServer.h>

// Counters/timers (shared state)
extern int failAttempts;
extern unsigned long touchStart;
extern bool doorOpen;


// Functions
bool setDoorPins();
void startDoor(AsyncWebSocket& ws);
void unlockDoor(AsyncWebSocket& ws);
void lockDoor(AsyncWebSocket& ws);

#endif
