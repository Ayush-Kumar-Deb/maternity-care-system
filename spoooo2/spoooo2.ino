/*
  spoooo2.ino  -  Maternity Care System (Health-IoT wearable)
  -----------------------------------------------------------
  Reads heart rate and blood-oxygen saturation (SpO2) from the MAX30102
  sensor over I2C, sends the values to the serial monitor and to a phone
  over the HC-05 Bluetooth module, and raises alerts (coin vibration motor
  + 12V relay) when readings are abnormal.

  Board   : Arduino Uno
  Library : "SparkFun MAX3010x Pulse and Proximity Sensor Library"
            (Arduino IDE -> Sketch -> Include Library -> Manage Libraries)
  Pins    : MAX30102  SDA -> A4, SCL -> A5, VIN -> 5V, GND -> GND
            HC-05 TX -> D2 (Arduino RX),  HC-05 RX <- D3 (Arduino TX)
            Vibration motor (via NPN transistor, 1k on base) -> D6
            Relay module IN -> D8

  NOTE: SpO2 from this sensor is a prototype-grade estimate.
        This is NOT a certified medical device.
*/

#include <Wire.h>
#include <SoftwareSerial.h>
#include "MAX30105.h"          // the MAX30102 uses the MAX30105 driver
#include "spo2_algorithm.h"

// ---------------- Pins ----------------
const uint8_t BT_RX_PIN = 2;   // Arduino RX  <- HC-05 TX
const uint8_t BT_TX_PIN = 3;   // Arduino TX  -> HC-05 RX (use a voltage divider, HC-05 RX is 3.3V)
const uint8_t VIB_PIN   = 6;
const uint8_t RELAY_PIN = 8;

// Most cheap relay modules switch ON when the pin is LOW. If yours clicks
// the wrong way round, change this to false.
const bool RELAY_ACTIVE_LOW = true;

// ---------------- Safe range & timing ----------------
const int HR_HIGH       = 100;   // BPM above this  -> alert + relay
const int HR_LOW        = 40;    // BPM below this  -> alert
const int SPO2_LOW      = 95;    // SpO2 below this -> vibration alert
const int SPO2_CRITICAL = 90;    // SpO2 below this -> relay as well
const unsigned long ALERT_INTERVAL_MS = 5000;   // cooldown between alerts
const unsigned long VIBRATION_MS      = 500;
const uint32_t FINGER_IR_MIN          = 20000;  // IR value below this = no finger (tune if needed)

MAX30105 particleSensor;
SoftwareSerial bt(BT_RX_PIN, BT_TX_PIN);

#define BUFFER_LEN 100
#if defined(__AVR_ATmega328P__) || defined(__AVR_ATmega168__)
  uint16_t irBuffer[BUFFER_LEN];    // Uno has only 2 KB RAM, so 16-bit buffers
  uint16_t redBuffer[BUFFER_LEN];
#else
  uint32_t irBuffer[BUFFER_LEN];
  uint32_t redBuffer[BUFFER_LEN];
#endif

int32_t spo2;
int8_t  validSPO2;
int32_t heartRate;
int8_t  validHeartRate;

unsigned long lastAlert = 0, vibUntil = 0;

// ---------------- Print to Serial Monitor AND Bluetooth ----------------
void out(const __FlashStringHelper* s)   { Serial.print(s);   bt.print(s); }
void outln(const __FlashStringHelper* s) { Serial.println(s); bt.println(s); }
void out(int v)                          { Serial.print(v);   bt.print(v); }
void outln(int v)                        { Serial.println(v); bt.println(v); }

// ---------------- Outputs ----------------
void setRelay(bool on) {
  digitalWrite(RELAY_PIN, (on != RELAY_ACTIVE_LOW) ? HIGH : LOW);
}

void serviceVibration() {
  if (vibUntil != 0 && millis() >= vibUntil) {
    digitalWrite(VIB_PIN, LOW);
    vibUntil = 0;
  }
}

