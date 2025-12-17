#include <Arduino.h>
#include <WiFi.h>
#include <ESPAsyncWebServer.h>
#include <AsyncTCP.h>
#include <LittleFS.h>
#include "doorSystem.h"
#include "roomSystem_1.h"
#include "roomSystem_2.h"
#include "roomSystem_3.h"
#include "lcd.h"

// ===== WiFi Credentials (hardcoded) =====
const char* ssid = "DomusLink";
const char* password = "12345678";

// ===== WiFi Management =====
bool wifiConnected = false;
unsigned long lastWiFiCheck = 0;
const unsigned long WIFI_CHECK_INTERVAL = 10000;

// ===== Server & WebSocket =====
AsyncWebServer server(80);
AsyncWebSocket ws("/ws");

// ===== Sensor Values =====
float temperature = NAN;
float humidity = NAN;
float distance = NAN;
bool soundDetected = false;
unsigned long lastSoundTime = 0;
const unsigned long LISTENING_TIMEOUT = 5000;  // 5 seconds to transition from listening to quiet
const unsigned long QUIET_TIMEOUT = 3000;  // Need 3 seconds of consistent sound to show listening

// ===== Timing =====
unsigned long lastDHTRead = 0;
const unsigned long DHT_INTERVAL = 5000;
unsigned long lastBroadcast = 0;
const unsigned long WS_BROADCAST_INTERVAL = 5000;

// ===== Safe WebSocket Notification =====
void notifyClients(float temp, float hum) {
    if (!wifiConnected || ws.count() == 0) return;
    
    if (!isnan(temp)) temperature = temp;
    if (!isnan(hum)) humidity = hum;
    
    String tempStr = isnan(temperature) ? "0" : String(temperature, 1);
    String humStr = isnan(humidity) ? "0" : String(humidity, 1);
    
    // Always send actual state and mode separately
    String room1State = room1_state ? "ON" : "OFF";
    String room2State = room2_state ? "ON" : "OFF";
    String room1Mode = room1_override ? "MANUAL" : "AUTO";
    String room2Mode = room2_override ? "MANUAL" : "AUTO";
    String doorStr = doorOpen ? "UNLOCKED" : "LOCKED";
    
    // Determine sound state (soundState is declared in roomSystem_2.h)
    String soundStr;
    if (soundDetected) {
        soundStr = "\"detected\"";
    } else {
        soundStr = (soundState == 1) ? "\"listening\"" : "\"quiet\"";
    }

    String json = "{";
    json += "\"temperature\":" + tempStr + ",";
    json += "\"humidity\":" + humStr + ",";
    json += "\"room1\":\"" + room1State + "\",";
    json += "\"room1Mode\":\"" + room1Mode + "\",";
    json += "\"room2\":\"" + room2State + "\",";
    json += "\"room2Mode\":\"" + room2Mode + "\",";
    json += "\"door\":\"" + doorStr + "\",";
    json += "\"sound\":" + soundStr;
    json += "}";

    // Add debug output
    Serial.print("Sound state: ");
    Serial.print(soundState);
    Serial.print(" | Detected: ");
    Serial.print(soundDetected);
    Serial.print(" | Sending: ");
    Serial.println(soundStr);

    ws.textAll(json);
}

// ===== WebSocket Event Handler =====
void onWsEvent(AsyncWebSocket *serverPtr, AsyncWebSocketClient *client,
               AwsEventType type, void *arg, uint8_t *data, size_t len) {
    switch(type){
        case WS_EVT_CONNECT:
            Serial.println("WebSocket client connected");
            notifyClients(temperature, humidity);
            break;

        case WS_EVT_DISCONNECT:
            Serial.println("WebSocket client disconnected");
            break;

        case WS_EVT_DATA: {
            AwsFrameInfo *info = (AwsFrameInfo*)arg;
            if(info->final && info->opcode == WS_TEXT){
                String msg = String((char*)data, len);

                if(msg == "getReadings") {
                    notifyClients(temperature, humidity);
                }

                // Room 1 Control
                if(msg.startsWith("room1:")){
                    String state = msg.substring(6);
                    if(state == "ON") {
                        room1_override = true;
                        room1_manualTarget = true;
                    } else if(state == "OFF") {
                        room1_override = true;
                        room1_manualTarget = false;
                    } else if(state == "AUTO") {
                        room1_override = false;
                    }
                    notifyClients(temperature, humidity);
                }

                // Room 2 Control
                if(msg.startsWith("room2:")){
                    String state = msg.substring(6);
                    if(state == "ON") {
                        room2_override = true;
                        room2_manualTarget = true;
                    } else if(state == "OFF") {
                        room2_override = true;
                        room2_manualTarget = false;
                    } else if(state == "AUTO") {
                        room2_override = false;
                        room2_state = false;
                    }
                    notifyClients(temperature, humidity);
                }

                // Door Control
                if(msg == "unlockDoor"){
                    unlockDoor(ws);
                }
                if(msg == "lockDoor"){
                    lockDoor(ws);
                }
            }
            break;
        }

        default: break;
    }
}

// ===== WiFi Management =====
void initWiFi() {
    WiFi.mode(WIFI_AP);
    WiFi.setSleep(false);
    
    Serial.println("Starting Access Point...");
    
    // Create AP with SSID and password
    if(WiFi.softAP(ssid, password)) {
        wifiConnected = true;
        IPAddress IP = WiFi.softAPIP();
        Serial.println("AP Started Successfully");
        Serial.println("AP IP address: " + IP.toString());
        Serial.println("SSID: " + String(ssid));
        Serial.println("Password: " + String(password));
    } else {
        wifiConnected = false;
        Serial.println("AP Start Failed");
    }
}


