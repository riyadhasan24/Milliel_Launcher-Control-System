/* The source Code from : https://github.com/riyadhasan24
 * By Md. Riyad Hasan
 */
 
#include <esp_now.h>
#include <WiFi.h>
#include <ESP32Servo.h>

#if __has_include(<esp_arduino_version.h>)
  #include <esp_arduino_version.h>
#endif

#ifndef ESP_ARDUINO_VERSION_MAJOR
  #define ESP_ARDUINO_VERSION_MAJOR 2
#endif


const int trigPin      = 22;
const int echoPin      = 23;

const int redLedPin    = 12;
const int greenLedPin  = 14;
const int yellowLedPin = 13;

const int baseServoPin = 33;
const int tiltServoPin = 25;
const int relayPin     = 26;

const int setupBtnPin  = 32;

uint8_t transmitterMAC[] = {0xB0, 0xCB, 0xD8, 0xCC, 0xE8, 0x4C};

const unsigned long setupLongPressMs      = 1200;
const unsigned long statusSendIntervalMs  = 100;

const int baseStartAngle        = 0;
const int tiltStartAngle        = 50;
const int setupBaseAngle        = 0;
const int setupTiltAngle        = 175;
const int autoNeutralTiltAngle  = 50;
const int autoShootTiltAngle    = 180;

const int baseServoMinUs = 500;
const int baseServoMaxUs = 2400;
const int tiltServoMinUs = 500;
const int tiltServoMaxUs = 2400;

const unsigned long servoUpdateIntervalMs = 20;
const int baseServoStep                   = 1;
const int tiltServoStep                   = 1;

const int scanMinAngle                 = 0;
const int scanMaxAngle                 = 180;
const int scanStepAngle                = 1;
const unsigned long scanStepIntervalMs = 35;

const int targetDistanceCm                = 20; 
const unsigned long distanceReadIntervalMs = 70;
const unsigned long echoTimeoutUs          = 25000;


const unsigned long relayOnTimeMs   = 5000;
const unsigned long postShotDelayMs = 7000;

const int relayOnLevel  = HIGH; 
const int relayOffLevel = LOW;

enum DisplayMode : uint8_t 
{
  MODE_AUTO   = 0,
  MODE_MANUAL = 1,
  MODE_SETUP  = 2
};

enum ShotState : uint8_t 
{
  SHOT_IDLE = 0,
  SHOT_MOVE_IN,
  SHOT_RELAY_ON,
  SHOT_HOLD_AFTER,
  SHOT_MOVE_OUT
};


// DATA STRUCTURES
typedef struct 
{
  uint8_t mode;  
  int16_t baseAngle;
  int16_t tiltAngle;
  bool shootRequest;
} TxPacket;

typedef struct 
{
  uint8_t displayMode;  // MODE_AUTO / MODE_MANUAL / MODE_SETUP
  bool relayActive;
  uint16_t distanceCm;
} RxStatusPacket;

volatile TxPacket rxCmd;
RxStatusPacket statusPacket;

Servo baseServo;
Servo tiltServo;

uint8_t controlMode = MODE_AUTO;
bool setupMode = false;

int currentBaseAngle = baseStartAngle;
int currentTiltAngle = tiltStartAngle;

int targetBaseAngle = baseStartAngle;
int targetTiltAngle = tiltStartAngle;

int manualBaseAngle = baseStartAngle;
int manualTiltAngle = tiltStartAngle;

int scanTargetAngle = scanMinAngle;
int scanDirection = 1;
int autoShotHoldBaseAngle = baseStartAngle;

unsigned long lastServoUpdateMs = 0;
unsigned long lastScanStepMs = 0;
unsigned long lastDistanceReadMs = 0;
unsigned long shotStateStartMs = 0;
unsigned long lastStatusSendMs = 0;

bool relayActive = false;
ShotState shotState = SHOT_IDLE;
bool shotFromAuto = false;
uint16_t lastDistanceCm = 999;

bool setupBtnPrev = HIGH;
bool setupLongPressHandled = false;
unsigned long setupPressStartMs = 0;

void setRelay(bool on) 
{
  relayActive = on;
  digitalWrite(relayPin, on ? relayOnLevel : relayOffLevel);
}

void updateLeds() 
{
  digitalWrite(redLedPin, LOW);
  digitalWrite(greenLedPin, LOW);
  digitalWrite(yellowLedPin, LOW);

  if (setupMode) 
  {
    digitalWrite(redLedPin, HIGH);
  } 
  else if (controlMode == MODE_MANUAL) 
  {
    digitalWrite(yellowLedPin, HIGH);
  } 
  else 
  {
    digitalWrite(greenLedPin, HIGH);
  }
}

