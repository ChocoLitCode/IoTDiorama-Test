#include "roomSystem_2.h"
#include "lcd.h"

bool room2_override = false;
bool room2_state = false;
bool room2_manualTarget = false;

// Clap detection state
static unsigned long lastClapTime = 0;
static int baseline = 0;
int soundState = 0;  // 0 = quiet, 1 = listening
static int consecutiveSoundDetections = 0;
static int consecutiveQuietDetections = 0;
static unsigned long lastSoundCheckTime = 0;

// Extern declarations for global sound detection
extern bool soundDetected;
extern unsigned long lastSoundTime;

// Extern for greeting state
extern bool greetingActive;

void setRoomTwo() {
    pinMode(sound, INPUT);
    pinMode(soundLED, OUTPUT);
    digitalWrite(soundLED, LOW);
    
    // Calibrate baseline noise level
    Serial.println("Calibrating sound sensor...");
    long sum = 0;
    for(int i = 0; i < 100; i++) {
        sum += analogRead(sound);
        delay(10);
    }
    baseline = sum / 100;
}

bool detectClap() {
    unsigned long now = millis();
    
    // Debounce: ignore if clap detected recently
    if (now - lastClapTime < CLAP_TIMEOUT) {
        return false;
    }
    
    // Take multiple samples to find peak
    int peak = 0;
    int valley = 4095;
    
    for(int i = 0; i < SAMPLE_WINDOW; i++) {
        int sample = analogRead(sound);
        if(sample > peak) peak = sample;
        if(sample < valley) valley = sample;
        delayMicroseconds(500);
    }
    
    // Calculate amplitude (peak-to-peak)
    int amplitude = peak - valley;
    
    // Track ambient sound for "listening" state with smoothing
    // Only update state every 500ms to avoid rapid changes
    if(now - lastSoundCheckTime >= 500) {
        lastSoundCheckTime = now;
       
        
        if(amplitude > NOISE_FLOOR) {
            consecutiveSoundDetections++;
            consecutiveQuietDetections = 0;
            
            // Need LISTENING_CONSISTENCY consecutive detections to switch to "listening"
            if(consecutiveSoundDetections >= LISTENING_CONSISTENCY) {
                if(soundState != 1) {
                    Serial.println(">>> STATE CHANGED TO LISTENING <<<");
                }
                soundState = 1;  // listening
            }
        } else {
            consecutiveQuietDetections++;
            consecutiveSoundDetections = 0;
            
            // Need 10 consecutive quiet readings (5 seconds) to switch to "quiet"
            if(consecutiveQuietDetections >= 10) {
                if(soundState != 0) {
                    Serial.println(">>> STATE CHANGED TO QUIET <<<");
                }
                soundState = 0;  // quiet
            }
        }
    }
    
    // Check if it's a sharp spike above threshold (clap detection)
    bool isClap = false;
    
    if(amplitude > CLAP_THRESHOLD) {
        // Additional validation: check for quick decay
        delay(5);
        int afterPeak = analogRead(sound);
        
        // Clap should decay quickly
        if(afterPeak < peak - (amplitude / 2)) {
            isClap = true;
            lastClapTime = now;
            
            // Update global sound detection
            soundDetected = true;
            lastSoundTime = now;
            
            Serial.println(">>> CLAP DETECTED <<<");
        }
    }
    
    return isClap;
}

void startRoomTwo(void (*notify)(float,float)) {
    static bool lastState = false;
    static bool lastOverride = false;
    static unsigned long lastNotifyTime = 0;
    unsigned long now = millis();

    if(room2_override){
        // Manual override: use manualTarget
        room2_state = room2_manualTarget;
    } else {
        // Auto mode: detect clap
        if(lastOverride){
            // Just switched from override to auto
            lastClapTime = now;
        }
        
        if(detectClap()) {
            room2_state = !room2_state; // Toggle on clap
        }
    }

    lastOverride = room2_override;
    
    // Update hardware
    digitalWrite(soundLED, room2_state ? HIGH : LOW);
    
    // Skip LCD update if greeting is active
    if(!greetingActive) {
        lcd.setCursor(16,1);
        lcd.print(room2_state ? "ON " : "OFF");
    }

    // Notify WebSocket if state changed
    if(room2_state != lastState && now - lastNotifyTime > 200){
        if(notify != nullptr) {
            notify(NAN, NAN);
        }
        lastState = room2_state;
        lastNotifyTime = now;
    }
}