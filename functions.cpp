#include "functions.h"

// ==========================================
// KEYPAD IMPLEMENTATION
// ==========================================

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
  {791, '3'}, {802, '6'}, {813, '9'}, {832, '#'}
};

static char decodeKey(int reading) {
  if (reading >= IDLE_LEVEL) return 0;
  char best = 0;
  int  bestDist = 2000;
  for (int i = 0; i < NUM_KEYS; i++) {
    int dist = abs(reading - keys[i].value);
    if (dist < bestDist) {
      bestDist = dist;
      best = keys[i].label;
    }
  }
  return best;
}

static int readKeypad() {
  const int N = 31;
  int s[N];
  for (int i = 0; i < N; i++) { s[i] = analogRead(KEYPAD_PIN); delayMicroseconds(150); }
  for (int i = 0; i < N; i++)
    for (int j = i + 1; j < N; j++)
      if (s[j] < s[i]) { int t = s[i]; s[i] = s[j]; s[j] = t; }
  return s[N / 2];
}

char getKey() {
  char now = decodeKey(readKeypad());
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

// ==========================================
// STATE MACHINE DATA & HELPERS
// ==========================================

char  entry[PW_LEN + 1];
int   entryPos = 0;
State state;

static uint8_t  salt[4];        // random per-device salt
static uint32_t storedHash;     // hash of the saved PIN
static uint32_t hashPin(const char* pin);   // defined in EEPROM section

void resetEntry() {
  entryPos = 0;
  entry[0] = '\0';
}

bool codeMatches() {
  if (entryPos != PW_LEN) return false;
  return hashPin(entry) == storedHash;   // compare hashes, not the PIN
}

// ==========================================
// DIRECT EEPROM REGISTER FUNCTIONS (hashed password)
// ==========================================

#define ReadEnable (1 << 0)
#define WriteEnable (1 << 1)
#define MasterWriteEnable (1 << 2)

static volatile uint8_t* const EECR_addr  = (volatile uint8_t*)0x3F;
static volatile uint8_t* const EEDR_addr  = (volatile uint8_t*)0x40;
static volatile uint8_t* const EEARL_addr = (volatile uint8_t*)0x41;
static volatile uint8_t* const EEARH_addr = (volatile uint8_t*)0x42;

static const uint16_t FLAG_ADDR = 0;
static const uint16_t SALT_ADDR = 1;   // 4 bytes
static const uint16_t HASH_ADDR = 5;   // 4 bytes
static const uint8_t  FLAG = 0x00;

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

// Salted, stretched FNV-1a hash. The PIN itself is never stored.
static uint32_t hashPin(const char* pin) {
  uint32_t h = 2166136261UL;                 // FNV offset basis
  for (int i = 0; i < 4; i++) {              // mix in salt
    h ^= salt[i];
    h *= 16777619UL;                         // FNV prime
  }
  for (int i = 0; i < PW_LEN; i++) {         // mix in PIN
    h ^= (uint8_t)pin[i];
    h *= 16777619UL;
  }
  for (uint16_t n = 0; n < 1000; n++) {      // stretch: slow brute force
    h ^= (h >> 13);
    h *= 16777619UL;
  }
  return h;
}

bool checkPassword() {
  return readByte(FLAG_ADDR) == FLAG;        // is a credential saved?
}

void savePassword(char* newPassword) {
  uint32_t seed = micros();                  // lightweight entropy for the salt
  if (seed == 0) seed = 1;
  for (int i = 0; i < 4; i++) {
    seed ^= seed << 13; seed ^= seed >> 17; seed ^= seed << 5;   // xorshift
    salt[i] = (uint8_t)seed;
  }
  storedHash = hashPin(newPassword);
  for (int i = 0; i < 4; i++) writeByte(SALT_ADDR + i, salt[i]);
  for (int i = 0; i < 4; i++) writeByte(HASH_ADDR + i, (uint8_t)(storedHash >> (8 * i)));
  writeByte(FLAG_ADDR, FLAG);
}

void loadPassword() {
  for (int i = 0; i < 4; i++) salt[i] = readByte(SALT_ADDR + i);
  storedHash = 0;
  for (int i = 0; i < 4; i++) storedHash |= ((uint32_t)readByte(HASH_ADDR + i)) << (8 * i);
}

// ==========================================
// SERVO DRIVER & STALL DETECTION
// ==========================================

static const int SERVO_PIN = 9;
static const int SENSE_PIN = A1;

static const unsigned long PERIOD_US    = 20000;
static const unsigned long PULSE_MIN_US  = 500;
static const unsigned long PULSE_MAX_US  = 2500;
static const unsigned long MOVE_TIME_MS  = 500;

static const int STALL_THRESHOLD = 30;

static unsigned long pulseWidth   = 1500;
static bool          servoEnabled = false;
static unsigned long startTime    = 0;
static unsigned long moveStart_ms = 0;

static int  targetAngle = SERVO_LOCKED_ANGLE;
static int  prevAngle   = SERVO_LOCKED_ANGLE;
static bool reverting   = false;
static bool blocked     = false;

static unsigned long lastSense_ms = 0;
static int  curReading = 0;

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
  if (!reverting) {
    prevAngle = targetAngle;
    blocked   = false;
  }
  targetAngle = angle;
  setAngle(angle);
  servoON();
  moveStart_ms = millis();
  lastSense_ms = millis();
}