long readDistanceCm() 
{
  digitalWrite(trigPin, LOW);
  delayMicroseconds(2);
  digitalWrite(trigPin, HIGH);
  delayMicroseconds(10);
  digitalWrite(trigPin, LOW);

  long duration = pulseIn(echoPin, HIGH, echoTimeoutUs);
  if (duration <= 0) return 999;

  long cm = (duration * 0.0343) / 2.0;
  if (cm <= 0) return 999;
  return cm;
}

void moveServosSlowly() 
{
  if (millis() - lastServoUpdateMs < servoUpdateIntervalMs) return;
  lastServoUpdateMs = millis();

  if (currentBaseAngle < targetBaseAngle) 
  {
    currentBaseAngle += baseServoStep;
    if (currentBaseAngle > targetBaseAngle) currentBaseAngle = targetBaseAngle;
  } 
  else if (currentBaseAngle > targetBaseAngle) 
  {
    currentBaseAngle -= baseServoStep;
    if (currentBaseAngle < targetBaseAngle) currentBaseAngle = targetBaseAngle;
  }

  if (currentTiltAngle < targetTiltAngle) 
  {
    currentTiltAngle += tiltServoStep;
    if (currentTiltAngle > targetTiltAngle) currentTiltAngle = targetTiltAngle;
  } 
  else if (currentTiltAngle > targetTiltAngle) 
  {
    currentTiltAngle -= tiltServoStep;
    if (currentTiltAngle < targetTiltAngle) currentTiltAngle = targetTiltAngle;
  }

  baseServo.write(currentBaseAngle);
  tiltServo.write(currentTiltAngle);
}

void cancelShotAndForceSafeState() 
{
  shotState = SHOT_IDLE;
  shotFromAuto = false;
  setRelay(false);
}

void startAutoShot() 
{
  if (shotState != SHOT_IDLE) return;

  shotFromAuto = true;
  shotState = SHOT_MOVE_IN;
  autoShotHoldBaseAngle = currentBaseAngle;
  targetBaseAngle = autoShotHoldBaseAngle;
  targetTiltAngle = autoShootTiltAngle;
}

void startManualShot() 
{
  if (shotState != SHOT_IDLE) return;

  shotFromAuto = false;
  shotState = SHOT_RELAY_ON;
  shotStateStartMs = millis();
  setRelay(true);
}

void updateShotStateMachine() 
{
  if (setupMode) 
  {
    cancelShotAndForceSafeState();
    return;
  }

  switch (shotState) 
  {
    case SHOT_IDLE:
      break;

    case SHOT_MOVE_IN:
      if (currentTiltAngle == autoShootTiltAngle) 
      {
        setRelay(true);
        shotState = SHOT_RELAY_ON;
        shotStateStartMs = millis();
      }
      break;

    case SHOT_RELAY_ON:
      if (millis() - shotStateStartMs >= relayOnTimeMs) 
      {
        setRelay(false);
        shotState = SHOT_HOLD_AFTER;
        shotStateStartMs = millis();
      }
      break;

    case SHOT_HOLD_AFTER:
      if (millis() - shotStateStartMs >= postShotDelayMs) 
      {
        if (shotFromAuto) 
        {
          shotState = SHOT_MOVE_OUT;
          targetTiltAngle = autoNeutralTiltAngle;
        } 
        else 
        {
          shotState = SHOT_IDLE;
        }
      }
      break;

    case SHOT_MOVE_OUT:
      if (currentTiltAngle == autoNeutralTiltAngle) 
      {
        shotState = SHOT_IDLE;
      }
      break;
  }
}

void handleSetupButton() 
{
  bool current = digitalRead(setupBtnPin);

  if (setupBtnPrev == HIGH && current == LOW) 
  {
    setupPressStartMs = millis();
    setupLongPressHandled = false;
  }

  if (current == LOW && !setupLongPressHandled) 
  {
    if (millis() - setupPressStartMs >= setupLongPressMs) 
    {
      setupMode = !setupMode;
      setupLongPressHandled = true;

      if (setupMode) 
      {
        cancelShotAndForceSafeState();
      }
    }
  }

  if (setupBtnPrev == LOW && current == HIGH) 
  {
    setupPressStartMs = 0;
    setupLongPressHandled = false;
  }

  setupBtnPrev = current;
}