// ===== Setup =====
void setup() {
    Serial.begin(115200);
    delay(100);
    
    Serial.println("\n=== ESP32 IoT System Starting ===");

    // Initialize hardware
    Serial.println("Initializing hardware...");
    if (!setLCD()) {
        Serial.println("LCD initialization failed!");
    }
    showLCD();
    if (!setRoomOne()) {
        Serial.println("Room 1 hardware initialization failed!");
    }
    if (!setRoomTwo()) {
        Serial.println("Room 2 hardware initialization failed!");
    }
    if (!setRoomThree()) {
        Serial.println("Room 3 hardware initialization failed!");
    }
    if (!setDoorPins()) {
        Serial.println("Door system hardware initialization failed!");
    }
    Serial.println("Hardware initialized");

    if(!LittleFS.begin()) {
        Serial.println("LittleFS mount failed");
    }

    // Initialize WiFi
    initWiFi();

    // WebSocket
    ws.onEvent(onWsEvent);
    server.addHandler(&ws);

    // ===== Serve Static Files =====
    server.on("/", HTTP_GET, [](AsyncWebServerRequest *request){
        request->send(LittleFS, "/index.html", "text/html");
    });
    
    server.on("/login.html", HTTP_GET, [](AsyncWebServerRequest *request){
        request->send(LittleFS, "/login.html", "text/html");
    });
    
    server.on("/script.js", HTTP_GET, [](AsyncWebServerRequest *request){
        request->send(LittleFS, "/script.js", "application/javascript");
    });
    
    server.on("/chart.js", HTTP_GET, [](AsyncWebServerRequest *request){
        request->send(LittleFS, "/chart.js", "application/javascript");
    });
    
    server.on("/style.css", HTTP_GET, [](AsyncWebServerRequest *request){
        request->send(LittleFS, "/style.css", "text/css");
    });

    // Serve all assets (fonts, icons, images) with automatic MIME type detection
    server.serveStatic("/assets/", LittleFS, "/assets/");

    // REST endpoint
    server.on("/readings", HTTP_GET, [](AsyncWebServerRequest *request){
        String tempStr = isnan(temperature) ? "0" : String(temperature, 1);
        String humStr = isnan(humidity) ? "0" : String(humidity, 1);
        String json = "{\"temperature\":" + tempStr + ",\"humidity\":" + humStr + "}";
        request->send(200, "application/json", json);
    });

    server.begin();
    Serial.println("HTTP server started");
    Serial.println("=== System Ready ===");
    Serial.println("Mode: " + String(wifiConnected ? "ONLINE" : "OFFLINE"));
}

// ===== Loop =====
void loop() {
    unsigned long now = millis();
   

    if(now - lastDHTRead >= DHT_INTERVAL){
        lastDHTRead = now;
        float t, h;
        getDHT(&h, &t);
        if(!isnan(t)) temperature = t;
        if(!isnan(h)) humidity = h;
    }

    // Pass WebSocket pointer to Room 3 for heat index alerts
    startRoomThree(&temperature, &humidity, &distance, &ws);
    startDoor(ws);
    startRoomOne(notifyClients);
    startRoomTwo(notifyClients);

    // Broadcast DHT readings every 5 seconds
    if(wifiConnected && now - lastBroadcast >= WS_BROADCAST_INTERVAL){
        lastBroadcast = now;
        
        // Only send temperature and humidity data
        String tempStr = isnan(temperature) ? "0" : String(temperature, 1);
        String humStr = isnan(humidity) ? "0" : String(humidity, 1);
        
        String json = "{";
        json += "\"temperature\":" + tempStr + ",";
        json += "\"humidity\":" + humStr;
        json += "}";
        
        ws.textAll(json);
    }

    // Broadcast room states more frequently (every 500ms)
    static unsigned long lastRoomBroadcast = 0;
    if(wifiConnected && now - lastRoomBroadcast >= 500){
        lastRoomBroadcast = now;
        
        // Always send actual state and mode separately
        String room1State = room1_state ? "ON" : "OFF";
        String room2State = room2_state ? "ON" : "OFF";
        String room1Mode = room1_override ? "MANUAL" : "AUTO";
        String room2Mode = room2_override ? "MANUAL" : "AUTO";
        String doorStr = doorOpen ? "UNLOCKED" : "LOCKED";
        
        // Add sound state to this broadcast
        String soundStr;
        if (soundDetected) {
            soundStr = "\"detected\"";
        } else {
            soundStr = (soundState == 1) ? "\"listening\"" : "\"quiet\"";
        }
        
        String json = "{";
        json += "\"room1\":\"" + room1State + "\",";
        json += "\"room1Mode\":\"" + room1Mode + "\",";
        json += "\"room2\":\"" + room2State + "\",";
        json += "\"room2Mode\":\"" + room2Mode + "\",";
        json += "\"door\":\"" + doorStr + "\",";
        json += "\"sound\":" + soundStr;
        json += "}";
        
        ws.textAll(json);
    }

    if(wifiConnected) {
        static unsigned long lastCleanup = 0;
        if(now - lastCleanup > 5000){
            lastCleanup = now;
            ws.cleanupClients();
        }
    }

    // Reset sound detection after 500ms, then broadcast new state
    static unsigned long lastSoundStateCheck = 0;
    if(soundDetected && millis() - lastSoundTime >= 500){
        soundDetected = false;
        notifyClients(temperature, humidity);  // Broadcast to update badge
    }

    delay(1);
}