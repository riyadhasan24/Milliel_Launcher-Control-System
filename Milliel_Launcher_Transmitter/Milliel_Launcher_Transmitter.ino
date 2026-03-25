/* The source Code from : https://github.com/riyadhasan24
 * By Md. Riyad Hasan
 */

#include <esp_now.h>
#include <WiFi.h>

#if __has_include(<esp_arduino_version.h>)
  #include <esp_arduino_version.h>
#endif

#ifndef ESP_ARDUINO_VERSION_MAJOR
  #define ESP_ARDUINO_VERSION_MAJOR 2
#endif

const int redLedPin    = 12;
const int greenLedPin  = 14;
const int yellowLedPin = 13;

const int basePotPin   = 39;
const int tiltPotPin   = 36;

const int modeBtnPin   = 32;   // press & hold = AUTO <-> MANUAL
const int actionBtnPin = 33;   // works only in MANUAL

uint8_t receiverMAC[] = {0xF4, 0x2D, 0xC9, 0x6C, 0x26, 0xEC};

const unsigned long sendIntervalMs       = 30;
const unsigned long modeLongPressMs      = 1200;
const unsigned long statusTimeoutMs      = 1200;

const int potRawMin = 0;
const int potRawMax = 4095;

const int baseAngleMin = 0;
const int baseAngleMax = 180;
const int tiltAngleMin = 0;
const int tiltAngleMax = 180;

const bool invertBasePot = false;
const bool invertTiltPot = false;

enum DisplayMode : uint8_t 
{
  MODE_AUTO   = 0,
  MODE_MANUAL = 1,
  MODE_SETUP  = 2
};

// DATA STRUCTURES
typedef struct 
{
  uint8_t mode;         // MODE_AUTO or MODE_MANUAL
  int16_t baseAngle;
  int16_t tiltAngle;
  bool shootRequest;    // one-shot pulse
} TxPacket;

typedef struct 
{
  uint8_t displayMode;  // MODE_AUTO / MODE_MANUAL / MODE_SETUP
  bool relayActive;
  uint16_t distanceCm;
} RxStatusPacket;

TxPacket txData;
volatile RxStatusPacket rxStatus;

uint8_t currentMode = MODE_AUTO;
uint8_t displayMode = MODE_AUTO;

unsigned long lastSendMs = 0;
unsigned long lastStatusMs = 0;

bool modeBtnPrev = HIGH;
bool modeLongPressHandled = false;
unsigned long modePressStartMs = 0;

bool actionBtnPrev = HIGH;
bool actionHandledUntilRelease = false;

bool shootPulsePending = false;

// HELPER FUNCTIONS
int mapPotToAngle(int raw, int outMin, int outMax, bool invertDirection) 
{
  raw = constrain(raw, potRawMin, potRawMax);
  int angle;
  if (!invertDirection) 
  {
    angle = map(raw, potRawMin, potRawMax, outMin, outMax);
  } 
  else 
  {
    angle = map(raw, potRawMin, potRawMax, outMax, outMin);
  }
  return constrain(angle, outMin, outMax);
}

void updateLeds() 
{
  digitalWrite(redLedPin, LOW);
  digitalWrite(greenLedPin, LOW);
  digitalWrite(yellowLedPin, LOW);

  if (displayMode == MODE_SETUP) 
  {
    digitalWrite(redLedPin, HIGH);
  } 
  else if (displayMode == MODE_MANUAL) 
  {
    digitalWrite(yellowLedPin, HIGH);
  } 
  else 
  {
    digitalWrite(greenLedPin, HIGH);
  }
}

void handleModeButton() 
{
  bool current = digitalRead(modeBtnPin);

  if (modeBtnPrev == HIGH && current == LOW) 
  {
    modePressStartMs = millis();
    modeLongPressHandled = false;
  }

  if (current == LOW && !modeLongPressHandled) 
  {
    if (millis() - modePressStartMs >= modeLongPressMs) 
    {
      currentMode = (currentMode == MODE_AUTO) ? MODE_MANUAL : MODE_AUTO;
      modeLongPressHandled = true;
    }
  }

  if (modeBtnPrev == LOW && current == HIGH) 
  {
    modePressStartMs = 0;
    modeLongPressHandled = false;
  }

  modeBtnPrev = current;
}

