#pragma once
#include <Arduino.h>

const int PW_LEN = 4;

// KEYPAD
extern char entry[PW_LEN + 1];
extern int entryPos;
char getKey();

// STATE MACHINE
enum State { SETUP, LOCKED, UNLOCKED };
extern State state;

void resetEntry();
bool codeMatches();

// PIN & EEPROM
extern char PASSWORD[PW_LEN + 1];

bool checkPassword();
void savePassword(char* newPassword);
void loadPassword();

// SERVO
const int SERVO_LOCKED_ANGLE   = 28;
const int SERVO_UNLOCKED_ANGLE = 120;

void servoMoveTo(int angle);
void servoUpdate();
bool servoBlocked();
bool servoBusy();

// SLEEP
void initSleep();
void enterSleep();