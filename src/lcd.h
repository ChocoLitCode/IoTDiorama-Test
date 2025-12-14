#ifndef LCD_H
#define LCD_H

#include <Arduino.h>
#include <LiquidCrystal_I2C.h>


extern LiquidCrystal_I2C lcd;

bool setLCD();
void showLCD();
#endif