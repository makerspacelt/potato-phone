#include <Arduino.h>
#include <AltSoftSerial.h>
#include <InputDebounce.h>
#include <TaskScheduler.h>

/*
  Rotary phone number counter using interrupts
  More info to come later
*/

const short RINGER_MOTOR_SPEED = 255;
#define RINGER_ENABLE_PIN A1

//-------------
const int DIALPAD_PIN = 3; // Yellow wire
const int NUMBER_STOP_PIN = 2; // Blue wire
const int PHONE_PICKED_UP_PIN = 4;
const int NUMBER_TIMEOUT_MS = 4000;
const int RING_TIMEOUT_MS = 6000;

const int DIAL_DEBOUNCE_DELAY = 20;
const int BUTTON_DEBOUNCE_DELAY = 10;
//-------------


static InputDebounce dialEnable;
static InputDebounce dialPulse;
static InputDebounce buttonCall;
static InputDebounce buttonPickup;

Scheduler ts;
void smackBell();
boolean enableBell();
void disableBell();
Task ringTask (100 * TASK_MILLISECOND, TASK_FOREVER, &smackBell, &ts, false, &enableBell, &disableBell);

//-------------
String phoneNumber = "";
int pulses = 0;
boolean phonePickedUp = false;
boolean isDialing = false;
boolean isOnCall = false;
boolean isCalling = false;
unsigned long millisAtLastNumStop = 0;
unsigned long millisSinceLastRing = 0;
AltSoftSerial gsm(9, 8);
//-------------

bool isCallInProgress() {
  gsm.println("AT+CPAS");
  delay(1000);
  while (gsm.available()) {
    String data = gsm.readString();
    Serial.println(data);
    if (data.indexOf("+CPAS: 4") != -1) {
      return true;
    }
  }
  return false;
}

bool isRinging() {
  gsm.println("AT+CPAS");
  delay(100);
  while (gsm.available()) {
    String data = gsm.readString();
    Serial.println(data);
    if (data.indexOf("+CPAS: 3") != -1) {
      return true;
    }
  }
  return false;
}

void flushSerialBuffer() {
  gsm.readString();
}

void pulseCounter() {
  Serial.println("Pulse detected " + String(millis()));
  if (phonePickedUp && !isOnCall) {
    pulses += 1;
  }
}

void endDialPulseCount(uint8_t pinIn) {
  // Serial.println("Num stop called");
  // if (phonePickedUp && !isOnCall) {
    if (isDialing) {
      millisAtLastNumStop = 0;
    } else {
      isDialing = true;
    }
    phoneNumber += (pulses == 10) ? 0 : pulses;
    Serial.println("Phone number: " + phoneNumber);
    pulses = 0;
    millisAtLastNumStop = millis();
  // }
}

void updateSerial() {
  while (Serial.available()) {
    gsm.write(Serial.read());
  }
  while (gsm.available()) {
    Serial.write(gsm.read());
  }
}

void answerCall() {
  if (!isOnCall) {
    gsm.println("ATA");
  }
  isOnCall = true;
  isCalling = false;
  millisSinceLastRing = 0;
}

void pickupPhone(uint8_t pinIn) {
  Serial.println("Pickup phone");

  phonePickedUp = true;
  if (isCalling) {
    answerCall();
  }
  phoneNumber = "";
  pulses = 0;
}

void hungupPhone(uint8_t pinIn) {
  Serial.println("Hungup phone");

  phonePickedUp = false;
  isDialing = false;
  isCalling = false;
  if (isOnCall) {
    gsm.println("ATH");
    isOnCall = false;
    flushSerialBuffer();
  }
  millisAtLastNumStop = 0;
  phoneNumber = "";
  pulses = 0;
}

void callNumber() {
  String telNumCommand = "ATD"+phoneNumber+";";
  // telNumCommand = "ATD867961606;";
  Serial.println("Calling: " + telNumCommand);
  gsm.println(telNumCommand);
  // String data = Serial.readString();
  // if (data.indexOf("OK") == -1) {
  //   Serial.println("ATH"); // something wrong, best to hung up here
  // }
}

void startDialPulseCount(uint8_t pinIn) {
  Serial.println("Start pulse count");
  pulses = 0;
}

