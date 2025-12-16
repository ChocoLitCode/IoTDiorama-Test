#ifndef ROOMSYSTEM_1_H
#define ROOMSYSTEM_1_H

// Shared state (accessed from main.cpp)
extern bool room1_override;   // true if web overrides
extern bool room1_state;      // actual LED state
extern bool room1_manualTarget;   // desired state: true=ON, false=OFF


void setRoomOne();
void startRoomOne(void (*notify)(float,float));

#endif
