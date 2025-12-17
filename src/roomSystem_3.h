#ifndef ROOMSYSTEM_3_H
#define ROOMSYSTEM_3_H

#include <DHT.h>
#include <ESPAsyncWebServer.h>

// Shared objects (accessed from main.cpp)
extern DHT dht22;
extern bool greetingActive;

// Initialize module
bool setRoomThree();

// Read DHT sensor
void getDHT(float* humidity, float* temperature);

// Non-blocking update function
void startRoomThree(float* temperature, float* humidity, float* distance, AsyncWebSocket* ws);

#endif
