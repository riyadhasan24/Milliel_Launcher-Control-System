# Milliel_Launcher-Control-System

# ESP32 Auto / Manual Turret Control System

A dual-ESP32 control system with **Auto Mode**, **Manual Mode**, and **Setup Mode**.  
The receiver ESP32 handles ultrasonic detection, servos, relay, and setup control.  
The transmitter ESP32 handles mode switching, manual aiming, and manual shooting.

## Hardware

### Receiver ESP32
- Ultrasonic Sensor
- Base Rotation Servo
- Tilt Rotation Servo
- Relay Module
- Setup Button
- Red / Green / Yellow LEDs

### Transmitter ESP32
- 2 Potentiometers
- Mode Button
- Action Button
- Red / Green / Yellow LEDs

## Pin Configuration

### Receiver ESP32
- **MAC:** `F4:2D:C9:6C:26:EC`
- **Trig Pin:** `22`
- **Echo Pin:** `23`
- **Red LED:** `12`
- **Green LED:** `14`
- **Yellow LED:** `13`
- **Base Rotation Servo:** `33`
- **Tilt Rotation Servo:** `25`
- **Relay Pin:** `26`
- **Setup Button:** `32`

### Transmitter ESP32
- **MAC:** `B0:CB:D8:CC:E8:4C`
- **Red LED:** `12`
- **Green LED:** `14`
- **Yellow LED:** `13`
- **Base Rotation Pot:** `39`
- **Tilt Rotation Pot:** `36`
- **Mode Button:** `32`
- **Action Button:** `33`

## Modes

### Auto Mode
- Receiver scans automatically
- If target distance is below the set limit, the system shoots automatically
- Relay turns ON for **5 seconds**
- After shooting, system waits **7 seconds**
- **Green LED stays ON**

### Manual Mode
- Two potentiometers control:
  - Base servo
  - Tilt servo
- Action button triggers shooting
- Relay turns ON for **5 seconds only**
- Even if the Action button is held, relay will not stay ON more than **5 seconds**
- **Yellow LED stays ON**

### Setup Mode
- Enter by pressing and holding the **receiver setup button**
- Servos move to fixed setup angles:
  - **Setup Base Angle = 0**
  - **Setup Tilt Angle = 175**
- Relay stays OFF
- **Red LED stays ON**
- Press and hold the receiver setup button again to return to normal operation

## Control Logic

### Receiver Button
- **Press & hold** = Toggle **Setup Mode ON / OFF**

### Transmitter Mode Button
- **Press & hold** = Toggle **Auto Mode / Manual Mode**

### Transmitter Action Button
- Works **only in Manual Mode**
- One press = one shooting cycle
- Holding the button does not increase relay ON time

## Features

- Dual ESP32 communication using **ESP-NOW**
- Auto scan and automatic shooting
- Manual aiming using two potentiometers
- Setup mode for fixed-angle adjustment
- Separate LEDs for mode indication
- Relay safety timing
- All important values can be adjusted easily from variables
- Servo movement designed to be **slow and smooth**

## Adjustable Variables

Examples of values that can be changed easily in code:
- Setup base angle
- Setup tilt angle
- Neutral tilt angle
- Shoot tilt angle
- Target distance
- Relay ON time
- Post-shot delay
- Servo step size
- Servo update interval
- Scan speed
- Long-press time

## How It Works

### Auto Mode
1. Receiver scans from side to side
2. Ultrasonic sensor checks distance
3. If target is detected within the configured range, tilt servo moves to shoot position
4. Relay turns ON for 5 seconds
5. System waits 7 seconds
6. Tilt returns to neutral position
7. Scanning continues

### Manual Mode
1. Use base potentiometer to control base rotation
2. Use tilt potentiometer to control tilt rotation
3. Press Action button to shoot
4. Relay turns ON for 5 seconds only
5. Yellow LED indicates Manual Mode

### Setup Mode
1. Press and hold receiver setup button
2. Base servo moves to 0°
3. Tilt servo moves to 175°
4. Red LED turns ON
5. Relay remains OFF
6. Press and hold again to return

## LED Status

### Receiver
- **Green LED ON** = Auto Mode
- **Yellow LED ON** = Manual Mode
- **Red LED ON** = Setup Mode

### Transmitter
- **Green LED ON** = Auto Mode
- **Yellow LED ON** = Manual Mode
- **Red LED** can be used for status or future expansion

## Notes
- Use a proper external power supply for servos and relay if needed
- Share **common GND** between ESP32, servo supply, and relay supply
- If servo movement is unstable, reduce speed and increase power stability
- If relay module is active LOW, update the relay logic in code
- ESP-NOW requires correct MAC address pairing between transmitter and receiver

## Libraries Needed
- `WiFi.h`
- `esp_now.h`
- `ESP32Servo.h`

## Project Status
- Auto Mode: Done
- Setup Mode: Done
- Manual Mode: In progress / integrated depending on code version
- README: Ready

## Future Improvements
- Pot deadband for smoother manual control
- OLED or LCD status display
- Battery monitoring
- Failsafe communication timeout
- Wireless setup parameter tuning

## License
This project is open-source. You can add your preferred license here.

## Contact
For any questions or support:
- **WhatsApp:** +8801730288553  
- **Email:** riyadhasan24a@gmail.com  
- **GitHub:** github.com/riyadhasan24