void handleActionButton() 
{
  bool current = digitalRead(actionBtnPin);

  if (currentMode == MODE_MANUAL) 
  {
    if (current == LOW && actionBtnPrev == HIGH && !actionHandledUntilRelease) 
    {
      shootPulsePending = true;               // one packet pulse
      actionHandledUntilRelease = true;       // block repeat while held
    }
    if (current == HIGH) 
    {
      actionHandledUntilRelease = false;
    }
  } 
  else 
  {
    shootPulsePending = false;
    actionHandledUntilRelease = false;
  }

  actionBtnPrev = current;
}

void prepareTxPacket() 
{
  int rawBase = analogRead(basePotPin);
  int rawTilt = analogRead(tiltPotPin);

  txData.mode = currentMode;
  txData.baseAngle = mapPotToAngle(rawBase, baseAngleMin, baseAngleMax, invertBasePot);
  txData.tiltAngle = mapPotToAngle(rawTilt, tiltAngleMin, tiltAngleMax, invertTiltPot);
  txData.shootRequest = shootPulsePending;
}

void sendPacketIfNeeded() 
{
  if (millis() - lastSendMs >= sendIntervalMs) 
  {
    lastSendMs = millis();
    prepareTxPacket();
    esp_now_send(receiverMAC, (uint8_t*)&txData, sizeof(txData));
    shootPulsePending = false;   // one-shot only
  }
}

void updateDisplayModeFromReceiver() 
{
  if (millis() - lastStatusMs <= statusTimeoutMs) 
  {
    displayMode = rxStatus.displayMode;
  } 
  else 
  {
    displayMode = currentMode;
  }
}


#if ESP_ARDUINO_VERSION_MAJOR >= 3
void onDataRecv(const esp_now_recv_info_t *info, const uint8_t *incomingData, int len) 

#else
void onDataRecv(const uint8_t *mac, const uint8_t *incomingData, int len) 
{
#endif
  if (len == sizeof(RxStatusPacket)) 
  {
    memcpy((void*)&rxStatus, incomingData, sizeof(RxStatusPacket));
    lastStatusMs = millis();
  }
}

void onDataSent(const uint8_t *mac_addr, esp_now_send_status_t status) 
{
  // optional debug
}

// =========================
// SETUP
// =========================
void setup() 
{
  Serial.begin(115200);

  pinMode(redLedPin, OUTPUT);
  pinMode(greenLedPin, OUTPUT);
  pinMode(yellowLedPin, OUTPUT);

  pinMode(modeBtnPin, INPUT_PULLUP);
  pinMode(actionBtnPin, INPUT_PULLUP);

  digitalWrite(redLedPin, LOW);
  digitalWrite(greenLedPin, LOW);
  digitalWrite(yellowLedPin, LOW);

  WiFi.mode(WIFI_STA);
  WiFi.disconnect();

  if (esp_now_init() != ESP_OK) 
  {
    Serial.println("ESP-NOW init failed");
    return;
  }

  esp_now_register_send_cb(onDataSent);
  esp_now_register_recv_cb(onDataRecv);

  esp_now_peer_info_t peerInfo = {};
  memcpy(peerInfo.peer_addr, receiverMAC, 6);
  peerInfo.channel = 0;
  peerInfo.encrypt = false;

  if (esp_now_add_peer(&peerInfo) != ESP_OK) 
  {
    Serial.println("Failed to add receiver peer");
    return;
  }

  displayMode = currentMode;
  updateLeds();

  Serial.println("Transmitter ready");
}

void loop() 
{
  handleModeButton();
  handleActionButton();
  sendPacketIfNeeded();
  updateDisplayModeFromReceiver();
  updateLeds();
}
