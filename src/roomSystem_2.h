#ifndef ROOMSYSTEM_2_H
#define ROOMSYSTEM_2_H

// Shared state (accessed from main.cpp)
extern bool soundDetected;          // declared here, defined in main.cpp
extern unsigned long lastSoundTime; // declared here, defined in main.cpp
extern bool room2_override;   
extern bool room2_state;            
extern bool room2_manualTarget;
extern int soundState;  // 0 = quiet, 1 = listening

// Accept a function pointer for notifying clients
void startRoomTwo(void (*notify)(float, float));
bool setRoomTwo();



#endif