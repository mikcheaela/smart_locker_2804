#include "functions.h"

static unsigned long lastActivity = 0;
static const unsigned long SLEEP_TIMEOUT_MS = 5000;

void setup() {
  Serial.begin(9600);
  initSleep();
  resetEntry();

  if (checkPassword()){
    loadPassword();
    servoMoveTo(SERVO_LOCKED_ANGLE);
    state = LOCKED;
    Serial.println("\nLOCKED.\nEnter PIN:");
  } else {
    state = SETUP;
    Serial.println("\nNo PIN saved.\nEnter a 4-digit PIN.\nPress * to clear input and # to confirm.\n");
  }

  lastActivity = millis();
}

void loop() {
  servoUpdate();
  if (servoBusy()){
    lastActivity = millis();
    return;
  }

  char k = getKey();
  if (k == 0) {
    if (state != SETUP && (millis() - lastActivity) >= SLEEP_TIMEOUT_MS){
      resetEntry();
      Serial.println("\nSleeping");
      enterSleep();
      Serial.println("\nAwake");
      lastActivity = millis();
    }
    return;
  }
  lastActivity = millis();

  switch (state) {

    case SETUP:
      if (k == '*') {
        resetEntry();
        Serial.println("\nInput cleared.");
      } else if (k == '#') {
        if (entryPos != PW_LEN) {
          Serial.println("\nInvalid PIN length.");
        } else {
          strcpy(PASSWORD, entry);
          savePassword(PASSWORD);
          state = LOCKED;
          servoMoveTo(SERVO_LOCKED_ANGLE);
          Serial.println("\nPIN saved!\nLOCKED.\nEnter PIN:");
          resetEntry();
        }
      } else if (entryPos < PW_LEN) {
        Serial.print(k);
        entry[entryPos] = k;
        entryPos++;
        entry[entryPos] = '\0';
      }
      break;

    case LOCKED:
      if (k == '*') {
        resetEntry();
        Serial.println("\nInput cleared.");
      } else if (k == '#') {
        if (entryPos == 0) {
          // nothing entered, ignore
        } else if (codeMatches()) {
          state = UNLOCKED;
          servoMoveTo(SERVO_UNLOCKED_ANGLE);
          Serial.println("\nUNLOCKED");
          resetEntry();
        } else {
          Serial.println("\nWRONG");
          resetEntry();
        }
      } else if (entryPos < PW_LEN) {
        Serial.print(k);
        entry[entryPos] = k;
        entryPos++;
        entry[entryPos] = '\0';
      }
      break;

    case UNLOCKED:
      if (k == '#') {
        state = LOCKED;
        servoMoveTo(SERVO_LOCKED_ANGLE);
        resetEntry();
        Serial.println("\nLOCKED.\nEnter PIN:");
      }
      break;
  }
}
