/*
  pulse.ino  -  Maternity Care System (Health-IoT wearable)
  ---------------------------------------------------------
  Reads the analog Pulse Sensor on A0, calculates heart rate (BPM),
  sends it to the serial monitor AND to a phone over the HC-05 Bluetooth
  module, and raises alerts (coin vibration motor + 12V relay) when the
  BPM goes outside the safe range.

  Board : Arduino Uno
  Pins  : Pulse sensor signal -> A0
          HC-05 TX -> D2 (Arduino RX),  HC-05 RX <- D3 (Arduino TX)
          Vibration motor (via NPN transistor, 1k on base) -> D6
          Relay module IN -> D8

  No external library is needed: BPM detection is done in this file.
*/

#include <SoftwareSerial.h>

// ---------------- Pins ----------------
const uint8_t PULSE_PIN = A0;
const uint8_t BT_RX_PIN = 2;   // Arduino RX  <- HC-05 TX
const uint8_t BT_TX_PIN = 3;   // Arduino TX  -> HC-05 RX (use a voltage divider, HC-05 RX is 3.3V)
const uint8_t VIB_PIN   = 6;   // transistor base (through 1k resistor)
const uint8_t RELAY_PIN = 8;

// Most cheap relay modules switch ON when the pin is LOW. If yours clicks
// the wrong way round, change this to false.
const bool RELAY_ACTIVE_LOW = true;

// ---------------- Safe range & timing ----------------
const int BPM_HIGH = 100;                       // above this -> alert
const int BPM_LOW  = 40;                        // below this -> alert (optional lower limit)
const unsigned long ALERT_INTERVAL_MS = 5000;   // cooldown: max one alert every 5 s
const unsigned long VIBRATION_MS      = 500;    // how long the motor buzzes per alert

// ---------------- Beat detection settings ----------------
const unsigned long SAMPLE_INTERVAL_MS    = 2;     // read sensor at ~500 Hz
const unsigned long MIN_BEAT_GAP_MS       = 300;   // ignore beats closer than this (>200 BPM)
const unsigned long MAX_BEAT_GAP_MS       = 1500;  // gaps longer than this (<40 BPM) = resync
const unsigned long NO_FINGER_TIMEOUT_MS  = 3000;  // no beat for 3 s -> "No reading"
const int           MIN_AMPLITUDE         = 40;    // weaker signal than this = no finger
const uint8_t       BEATS_TO_AVERAGE      = 4;     // smooth BPM over the last 4 beats

SoftwareSerial bt(BT_RX_PIN, BT_TX_PIN);

int  signalVal = 512, peakVal = 512, troughVal = 512;
bool aboveThreshold = false;
unsigned long lastSample = 0, lastBeat = 0, lastDecay = 0, lastAlert = 0, vibUntil = 0;
unsigned long ibiBuf[BEATS_TO_AVERAGE];
uint8_t ibiCount = 0, ibiIdx = 0;
int bpm = 0;

// ---------------- Print to Serial Monitor AND Bluetooth ----------------
void out(const __FlashStringHelper* s)   { Serial.print(s);   bt.print(s); }
void outln(const __FlashStringHelper* s) { Serial.println(s); bt.println(s); }
void out(int v)                          { Serial.print(v);   bt.print(v); }
void outln(int v)                        { Serial.println(v); bt.println(v); }

// ---------------- Outputs ----------------
void setRelay(bool on) {
  digitalWrite(RELAY_PIN, (on != RELAY_ACTIVE_LOW) ? HIGH : LOW);
}

void serviceVibration(unsigned long now) {
  if (vibUntil != 0 && now >= vibUntil) {
    digitalWrite(VIB_PIN, LOW);
    vibUntil = 0;
  }
}

void checkAlerts(unsigned long now) {
  bool high = (bpm > BPM_HIGH);
  bool low  = (bpm > 0 && bpm < BPM_LOW);

  setRelay(high);   // relay follows the critical (high BPM) condition

  // Cooldown logic: only alert once every ALERT_INTERVAL_MS, not continuously
  if ((high || low) && (now - lastAlert >= ALERT_INTERVAL_MS)) {
    lastAlert = now;
    digitalWrite(VIB_PIN, HIGH);
    vibUntil = now + VIBRATION_MS;
    outln(high ? F("ALERT: High heart rate - VIBRATOR ON")
               : F("ALERT: Low heart rate - VIBRATOR ON"));
  }
}

// ---------------- Beat detection ----------------
void onBeat(unsigned long now) {
  if (lastBeat != 0) {
    unsigned long ibi = now - lastBeat;          // inter-beat interval
    if (ibi < MIN_BEAT_GAP_MS) return;           // too soon: noise
    if (ibi <= MAX_BEAT_GAP_MS) {
      ibiBuf[ibiIdx] = ibi;
      ibiIdx = (ibiIdx + 1) % BEATS_TO_AVERAGE;
      if (ibiCount < BEATS_TO_AVERAGE) ibiCount++;

      if (ibiCount >= 3) {                       // wait for a few beats before reporting
        unsigned long sum = 0;
        for (uint8_t i = 0; i < ibiCount; i++) sum += ibiBuf[i];
        bpm = (int)(60000UL / (sum / ibiCount));
        outln(F("A HeartBeat Happened!"));
        out(F("BPM: "));
        outln(bpm);
        checkAlerts(now);
      }
    } else {
      ibiCount = 0; ibiIdx = 0;                  // gap too long: start averaging again
    }
  }
  lastBeat = now;
}

void readPulse(unsigned long now) {
  int raw = analogRead(PULSE_PIN);
  signalVal = (signalVal * 3 + raw) / 4;         // light smoothing filter

  if (signalVal > peakVal)   peakVal   = signalVal;
  if (signalVal < troughVal) troughVal = signalVal;

  // Let peak/trough slowly close in so the threshold adapts to the signal
  if (now - lastDecay >= 50) {
    lastDecay = now;
    if (peakVal > troughVal + 2) { peakVal--; troughVal++; }
  }

  int amp    = peakVal - troughVal;
  int thresh = troughVal + amp / 2;

  if (amp < MIN_AMPLITUDE) { aboveThreshold = false; return; }   // no usable signal

  int margin = amp / 10;                                          // hysteresis
  if (!aboveThreshold && signalVal > thresh + margin) {
    aboveThreshold = true;                                        // rising edge = heartbeat
    onBeat(now);
  } else if (aboveThreshold && signalVal < thresh - margin) {
    aboveThreshold = false;
  }
}

// ---------------- Arduino entry points ----------------
void setup() {
  pinMode(VIB_PIN, OUTPUT);
  pinMode(RELAY_PIN, OUTPUT);
  digitalWrite(VIB_PIN, LOW);
  setRelay(false);

  Serial.begin(9600);
  bt.begin(9600);                                // HC-05 default data baud rate
  outln(F("Maternity Care System - Pulse Monitor"));
  outln(F("Place finger on the pulse sensor..."));
}

void loop() {
  unsigned long now = millis();

  if (now - lastSample >= SAMPLE_INTERVAL_MS) {
    lastSample = now;
    readPulse(now);
  }

  // No beat for a while -> finger removed
  if (bpm > 0 && (now - lastBeat > NO_FINGER_TIMEOUT_MS)) {
    bpm = 0;
    ibiCount = 0; ibiIdx = 0;
    setRelay(false);
    outln(F("No reading - place finger on sensor"));
  }

  serviceVibration(now);
}
