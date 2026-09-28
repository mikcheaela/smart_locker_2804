#pragma once

const int PW_LEN = 4;
extern char PASSWORD[PW_LEN + 1];
extern char entry[PW_LEN + 1];
extern int entryPos;
enum State { SETUP, LOCKED, UNLOCKED };
extern State state;

// KEYPAD
char getKey();

// PIN
void resetEntry();
bool codeMatches();
bool checkPassword();
void savePassword(char* newPassword);
void loadPassword();