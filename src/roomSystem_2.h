#ifndef ROOMSYSTEM_2_H
#define ROOMSYSTEM_2_H

const int sound = 35;   
const int soundLED = 26;


// At the top of roomSystem_2.cpp, add extern declarations
extern bool soundDetected;
extern unsigned long lastSoundTime;
extern bool room2_override;   
extern bool room2_state;            
extern bool room2_manualTarget;
extern int soundState;  // 0 = quiet, 1 = listening

// Clap detection parameters
const int CLAP_THRESHOLD = 2500;        // Minimum peak to detect clap
const int NOISE_FLOOR = 10;            // Background noise level to detect sound
const int CLAP_TIMEOUT = 50;            // Minimum ms between claps
const int SAMPLE_WINDOW = 10;           // Number of samples to check for peak
const int LISTENING_CONSISTENCY = 3;    // Number of consecutive detections needed for "listening"

// Accept a function pointer for notifying clients
void startRoomTwo(void (*notify)(float, float));
void setRoomTwo();



#endif