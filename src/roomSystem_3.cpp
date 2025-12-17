#include "roomSystem_3.h"
#include "lcd.h"

// Pin Declarations
const int DHT22_PIN = 17;
const int trig = 19;
const int echo = 18;

DHT dht22(DHT22_PIN, DHT22);

// Timing for greeting
unsigned long lastGreetingTime = 0;
unsigned long lastAnimationUpdate = 0;
static const unsigned long greetingDuration = 5000; // 5 seconds
static const unsigned long animationSpeed = 300; // ms per frame
bool greetingActive = false;
bool presenceDetected = false;
int animationFrame = 0;
static int selectedMessage = 0; // Store the randomly selected message

// Heat Index Alert System
unsigned long lastHeatIndexCheck = 0;
const unsigned long HEAT_INDEX_CHECK_INTERVAL = 60000; // Check every 60 seconds
String lastHeatIndexLevel = "none";

bool setRoomThree(){
    pinMode(trig, OUTPUT);
    pinMode(echo, INPUT);
    dht22.begin();
    randomSeed(analogRead(0)); // Initialize random seed
    return true;
}

void getDHT(float* humidity, float* temperature){
    *humidity = dht22.readHumidity();
    *temperature = dht22.readTemperature();
}

void showGreetingAnimation() {
    unsigned long now = millis();
    static int lastStarPos = -1;
    
    // Update star positions every 300ms
    if(now - lastAnimationUpdate >= animationSpeed) {
        lastAnimationUpdate = now;
        
        // Clear previous stars if not first frame
        if(lastStarPos >= 0) {
            lcd.setCursor(lastStarPos, 3);
            lcd.print(" ");
            lcd.setCursor(lastStarPos + 5, 3);
            lcd.print(" ");
            lcd.setCursor(lastStarPos + 10, 3);
            lcd.print(" ");
            lcd.setCursor(lastStarPos + 15, 3);
            lcd.print(" ");
        }
        
        // Draw new stars
        int starPos = (animationFrame % 4);
        lcd.setCursor(starPos, 3);
        lcd.print("*");
        lcd.setCursor(starPos + 5, 3);
        lcd.print("*");
        lcd.setCursor(starPos + 10, 3);
        lcd.print("*");
        lcd.setCursor(starPos + 15, 3);
        lcd.print("*");
        
        lastStarPos = starPos;
        animationFrame++;
    }
}

String checkHeatIndexLevel(float heatIndex) {
    if(heatIndex >= 52.0) {
        return "extreme_danger";
    } else if(heatIndex >= 42.0) {
        return "danger";
    } else if(heatIndex >= 33.0) {
        return "extreme_caution";
    } else if(heatIndex >= 27.0) {
        return "caution";
    }
    return "none";
}

void startRoomThree(float* temperature, float* humidity, float* distance, AsyncWebSocket* ws){
    // Read DHT
    getDHT(humidity, temperature);
    // Compute heat index in Celsius (isFahreheit = false)
    float hic = dht22.computeHeatIndex(*temperature, *humidity, false);
   
    // Ultrasonic
    digitalWrite(trig, HIGH);
    delayMicroseconds(10);
    digitalWrite(trig, LOW);
    float duration_us = pulseIn(echo, HIGH, 30000); // 30ms timeout

    // Convert only if valid pulse was read
    if(duration_us > 0){
        *distance = 0.017 * duration_us; // cm
    } else {
        *distance = NAN; // invalid reading
    }

    unsigned long now = millis();

    // ---- HEAT INDEX MONITORING ----
    if(!isnan(hic) && (now - lastHeatIndexCheck >= HEAT_INDEX_CHECK_INTERVAL)) {
        lastHeatIndexCheck = now;
        String currentLevel = checkHeatIndexLevel(hic);
        
        // Only send alert if level changed and is not "none"
        if(currentLevel != lastHeatIndexLevel && currentLevel != "none") {
            Serial.print("Heat Index Alert: ");
            Serial.print(hic);
            Serial.print("°C - Level: ");
            Serial.println(currentLevel);
            
            // Send alert to web interface if websocket provided
            if(ws != nullptr && ws->count() > 0) {
                String json = "{\"heatIndexAlert\":\"" + currentLevel + "\",\"heatIndex\":" + String(hic, 1) + "}";
                ws->textAll(json);
            }
            
            lastHeatIndexLevel = currentLevel;
        } else if(currentLevel == "none" && lastHeatIndexLevel != "none") {
            // Heat index returned to safe levels
            lastHeatIndexLevel = "none";
        }
    }

    // ---- DETECT PRESENCE ----
    bool detected = (!isnan(*distance) && *distance < 10);

    // Only trigger greeting if not already active and presence newly detected
    if(detected && !presenceDetected && !greetingActive){
        Serial.println(">>> GREETING TRIGGERED <<<");
        
        // Select message based on temp conditions
        if(*temperature >= 32.0) {
            selectedMessage = 5;
            Serial.println("Selected message: Hot");
        } else if(*temperature < 22.0) {
            selectedMessage = 6;
            Serial.println("Selected message: Cold");
        } else {
            selectedMessage = random(0, 5);
            Serial.print("Selected message: ");
            Serial.println(selectedMessage);
        }
        
        // Clear and display greeting message once
        lcd.clear();
        switch(selectedMessage) {
            case 0:
                lcd.setCursor(6, 1);
                lcd.print("Welcome!");
                break;
            case 1:
                lcd.setCursor(4, 1);
                lcd.print("Hello There!");
                break;
            case 2:
                lcd.setCursor(5, 1);
                lcd.print("Hi There!");
                break;
            case 3:
                lcd.setCursor(3, 1);
                lcd.print("Good to see");
                lcd.setCursor(7, 2);
                lcd.print("you!");
                break;
            case 4:
                lcd.setCursor(5, 1);
                lcd.print("Greetings!");
                break;
            case 5:
                lcd.setCursor(3, 1);
                lcd.print("Whew! So Hot!");
                lcd.setCursor(1, 2);
                lcd.print("  Stay Hydrated!");
                break;
            case 6:
                lcd.setCursor(3, 1);
                lcd.print("Brrr! It's Cold");
                lcd.setCursor(4, 2);
                lcd.print("   Warm Up!");
                break;
        }
        
        greetingActive = true;
        lastGreetingTime = now;
        lastAnimationUpdate = now;
        animationFrame = 0;
        presenceDetected = true;
    }

    // Reset presence flag only when user leaves AND greeting is finished
    if(!detected && !greetingActive){
        presenceDetected = false;
    }

    // ---- SHOW ANIMATED GREETING ----
    if(greetingActive) {
        showGreetingAnimation();
        
        // End greeting after duration
        if(now - lastGreetingTime >= greetingDuration){
            greetingActive = false;
            lcd.clear(); // Clear before restoring normal display
            showLCD(); // restore static labels
            // Don't reset presenceDetected here - wait for user to leave
        }
        return; // skip normal LCD updates
    }

    // ---- UPDATE DYNAMIC VALUES ----
    if(!isnan(*temperature)){
        lcd.setCursor(6,2);
        lcd.print(*temperature,1);       // 1 decimal
        lcd.setCursor(11,2);
        lcd.print((char)223);             // degree symbol
    }

    if(!isnan(*humidity)){
        lcd.setCursor(10,3);
        lcd.print(*humidity,1);           // 1 decimal
    }
}