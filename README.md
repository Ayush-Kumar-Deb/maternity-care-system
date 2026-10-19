# Maternity Care System: Wearable Health-IoT Monitor

A low-cost wearable prototype that monitors a pregnant woman's **heart rate** and **blood oxygen (SpO2)** and alerts her with a **vibration** when readings go out of the safe range. Live data is also sent over **Bluetooth** to a phone.

Built as a first-year Innovation & Design Thinking (IDT) project, aligned with **UN SDG 3: Good Health and Well-being**.

> **Disclaimer:** this is a student prototype, not a certified medical device.

## Why this project?
Many pregnant women in rural or low-resource areas can't get regular monitoring. Using the Design Thinking process (interviews with gynecologists, nurses, mothers and families), we defined the problem as: *there is no affordable, easy-to-use wearable for real-time maternal health tracking at home.*

## How it works
1. Sensors (MAX30102 + analog pulse sensor) measure heart rate and SpO2.
2. The Arduino Uno compares readings against safe thresholds.
3. If a reading is abnormal, the **coin vibration motor** buzzes and a **relay** can trigger an external alarm.
4. The **HC-05** module streams readings to an Android Bluetooth terminal app.

## Prototype and results
![Prototype](images/prototype.jpg)
### Block diagram
![Block diagram](images/block-diagram.png)
### Circuit diagram
![Circuit diagram](images/circuit-diagram.png)
### Serial monitor output
![Pulse reading](images/pulse-reading.png)
![SpO2 reading](images/spo2-reading.png)

## Hardware
- Arduino Uno
- MAX30102 pulse oximeter sensor (I2C)
- Analog pulse sensor
- HC-05 Bluetooth module
- Coin vibration motor + NPN transistor (BC547 / 2N2222) + 1 kOhm resistor
- 1-channel 12V relay module
- Breadboard, jumper wires, 9V battery / USB power

## Pin connections
| Component | Pin | Arduino Uno |
|---|---|---|
| MAX30102 | VIN / GND | 5V / GND |
| MAX30102 | SDA / SCL | A4 / A5 |
| Pulse sensor | Signal | A0 |
| HC-05 | TX | D2 (Arduino RX) |
| HC-05 | RX | D3 (Arduino TX, via voltage divider) |
| Vibration motor | via NPN transistor, base through 1 kOhm | D6 |
| Relay module | IN | D8 |

## Code
| Folder | What it does |
|---|---|
| [`pulse/`](pulse/pulse.ino) | Heart rate (BPM) from the analog pulse sensor, with Bluetooth output and alerts |
| [`spoooo2/`](spoooo2/spoooo2.ino) | Heart rate + SpO2 from the MAX30102, with Bluetooth output and alerts |

### Setup
1. Install the [Arduino IDE](https://www.arduino.cc/en/software).
2. For `spoooo2`, install **SparkFun MAX3010x Pulse and Proximity Sensor Library** from the Library Manager.
3. Open a sketch, select **Board: Arduino Uno** and your port, then click Upload.
4. Open the Serial Monitor at **9600 baud**, or pair the HC-05 (default PIN `1234`) with a Bluetooth terminal app on your phone.

### Alert thresholds
| Condition | Action |
|---|---|
| Heart rate > 100 BPM | Vibration + relay |
| Heart rate < 40 BPM | Vibration |
| SpO2 < 95% | Vibration |
| SpO2 < 90% | Vibration + relay |

Alerts have a 5-second cooldown so the motor doesn't buzz continuously.

## Test results
| Test | Result |
|---|---|
| Normal BPM (70) | No alert, data on phone |
| Elevated BPM (103) | Vibration + relay triggered |
| Bluetooth transmission | Live data, no loss |
| 9V battery power | Works without USB |

**Note:** These results are from the original prototype. The code in this repository was rewritten after the original source files were lost.

## Future scope
- Body temperature sensor
- Dedicated Android app with reading history
- SMS / app alerts to caregivers
- Low-power mode and a waterproof wearable casing

## Team
*(Ayush Kumar Deb, Sara Johns, CS Rohith, Siri KA)*
