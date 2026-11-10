#include "functions.h"

static unsigned long lastActivity = 0;
static const unsigned long SLEEP_TIMEOUT_MS = 5000;
static State prevState = LOCKED;
static bool servoWasBusy = false;

void setup() {
  pinMode(9, OUTPUT);
  digitalWrite(9, LOW);
  Serial.begin(9600);

  uiInit();
  batteryInit();
  initSleep();
  resetEntry();

  if (checkPassword()) {
    loadPassword();
    servoMoveTo(SERVO_LOCKED_ANGLE);
    state = LOCKED;
    Serial.println("\nLOCKED.\nEnter PIN:");
  } else {
    state = SETUP;
    Serial.println("\nNo PIN saved.\nEnter a 4-digit PIN.\nPress * to clear input and # to confirm.\n");
  }

  showStateLEDs();
  lastActivity = millis();
}

void loop() {
  servoUpdate();

  if (servoBusy()) {
    servoWasBusy = true;        // remember we were moving
    lastActivity = millis();
    return;
  }

  if (servoWasBusy) {
    servoWasBusy = false;
    batteryStart();             // measure battery once, right after a servo move
  }

  batteryUpdate();

  // Handle Motor Stalling Exception
  if (servoBlocked()) {
    state = prevState;
    resetEntry();
    showStateLEDs();
    Serial.println("\nMOTOR BLOCKED - reverted to previous state");
    if (state == LOCKED)        Serial.println("LOCKED.\nEnter PIN:");
    else if (state == UNLOCKED) Serial.println("UNLOCKED");
    else                        Serial.println("Enter a 4-digit PIN.");
    lastActivity = millis();
    return;
  }

  char k = getKey();
  if (k == 0) {
    if (state != SETUP && (millis() - lastActivity) >= SLEEP_TIMEOUT_MS) {
      resetEntry();
      Serial.println("\nSleeping");
      uiOff();
      enterSleep();

      delay(20);

      Serial.println("\nAwake");
      showStateLEDs();
      lastActivity = millis();
      batteryStart();           // check battery on wake
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
          savePassword(entry);          // stores salt + hash, not the PIN
          prevState = state;
          state = LOCKED;
          servoMoveTo(SERVO_LOCKED_ANGLE);
          beepCorrect();
          showStateLEDs();
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
          // ignore empty submit
        } else if (codeMatches()) {
          prevState = state;
          state = UNLOCKED;
          servoMoveTo(SERVO_UNLOCKED_ANGLE);
          beepCorrect();
          showStateLEDs();
          Serial.println("\nUNLOCKED");
          resetEntry();
        } else {
          Serial.println("\nWRONG");
          showWrong();
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
        prevState = state;
        state = LOCKED;
        servoMoveTo(SERVO_LOCKED_ANGLE);
        showStateLEDs();
        resetEntry();
        Serial.println("\nLOCKED.\nEnter PIN:");
      }
      break;
  }
}