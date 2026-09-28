#include "functions.h"

// KEYPAD

struct Key {
  int value;
  char label;
};

static const int KEYPAD_PIN = A0;
static const int NUM_KEYS = 12;
static const int IDLE_LEVEL = 900;
static const int CONFIRM = 8;

static char stableKey = 0;
static char candidate = 0;
static int  sameCount = 0;

static Key keys[] = {
  {3, '1'}, {185, '4'}, {328, '7'}, {512, '*'},
  {615, '2'}, {648, '5'}, {681, '8'}, {732, '0'},
  {786, '3'}, {798, '6'}, {810, '9'}, {832, '#'}
};

static char decodeKey(int reading) {
  if (reading >= IDLE_LEVEL) return 0;
  char best = 0;
  int  bestDist = 2000;
  for (int i = 0; i < NUM_KEYS; i++) {
    int dist = abs(reading - keys[i].value);
    if (dist < bestDist) { bestDist = dist; best = keys[i].label; }
  }
  return best;
}

static int readAveraged() {
  long sum = 0;
  for (int i = 0; i < 8; i++) sum += analogRead(KEYPAD_PIN);
  return sum / 8;
}

char getKey() {   // public
  char now = decodeKey(readAveraged());
  if (now == candidate) {
    if (sameCount < CONFIRM) sameCount++;
  } else {
    candidate = now;
    sameCount = 1;
  }
  if (sameCount < CONFIRM) return 0;
  if (candidate == stableKey) return 0;
  stableKey = candidate;
  if (stableKey == 0) return 0;
  return stableKey;
}

// STATE MACHINE

char  PASSWORD[PW_LEN + 1];
char  entry[PW_LEN + 1];
int   entryPos = 0;
State state;

void resetEntry() {
  entryPos = 0;
  entry[0] = '\0';
}

bool codeMatches() {
  if (entryPos != PW_LEN) return false;
  for (int i = 0; i < PW_LEN; i++) {
    if (entry[i] != PASSWORD[i]) return false;
  }
  return true;
}

// PIN

#define ReadEnable (1 << 0)
#define WriteEnable (1 << 1)
#define MasterWriteEnable (1 << 2)

static volatile uint8_t* const EECR_addr = (volatile uint8_t*)0x3F;
static volatile uint8_t* const EEDR_addr = (volatile uint8_t*)0x40;
static volatile uint8_t* const EEARL_addr = (volatile uint8_t*)0x41;
static volatile uint8_t* const EEARH_addr = (volatile uint8_t*)0x42;

static const uint16_t FLAG_ADDR = 0;
static const uint16_t PW_START_ADDR = 1;
static const uint8_t FLAG = 0x00;

static uint8_t readByte(uint16_t addr) {
  while (*EECR_addr & WriteEnable) {}
  *EEARH_addr = (addr >> 8);
  *EEARL_addr = (addr & 0xFF);
  *EECR_addr |= ReadEnable;
  return *EEDR_addr;
}

static void writeByte(uint16_t addr, uint8_t data) {
  while (*EECR_addr & WriteEnable) {}
  *EEARH_addr = (addr >> 8);
  *EEARL_addr = (addr & 0xFF);
  *EEDR_addr = data;
  *EECR_addr |= MasterWriteEnable;
  *EECR_addr |= WriteEnable;
}

bool checkPassword() {
  return readByte(FLAG_ADDR) == FLAG;
}

void savePassword(char* newPassword) {
  for (int i = 0; i < PW_LEN; ++i) {
    writeByte(PW_START_ADDR + i, (uint8_t)newPassword[i]);
  }
  writeByte(FLAG_ADDR, FLAG);
}

void loadPassword() {
  for (int i = 0; i < PW_LEN; ++i) {
    PASSWORD[i] = (char)readByte(PW_START_ADDR + i);
  }
  PASSWORD[PW_LEN] = '\0';
}

// SERVO

static const int SERVO_PIN = 9;

static const unsigned long PERIOD_US = 20000;
static const unsigned long PULSE_MIN_US = 500;
static const unsigned long PULSE_MAX_US = 2500;
static const unsigned long MOVE_TIME_MS = 500;

static unsigned long pulseWidth = 1500;
static bool servoEnabled = false;
static unsigned long startTime = 0;
static unsigned long moveStart_ms = 0;

static void setAngle(int angle) {
  angle = constrain(angle, 0, 180);
  pulseWidth = map(angle, 0, 180, PULSE_MIN_US, PULSE_MAX_US);
}

static void servoON() {
  servoEnabled = true;
  pinMode(SERVO_PIN, OUTPUT);
  startTime = micros();
}

static void servoOFF() {
  servoEnabled = false;
  digitalWrite(SERVO_PIN, LOW);
}

void servoMoveTo(int angle) {
  setAngle(angle);
  servoON();
  moveStart_ms = millis();
}

bool servoBusy() {
  return servoEnabled;
}

void servoUpdate() {
  if (!servoEnabled) return;

  if (millis() - moveStart_ms >= MOVE_TIME_MS) {
    servoOFF();
    return;
  }

  unsigned long elapsedTime = micros() - startTime;

  if (elapsedTime >= pulseWidth) {
    digitalWrite(SERVO_PIN, LOW);
  } else {
    digitalWrite(SERVO_PIN, HIGH);
  }

  if (elapsedTime >= PERIOD_US) {
    startTime += PERIOD_US;
  }
}