bool servoBusy()    { return servoEnabled; }
bool servoBlocked() { bool b = blocked; blocked = false; return b; }

void servoUpdate() {
  if (!servoEnabled) return;

  unsigned long moveElapsed = millis() - moveStart_ms;

  if (millis() - lastSense_ms >= 20) {
    curReading = analogRead(SENSE_PIN);
    lastSense_ms = millis();
  }

  if (moveElapsed >= MOVE_TIME_MS) {
    if (!reverting && curReading >= STALL_THRESHOLD) {
      Serial.print("  end-of-move current = "); Serial.print(curReading);
      Serial.println("  >= 30  -> STALL detected, reverting");
      blocked   = true;
      reverting = true;
      servoMoveTo(prevAngle);
      return;
    }
    if (!reverting) {
      Serial.print("  end-of-move current = "); Serial.print(curReading);
      Serial.println("  < 30  -> move OK");
    }
    servoOFF();
    reverting = false;
    return;
  }

  unsigned long elapsedTime = micros() - startTime;
  if (elapsedTime >= pulseWidth) digitalWrite(SERVO_PIN, LOW);
  else                           digitalWrite(SERVO_PIN, HIGH);
  if (elapsedTime >= PERIOD_US)  startTime += PERIOD_US;
}

// ==========================================
// LOW-POWER SLEEP IMPLEMENTATION
// ==========================================

#define ROW_MASK             (1 << 4)
#define ADCSRA_MASK          (1 << 7)
#define PCICR_MASK           (1 << 2)
#define PCIFR_MASK           (1 << 2)

static volatile uint8_t* const SMCR_addr   = (volatile uint8_t*)0x53;
static volatile uint8_t* const ADCSRA_addr = (volatile uint8_t*)0x7A;
static volatile uint8_t* const PCICR_addr  = (volatile uint8_t*)0x68;
static volatile uint8_t* const PCMSK2_addr = (volatile uint8_t*)0x6D;
static volatile uint8_t* const PCIFR_addr  = (volatile uint8_t*)0x3B;
static volatile uint8_t* const PIND_addr   = (volatile uint8_t*)0x29;

static void rowsAsGround() {
  digitalWrite(4, LOW);
  pinMode(4, OUTPUT);
}

static void rowsAsWake() {
  pinMode(4, INPUT);
}

void initSleep() {
  rowsAsGround();
}

void enterSleep() {
  Serial.flush();
  rowsAsWake();

  for (volatile int i = 0; i < 50; i++) {}

  *ADCSRA_addr &= ~ADCSRA_MASK;
  *PCMSK2_addr |= ROW_MASK;
  *PCICR_addr |= PCICR_MASK;
  *PCIFR_addr = PCIFR_MASK;

  if (!(*PIND_addr & ROW_MASK)) {
    *SMCR_addr = 0x04 | 0x01; // Power-down mode + Sleep Enable

    uint8_t t;
    asm volatile(
      "in %0, 0x35 \n\t"
      "ori %0, 0x60 \n\t"
      "out 0x35, %0 \n\t"
      "andi %0, 0xDF \n\t"
      "out 0x35, %0 \n\t"
      "sleep \n\t"
      : "=&d"(t));

    *SMCR_addr &= ~0x01;
  }

  *PCMSK2_addr &= ~ROW_MASK;
  *PCIFR_addr = PCIFR_MASK;
  rowsAsGround();
  *ADCSRA_addr |= ADCSRA_MASK;
}