void checkAlerts(int bpm, int spo2Val) {
  bool hrHigh  = (bpm > HR_HIGH);
  bool hrLow   = (bpm < HR_LOW);
  bool spo2Low = (spo2Val < SPO2_LOW);
  bool critical = hrHigh || (spo2Val < SPO2_CRITICAL);

  setRelay(critical);

  unsigned long now = millis();
  if ((hrHigh || hrLow || spo2Low) && (now - lastAlert >= ALERT_INTERVAL_MS)) {
    lastAlert = now;
    digitalWrite(VIB_PIN, HIGH);
    vibUntil = now + VIBRATION_MS;
    if (hrHigh)       outln(F("ALERT: High heart rate - VIBRATOR ON"));
    else if (hrLow)   outln(F("ALERT: Low heart rate - VIBRATOR ON"));
    else              outln(F("ALERT: Low SpO2 - VIBRATOR ON"));
  }
}

// ---------------- Sampling ----------------
void readSample(uint8_t i) {
  while (!particleSensor.available()) {
    particleSensor.check();
    serviceVibration();                         // keep the motor timing accurate while waiting
  }
  redBuffer[i] = particleSensor.getRed();
  irBuffer[i]  = particleSensor.getIR();
  particleSensor.nextSample();
}

void handleReading() {
  bool finger = (irBuffer[BUFFER_LEN - 1] > FINGER_IR_MIN);
  if (!finger) {
    setRelay(false);
    outln(F("No reading - place finger on sensor"));
    return;
  }

  if (validHeartRate && validSPO2 &&
      heartRate > 20 && heartRate < 255 && spo2 > 50 && spo2 <= 100) {
    out(F("HR: "));   out((int)heartRate);
    out(F(" BPM | SpO2: ")); out((int)spo2);
    outln(F(" %"));
    checkAlerts((int)heartRate, (int)spo2);
  } else {
    outln(F("Measuring... keep finger still"));
  }
}

// ---------------- Arduino entry points ----------------
void setup() {
  pinMode(VIB_PIN, OUTPUT);
  pinMode(RELAY_PIN, OUTPUT);
  digitalWrite(VIB_PIN, LOW);
  setRelay(false);

  Serial.begin(9600);
  bt.begin(9600);                               // HC-05 default data baud rate
  outln(F("Maternity Care System - SpO2 & Heart Rate Monitor"));

  if (!particleSensor.begin(Wire, I2C_SPEED_FAST)) {
    outln(F("MAX30102 not found. Check wiring (SDA->A4, SCL->A5, 5V, GND)."));
    while (1);
  }

  // Sensor settings (SparkFun's recommended values for SpO2)
  byte ledBrightness = 60;    // 0-255
  byte sampleAverage = 4;
  byte ledMode       = 2;     // Red + IR
  int  sampleRate    = 100;
  int  pulseWidth    = 411;
  int  adcRange      = 4096;
  particleSensor.setup(ledBrightness, sampleAverage, ledMode, sampleRate, pulseWidth, adcRange);

  outln(F("Place finger on the sensor..."));

  // Fill the first 100 samples
  for (uint8_t i = 0; i < BUFFER_LEN; i++) readSample(i);
  maxim_heart_rate_and_oxygen_saturation(irBuffer, BUFFER_LEN, redBuffer,
                                         &spo2, &validSPO2, &heartRate, &validHeartRate);
}

void loop() {
  // Keep the newest 75 samples, drop the oldest 25
  for (uint8_t i = 25; i < BUFFER_LEN; i++) {
    redBuffer[i - 25] = redBuffer[i];
    irBuffer[i - 25]  = irBuffer[i];
  }
  // Take 25 new samples (about 1 second of data)
  for (uint8_t i = 75; i < BUFFER_LEN; i++) readSample(i);

  maxim_heart_rate_and_oxygen_saturation(irBuffer, BUFFER_LEN, redBuffer,
                                         &spo2, &validSPO2, &heartRate, &validHeartRate);
  handleReading();
  serviceVibration();
}
