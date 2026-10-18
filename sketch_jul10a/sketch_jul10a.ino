#include <LiquidCrystal.h>

// LCD: RS, E, D4, D5, D6, D7
LiquidCrystal lcd(12, 11, 5, 4, 3, 2);

// Joystick pins
const int joyX = A1;
const int joyY = A2;
const int joySW = 7;

void setup() {
  pinMode(joySW, INPUT_PULLUP);

  lcd.begin(16, 2);
  lcd.clear();
}

void loop() {
  int x = analogRead(joyX);
  int y = analogRead(joyY);
  bool pressed = !digitalRead(joySW);

  // First line
  lcd.setCursor(0, 0);
  lcd.print("X:");
  lcd.print(x);
  lcd.print("     ");      // erase leftover digits

  // Second line
  lcd.setCursor(0, 1);
  lcd.print("Y:");
  lcd.print(y);
  lcd.print("     ");

  lcd.setCursor(11, 1);
  lcd.print("SW:");
  lcd.print(pressed ? "ON " : "OFF");

  delay(100);
}