void updateControlTargets() 
{
  if (setupMode) 
  {
    targetBaseAngle = setupBaseAngle;
    targetTiltAngle = setupTiltAngle;
    return;
  }

  if (controlMode == MODE_MANUAL) 
  {
    targetBaseAngle = manualBaseAngle;
    targetTiltAngle = manualTiltAngle;
    return;
  }

  // AUTO MODE
  if (shotFromAuto && shotState != SHOT_IDLE) 
  {
    targetBaseAngle = autoShotHoldBaseAngle;

    if (shotState == SHOT_MOVE_IN || shotState == SHOT_RELAY_ON || shotState == SHOT_HOLD_AFTER) 
    {
      targetTiltAngle = autoShootTiltAngle;
    } 
    else if (shotState == SHOT_MOVE_OUT) 
    {
      targetTiltAngle = autoNeutralTiltAngle;
    }
    return;
  }

  targetTiltAngle = autoNeutralTiltAngle;

  if (millis() - lastScanStepMs >= scanStepIntervalMs) 
  {
    lastScanStepMs = millis();
    scanTargetAngle += scanDirection * scanStepAngle;

    if (scanTargetAngle >= scanMaxAngle) 
    {
      scanTargetAngle = scanMaxAngle;
      scanDirection = -1;
    } 
    else if (scanTargetAngle <= scanMinAngle) 
    {
      scanTargetAngle = scanMinAngle;
      scanDirection = 1;
    }
  }

  targetBaseAngle = scanTargetAngle;
}

void handleAutoDetection() 
{
  if (setupMode) return;
  if (controlMode != MODE_AUTO) return;
  if (shotState != SHOT_IDLE) return;

  if (millis() - lastDistanceReadMs < distanceReadIntervalMs) return;
  lastDistanceReadMs = millis();

  lastDistanceCm = readDistanceCm();

  if (lastDistanceCm > 0 && lastDistanceCm <= targetDistanceCm) 
  {
    startAutoShot();
  }
}

void handleManualShootRequest() 
{
  if (setupMode) return;
  if (controlMode != MODE_MANUAL) return;
  if (shotState != SHOT_IDLE) return;

  if (rxCmd.shootRequest) 
  {
    startManualShot();
    rxCmd.shootRequest = false;
  }
}

void sendStatusToTransmitter() 
{
  if (millis() - lastStatusSendMs < statusSendIntervalMs) return;
  lastStatusSendMs = millis();

  statusPacket.displayMode = setupMode ? MODE_SETUP : controlMode;
  statusPacket.relayActive = relayActive;
  statusPacket.distanceCm = lastDistanceCm;

  esp_now_send(transmitterMAC, (uint8_t*)&statusPacket, sizeof(statusPacket));
}

#if ESP_ARDUINO_VERSION_MAJOR >= 3
void onDataRecv(const esp_now_recv_info_t *info, const uint8_t *incomingData, int len) 
{
#else
void onDataRecv(const uint8_t *mac, const uint8_t *incomingData, int len) 
{
#endif
  if (len == sizeof(TxPacket)) 
  {
    memcpy((void*)&rxCmd, incomingData, sizeof(TxPacket));

    controlMode = rxCmd.mode;
    manualBaseAngle = constrain(rxCmd.baseAngle, 0, 180);
    manualTiltAngle = constrain(rxCmd.tiltAngle, 0, 180);
  }
}

void onDataSent(const uint8_t *mac_addr, esp_now_send_status_t status) 
{
  // optional debug
}


void setup() 
{
  Serial.begin(115200);

  pinMode(trigPin, OUTPUT);
  pinMode(echoPin, INPUT);

  pinMode(redLedPin, OUTPUT);
  pinMode(greenLedPin, OUTPUT);
  pinMode(yellowLedPin, OUTPUT);

  pinMode(relayPin, OUTPUT);
  pinMode(setupBtnPin, INPUT_PULLUP);

  setRelay(false);

  baseServo.setPeriodHertz(50);
  tiltServo.setPeriodHertz(50);

  baseServo.attach(baseServoPin, baseServoMinUs, baseServoMaxUs);
  tiltServo.attach(tiltServoPin, tiltServoMinUs, tiltServoMaxUs);

  baseServo.write(currentBaseAngle);
  tiltServo.write(currentTiltAngle);

  WiFi.mode(WIFI_STA);
  WiFi.disconnect();

  if (esp_now_init() != ESP_OK) 
  {
    Serial.println("ESP-NOW init failed");
    return;
  }

  esp_now_register_recv_cb(onDataRecv);
  esp_now_register_send_cb(onDataSent);

  esp_now_peer_info_t peerInfo = {};
  memcpy(peerInfo.peer_addr, transmitterMAC, 6);
  peerInfo.channel = 0;
  peerInfo.encrypt = false;

  if (esp_now_add_peer(&peerInfo) != ESP_OK) 
  {
    Serial.println("Failed to add transmitter peer");
    return;
  }

  updateLeds();
  Serial.println("Receiver ready");
}


void loop() 
{
  handleSetupButton();

  if (setupMode) 
  {
    cancelShotAndForceSafeState();
  }

  handleManualShootRequest();
  handleAutoDetection();
  updateShotStateMachine();
  updateControlTargets();
  moveServosSlowly();
  updateLeds();
  sendStatusToTransmitter();
}
