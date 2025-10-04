/*
  RFID‑BASED PARKING SYSTEM  (Arduino Uno + RC522 + I²C LCD + SG90 + LEDs + Buzzer)
  ────────────────────────────────────────────────────────────────────────────────
  • Idle  : red LED on, buzzer beeps 150 ms every 500 ms, gate closed,
            “PLEASE SCAN UID TO PASS”.
  • Scan  : “PLEASE WAIT – AUTHENTICATING…”.
  • Allowed → two short beeps, green LED, gate opens 5 s, “ACCESS GRANTED”.
  • Denied  → one long beep, red LED, gate stays shut, “ACCESS DENIED”.
*/

// ─── Libraries ────────────────────────────────────────────────────────────────
#include <SPI.h>
#include <MFRC522.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <Servo.h>

// Alias .begin() → .init() so you can write init() everywhere
#define init(...) begin(__VA_ARGS__)

// ─── Pin definitions ─────────────────────────────────────────────────────────
#define RST_PIN    9
#define SS_PIN     10
#define GREEN_LED  3
#define RED_LED    4
#define BUZZER     5
#define SERVO_PIN  6

// ─── Objects ────────────────────────────────────────────────────────────────
MFRC522 rfid(SS_PIN, RST_PIN);
LiquidCrystal_I2C lcd(0x27, 16, 2);
Servo gateServo;

// Whitelist of authorised UIDs
const char *allowedUIDs[][5] = {
  {"43","95","BB","F7"}
};

// ─── Idle‑buzzer timing constants ───────────────────────────────────────────
const unsigned long IDLE_BEEP_ON_MS  = 150;  // tone length
const unsigned long IDLE_BEEP_OFF_MS = 350;  // silence between tones

// ─── Globals for idle buzzer state ──────────────────────────────────────────
unsigned long idleBeepTimestamp = 0;
bool         idleBuzzerState   = false;

// ─── Function prototypes ─────────────────────────────────────────────────────
void idleScreen();
void idleBeep();
bool isAllowed(const char *uid);
void grantAccess();
void denyAccess();

// ─── SETUP ───────────────────────────────────────────────────────────────────
void setup() {
  Serial.init(9600);
  SPI.init();
  rfid.PCD_Init();
  Wire.init();
  lcd.init(16, 2);                 // → lcd.begin(16, 2);
  lcd.backlight();

  pinMode(GREEN_LED, OUTPUT);
  pinMode(RED_LED,   OUTPUT);
  pinMode(BUZZER,    OUTPUT);

  gateServo.attach(SERVO_PIN);
  gateServo.write(0);              // gate closed

  idleScreen();
  Serial.println(F("System ready – waiting for card"));
}

// ─── MAIN LOOP ───────────────────────────────────────────────────────────────
void loop() {

  // No new card? keep idling and pulsing buzzer
  if (!rfid.PICC_IsNewCardPresent() || !rfid.PICC_ReadCardSerial()) {
    idleBeep();
    return;
  }

  // Card detected → stop idle beep, show auth message
  digitalWrite(BUZZER, LOW);
  idleBuzzerState = false;                 // reset state
  lcd.clear();
  lcd.print("PLEASE WAIT");
  lcd.setCursor(0,1);
  lcd.print("AUTHENTICATING");

  // Convert UID to printable string
  char uidStr[15] = "";
  for (byte i = 0; i < rfid.uid.size; i++) {
    char hexByte[4];
    sprintf(hexByte, "%02X", rfid.uid.uidByte[i]);
    strcat(uidStr, hexByte);
    if (i < rfid.uid.size - 1) strcat(uidStr, " ");
  }
  Serial.print(F("Scanned UID: "));
  Serial.println(uidStr);

  // Decide
  if (isAllowed(uidStr))  grantAccess();
  else                    denyAccess();

  // Cleanup & back to idle
  rfid.PICC_HaltA();
  rfid.PCD_StopCrypto1();
  idleScreen();
}

// ─── Idle state (LCD + LEDs + start beep cycle) ─────────────────────────────
void idleScreen() {
  lcd.clear();
  lcd.print("PLEASE SCAN UID");
  lcd.setCursor(0,1);
  lcd.print("TO PASS");

  digitalWrite(RED_LED, HIGH);
  digitalWrite(GREEN_LED, LOW);
  gateServo.write(0);              // gate closed

  digitalWrite(BUZZER, LOW);       // ensure silent before first pulse
  idleBuzzerState   = false;
  idleBeepTimestamp = millis();
}

// ─── Pulsing the buzzer while idling (non‑blocking) ─────────────────────────
void idleBeep() {
  unsigned long now = millis();

  if (idleBuzzerState) {  // currently ON?
    if (now - idleBeepTimestamp >= IDLE_BEEP_ON_MS) {
      digitalWrite(BUZZER, LOW);        // turn it off
      idleBuzzerState   = false;
      idleBeepTimestamp = now;
    }
  } else {                // currently OFF
    if (now - idleBeepTimestamp >= IDLE_BEEP_OFF_MS) {
      digitalWrite(BUZZER, HIGH);       // turn it on
      idleBuzzerState   = true;
      idleBeepTimestamp = now;
    }
  }
}

// ─── Check UID against whitelist ─────────────────────────────────────────────
bool isAllowed(const char *uid) {
  const size_t nCards = sizeof(allowedUIDs) / sizeof(allowedUIDs[0]);
  for (size_t i = 0; i < nCards; i++) {
    char concat[15] = "";
    for (byte b = 0; b < 4; b++) {
      strcat(concat, allowedUIDs[i][b]);
      if (b < 3) strcat(concat, " ");
    }
    if (!strcmp(uid, concat)) return true;
  }
  return false;
}

// ─── Access granted ─────────────────────────────────────────────────────────
void grantAccess() {
  Serial.println(F("Access GRANTED"));

  // Two short beeps
  for (byte i = 0; i < 2; i++) {
    digitalWrite(BUZZER, HIGH); delay(150);
    digitalWrite(BUZZER, LOW);  delay(150);
  }

  digitalWrite(RED_LED, LOW);
  digitalWrite(GREEN_LED, HIGH);
  lcd.clear();
  lcd.print("ACCESS GRANTED");

  gateServo.write(90);             // open gate
  delay(5000);
  gateServo.write(0);              // close gate
  digitalWrite(GREEN_LED, LOW);
}

// ─── Access denied ─────────────────────────────────────────────────────────
void denyAccess() {
  Serial.println(F("Access DENIED"));

  digitalWrite(BUZZER, HIGH); delay(1000);
  digitalWrite(BUZZER, LOW);

  digitalWrite(RED_LED, HIGH);
  digitalWrite(GREEN_LED, LOW);
  lcd.clear();
  lcd.print("ACCESS DENIED");
  delay(2000);
}
