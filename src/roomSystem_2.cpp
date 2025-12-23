#include "roomSystem_2.h"
#include "lcd.h"

// Pin declarations
const int sound = 35; 
const int led = 26;  

// Environment noise level states
enum Environment { QUIET, NORMAL, NOISY };
Environment currentEnv = NORMAL;

// Thresholds and timing constants
const int QUIET_THRESHOLD = 20;
const int NORMAL_THRESHOLD = 35;
const int NOISY_THRESHOLD = 60;
const int CLAP_TIMEOUT = 300;           // ms between valid claps
const int SAMPLE_WINDOW = 10;           // Number of samples per check
const int NOISE_FLOOR = 10;             // Minimum amplitude to consider sound
const int LISTENING_CONSISTENCY = 3;    // Number of consecutive detections for listening
const int SAMPLE_SIZE = 100;            // For baseline calculation
const int ADAPTATION_INTERVAL = 30000;  // ms between environment adaptation
const int ADAPTATION_SAMPLES = 50;      // Samples for adaptation

// Baseline and state variables
int baselineSum = 0;
int sampleCount = 0;
int dynamicThreshold = NORMAL_THRESHOLD;
int baselineNoise = 0;
bool room2_override = false;     // Manual override flag
bool room2_state = false;        // Current state (ON/OFF)
bool room2_manualTarget = false; // Target state in manual mode

// Timing variables
static unsigned long lastClapTime = 0;
static unsigned long lastNotifyTime = 0;
static unsigned long lastAdaptationTime = 0;
static unsigned long lastSoundCheckTime = 0;

// Sound state: 0 = quiet, 1 = listening, 2 = activated
int soundState = 0;
static int consecutiveSoundDetections = 0;
static int consecutiveQuietDetections = 0;
static int adaptationSamples[50];
static int adaptationIndex = 0;
static bool adaptationReady = false;

// External flag for greeting display
extern bool greetingActive;

// Initialize Room 2 hardware and baseline noise
bool setRoomTwo() {
    pinMode(sound, INPUT);
    pinMode(led, OUTPUT);
    digitalWrite(led, LOW);
    
    // Calculate initial baseline noise
    long sum = 0;
    for(int i = 0; i < 100; i++) {
        sum += analogRead(sound);
        delay(10);
    }
    baselineNoise = sum / 100;
    return true;
}

// Get threshold based on current environment
int getCurrentThreshold() {
    switch(currentEnv) {
        case QUIET: return QUIET_THRESHOLD;
        case NOISY: return NOISY_THRESHOLD;
        default: return NORMAL_THRESHOLD;
    }
}

// Update running baseline and dynamic threshold
void updateBaseline(int value) {
    baselineSum += value;
    sampleCount++;
    
    if (sampleCount >= SAMPLE_SIZE) {
        baselineNoise = baselineSum / SAMPLE_SIZE;
        int baseThreshold = getCurrentThreshold();
        dynamicThreshold = baselineNoise + baseThreshold;
        if (dynamicThreshold < 15) dynamicThreshold = 15;
        baselineSum = 0;
        sampleCount = 0;
    }
}

// Detect a clap event based on amplitude and timing
bool detectClap() {
    unsigned long now = millis();
    if (now - lastClapTime < CLAP_TIMEOUT) return false;
    
    int peak = 0, valley = 4095;
    for(int i = 0; i < SAMPLE_WINDOW; i++) {
        int sample = analogRead(sound);
        if(sample > peak) peak = sample;
        if(sample < valley) valley = sample;
        delayMicroseconds(500);
    }
    
    int amplitude = peak - valley;
    
    // Update sound state (listening vs quiet)
    if(now - lastSoundCheckTime >= 500) {
        lastSoundCheckTime = now;
        
        if(amplitude > NOISE_FLOOR) {
            consecutiveSoundDetections++;
            consecutiveQuietDetections = 0;
            if(consecutiveSoundDetections >= LISTENING_CONSISTENCY) {
                soundState = 1;
            }
        } else {
            consecutiveQuietDetections++;
            consecutiveSoundDetections = 0;
            if(consecutiveQuietDetections >= 10) {
                soundState = 0;
            }
        }
    }
    
    // Detect actual clap
    if(amplitude > dynamicThreshold) {
        delay(5);
        int afterPeak = analogRead(sound);
        if(afterPeak < peak - (amplitude / 2)) {
            lastClapTime = now;
            soundState = 2;
            return true;
        }
    }
    
    return false;
}

// Automatically adapt environment classification based on noise samples
void autoAdaptEnvironment() {
    unsigned long now = millis();
    
    if(adaptationIndex < ADAPTATION_SAMPLES) {
        adaptationSamples[adaptationIndex++] = analogRead(sound);
    } else {
        adaptationReady = true;
    }
    
    if(adaptationReady && (now - lastAdaptationTime >= ADAPTATION_INTERVAL)) {
        lastAdaptationTime = now;
        
        long sum = 0;
        int maxVal = 0, variance = 0;
        for(int i = 0; i < ADAPTATION_SAMPLES; i++) {
            sum += adaptationSamples[i];
            if(adaptationSamples[i] > maxVal) maxVal = adaptationSamples[i];
        }
        
        int avgNoise = sum / ADAPTATION_SAMPLES;
        for(int i = 0; i < ADAPTATION_SAMPLES; i++) {
            int diff = adaptationSamples[i] - avgNoise;
            variance += (diff * diff);
        }
        variance = variance / ADAPTATION_SAMPLES;
        
        Environment newEnv = currentEnv;
        if(avgNoise < 15 && variance < 50) {
            newEnv = QUIET;
        } else if(avgNoise > 30 || variance > 200) {
            newEnv = NOISY;
        } else {
            newEnv = NORMAL;
        }
        
        if(newEnv != currentEnv) {
            currentEnv = newEnv;
            baselineSum = 0;
            sampleCount = 0;
        }
        
        adaptationIndex = 0;
        adaptationReady = false;
    }
}

void startRoomTwo(void (*notify)(float,float)) {
    static bool lastState = false;
    static bool lastOverride = false;
    unsigned long now = millis();
    
    int sensorValue = analogRead(sound);
    autoAdaptEnvironment();
    updateBaseline(sensorValue);
    
    // Manual override logic
    if(room2_override){
        room2_state = room2_manualTarget;
    } else {
        if(lastOverride) lastClapTime = now;
        if(detectClap()) room2_state = !room2_state;
    }
    
    lastOverride = room2_override;
    digitalWrite(led, room2_state ? HIGH : LOW);
    
    // Update LCD if not in greeting mode
    if(!greetingActive) {
        lcd.setCursor(16,1);
        lcd.print(room2_state ? "ON " : "OFF");
    }
    
    // Notify if state changed
    if(room2_state != lastState && now - lastNotifyTime > 200){
        if(notify != nullptr) notify(NAN, NAN);
        lastState = room2_state;
        lastNotifyTime = now;
    }
}