void pulseDial(int8_t pinIn) {
  Serial.println("Pulse dial");
  pulses++;
}

void call(int8_t pinIn) {
  if(phoneNumber.length() >= 10) {
    callNumber();
  } else {
    Serial.println("Number cleared");
  }
  phoneNumber = "";
}

boolean smackSide = true;
void smackBell() {
  Serial.println("Smack");
    digitalWrite(RINGER_ENABLE_PIN, smackSide ? HIGH : LOW);
    smackSide = !smackSide;
}

boolean enableBell() {
  Serial.println("Enable bell");
  return true;
}

void disableBell() {
  Serial.println("Disable bell");
}

void setup() {
  Serial.begin(9600);
  gsm.begin(9600);
  // delay(500);
  // gsm.println("AT");
  // updateSerial();
  // flushSerialBuffer();
  // callNumber();

  pinMode(DIALPAD_PIN, INPUT);
  pinMode(NUMBER_STOP_PIN, INPUT);
  pinMode(PHONE_PICKED_UP_PIN, INPUT);
  pinMode(12, INPUT);

  pinMode(RINGER_ENABLE_PIN, OUTPUT);

  digitalWrite(RINGER_ENABLE_PIN, LOW);

  // digitalWrite(PHONE_PICKED_UP_PIN, HIGH);

  dialEnable.registerCallbacks(NULL, endDialPulseCount);
  dialPulse.registerCallbacks(pulseDial, NULL);
  buttonCall.registerCallbacks(call, NULL);
  buttonPickup.registerCallbacks(pickupPhone, hungupPhone, NULL, NULL);


  dialEnable.setup(NUMBER_STOP_PIN, DIAL_DEBOUNCE_DELAY, InputDebounce::PIM_EXT_PULL_UP_RES, NULL, InputDebounce::ST_NORMALLY_CLOSED);
  dialPulse.setup(DIALPAD_PIN, DIAL_DEBOUNCE_DELAY, InputDebounce::PIM_EXT_PULL_UP_RES);
  buttonCall.setup(12, BUTTON_DEBOUNCE_DELAY, InputDebounce::PIM_EXT_PULL_UP_RES, NULL, InputDebounce::ST_NORMALLY_CLOSED);
  buttonPickup.setup(PHONE_PICKED_UP_PIN, InputDebounce::PIM_EXT_PULL_UP_RES);

  ts.startNow();
}

void loop() {
  long now = millis();

  dialEnable.process(now);
  dialPulse.process(now);
  buttonCall.process(now);
  buttonPickup.process(now);

  updateSerial();
  delay(5);
  // return;
  if (gsm.available()) {
    String data = gsm.readString();
    Serial.println(data);
    if (data.indexOf("RING") != -1) { // incomming call
      if (phonePickedUp) {
        Serial.println("Phone already picked up");
        Serial.println("ATH"); // decline call if phone is picked up
        flushSerialBuffer();
      } else {
        Serial.println("Incomming call...");
        isCalling = true;
        millisSinceLastRing = millis();
      }
    } else if (data.indexOf("NO CARRIER") != -1) { // call ended
      Serial.println("Call ended");
      isCalling = false;
      millisSinceLastRing = 0;
    } else if (data.indexOf("BUSY") != -1) { // call declined
      Serial.println("Call declined");
      isCalling = false;
      millisSinceLastRing = 0;
    }
  }

  if (phonePickedUp && isDialing && (millis() >= millisAtLastNumStop + NUMBER_TIMEOUT_MS)) {
    isDialing = false;
    isOnCall = true;
    callNumber();
  }

  // this check is needed in case we missed reading when other party hung up when we didn't answer
  if (isCalling && !isRinging()) {
    Serial.println("not calling");
    gsm.println("ATH");
    isCalling = false;
    flushSerialBuffer();
  }

  if(ringTask.isEnabled() != isCalling) {
    if(isCalling) {
      ringTask.enable();
    } else {
      ringTask.disable();
    }
  }

  ts.execute();

  // if (isOnCall && !isCallInProgress()) {
  //   Serial.println("Call not in progress");
  //   gsm.println("ATH");
  //   isOnCall = false;
  //   flushSerialBuffer();
  // }
}