ISR(PCINT2_vect) {
  // Pin Change Interrupt to wake CPU
}

// ==========================================
// LED + BUZZER (User Interface)
// ==========================================

static const int LED_YELLOW = 5;   // D5, pin 11
static const int LED_GREEN  = 6;   // D6, pin 12
static const int LED_RED    = 7;   // D7, pin 13
static const int BUZZER     = 8;   // D8, pin 14

void uiInit() {
  pinMode(LED_YELLOW, OUTPUT);
  pinMode(LED_GREEN,  OUTPUT);
  pinMode(LED_RED,    OUTPUT);
  pinMode(BUZZER,     OUTPUT);
  digitalWrite(LED_YELLOW, LOW);
  digitalWrite(LED_GREEN,  LOW);
  digitalWrite(LED_RED,    LOW);
  digitalWrite(BUZZER,     LOW);
}

void showStateLEDs() {
  digitalWrite(LED_RED, LOW);
  if (state == UNLOCKED) {
    digitalWrite(LED_GREEN,  HIGH);
    digitalWrite(LED_YELLOW, LOW);
  } else {                      // SETUP or LOCKED = waiting for a code
    digitalWrite(LED_GREEN,  LOW);
    digitalWrite(LED_YELLOW, HIGH);
  }
}

void beepCorrect() {            // correct PIN, high beep
  tone(BUZZER, 2000, 150);
}

static void beepWrong() {       // wrong PIN, womp-womp
  tone(BUZZER, 800, 180);
  delay(200);
  tone(BUZZER, 500, 260);
  delay(280);
  noTone(BUZZER);
  digitalWrite(BUZZER, LOW);
}

void showWrong() {
  digitalWrite(LED_YELLOW, LOW);
  digitalWrite(LED_GREEN,  LOW);
  digitalWrite(LED_RED,    HIGH);
  beepWrong();
  digitalWrite(LED_RED, LOW);
  showStateLEDs();
}

void uiOff() {
  digitalWrite(LED_YELLOW, LOW);
  digitalWrite(LED_GREEN,  LOW);
  digitalWrite(LED_RED,    LOW);
  noTone(BUZZER);
  digitalWrite(BUZZER, LOW);
}

// ==========================================
// BATTERY INDICATOR
// ==========================================

static const int BATTERY_PIN = A2;   // divider read node
static const int BATTERY_SW  = 13;   // D13
static const int LOW_ADC = 802;
static const int RECOVER = 822;

static bool lowFlag = false;
static bool measuring = false;
static unsigned long measuringStart = 0;

void batteryInit() {
  pinMode(BATTERY_SW, OUTPUT);
  digitalWrite(BATTERY_SW, LOW);       // divider off at rest
}

void batteryStart() {
  digitalWrite(BATTERY_SW, HIGH);      // divider ON
  measuringStart = millis();
  measuring = true;
}

static void batteryAlert() {           // distinct low-battery signal
  for (int i = 0; i < 2; i++) {
    digitalWrite(LED_RED, HIGH);
    tone(BUZZER, 400, 120);
    delay(150);
    digitalWrite(LED_RED, LOW);
    delay(120);
  }
  noTone(BUZZER);
  digitalWrite(BUZZER, LOW);
  showStateLEDs();                     // restore normal LED
}

void batteryUpdate() {
  if (!measuring) return;
  if (millis() - measuringStart < 10) return;   // let the divider settle

  long sum = 0;
  for (int i = 0; i < 8; i++) sum += analogRead(BATTERY_PIN);
  digitalWrite(BATTERY_SW, LOW);       // divider OFF (save power)
  measuring = false;

  int r = sum / 8;
  if (!lowFlag && r < LOW_ADC) lowFlag = true;    // hysteresis
  if (lowFlag && r > RECOVER)  lowFlag = false;
  if (lowFlag) batteryAlert();
}

bool batteryLow() { return lowFlag; }
