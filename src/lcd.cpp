#include "lcd.h"

LiquidCrystal_I2C lcd(0x27, 20, 4); // I2C address 0x27, 20 columns x 4 rows

bool setLCD(){
    lcd.init();
    lcd.backlight();
    return true;
}

// Show static labels with space reserved for dynamic values
void showLCD(){
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("Room 1 Lights:     ");  // 5 spaces reserved for values
    lcd.setCursor(0, 1);
    lcd.print("Room 2 Lights:     ");
    lcd.setCursor(0, 2);
    lcd.print("Temp:       C");         // leave 6 spaces for temperature
    lcd.setCursor(0, 3);
    lcd.print("Humidity:     %");       // leave 4 spaces for humidity
}
