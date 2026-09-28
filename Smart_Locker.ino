// Keypad initialization
#include "functions.h"

void setup() {
  Serial.begin(9600);
  resetEntry();

  if (checkPassword()){
    loadPassword();
    state = LOCKED;
    Serial.println("\nLOCKED.\nEnter PIN:");
  } else {
    state = SETUP;
    Serial.println("\nNo PIN saved.\nEnter a 4-digit PIN.\nPress * to clear input and # to confirm.\n");
  }
}

void loop() {
  char k = getKey();
  if (k == 0) { delay(5); return; }

  if (state == SETUP) {
    if (k == '*') {
      resetEntry();
      Serial.println("\nInput cleared.");
    } else if (k == '#'){
      if (entryPos != PW_LEN){
        Serial.println("\nInvalid PIN length.");
      } else {
        strcpy(PASSWORD, entry);
        savePassword(PASSWORD);
        state = LOCKED;
        Serial.println("\nPIN saved!\nLOCKED.\nEnter PIN:");
        resetEntry();
      }
    } else if (entryPos < PW_LEN){
      Serial.print(k);
      entry[entryPos] = k;
      entryPos++;
      entry[entryPos] = '\0';
    }

  } else if (state == LOCKED) {
    if (k == '*') {
      resetEntry();
      Serial.println("\nInput cleared.");
    }
    else if (k == '#') {
      if (entryPos == 0) {
        // do nothing
      } else if (codeMatches()) {
        state = UNLOCKED;
        Serial.println("\nUNLOCKED");
        resetEntry();
      } else {
        Serial.println("\nWRONG");
        resetEntry();
      }
    }
    else if (entryPos < PW_LEN) {
      Serial.print(k);
      entry[entryPos] = k;
      entryPos++;
      entry[entryPos] = '\0';
    }
  } else { // UNLOCKED
    if (k == '#') {
      state = LOCKED;
      resetEntry();
      Serial.println("\nLOCKED.\nEnter PIN:");
    }
  }

  delay(5);
}
