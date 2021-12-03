#include <Arduino.h>
#include <AltSoftSerial.h>
#include <InputDebounce.h>

/*
  Rotary phone number counter using interrupts
  More info to come later
*/

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

//-------------
String phoneNumber = "";
int pulses = 0;
boolean phonePickedUp = false;
boolean isDialing = false;
boolean isOnCall = false;
boolean isCalling = false;
unsigned long millisSinceLastNumStop = 0;
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
      millisSinceLastNumStop = 0;
    } else {
      isDialing = true;
    }
    phoneNumber += (pulses == 10) ? 0 : pulses;
    Serial.println("Phone number: " + phoneNumber);
    pulses = 0;
    millisSinceLastNumStop = millis();
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

void pickupPhone() {
  if(!phonePickedUp) {
    Serial.println("Pickup phone");
  }
  phonePickedUp = true;
  if (isCalling) {
    answerCall();
  }
}

void hungupPhone() {
  if (phonePickedUp) {
    Serial.println("Hungup phone");
  }
  phonePickedUp = false;
  isDialing = false;
  isCalling = false;
  if (isOnCall) {
    gsm.println("ATH");
    isOnCall = false;
    flushSerialBuffer();
  }
  millisSinceLastNumStop = 0;
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
  if(phoneNumber.length() != 10) {
    callNumber();
  } else {
    Serial.println("Number cleared");
  }
  phoneNumber = "";
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

  digitalWrite(PHONE_PICKED_UP_PIN, HIGH);

  dialEnable.registerCallbacks(NULL, endDialPulseCount);
  dialPulse.registerCallbacks(pulseDial, NULL);
  buttonCall.registerCallbacks(call, NULL);

  dialEnable.setup(NUMBER_STOP_PIN, DIAL_DEBOUNCE_DELAY, InputDebounce::PIM_EXT_PULL_UP_RES, NULL, InputDebounce::ST_NORMALLY_CLOSED);
  dialPulse.setup(DIALPAD_PIN, DIAL_DEBOUNCE_DELAY, InputDebounce::PIM_EXT_PULL_UP_RES);
  buttonCall.setup(12, BUTTON_DEBOUNCE_DELAY, InputDebounce::PIM_EXT_PULL_UP_RES, NULL, InputDebounce::ST_NORMALLY_CLOSED);

  // attachInterrupt(digitalPinToInterrupt(12), buttonPushed, RISING);
  // attachInterrupt(digitalPinToInterrupt(DIALPAD_PIN), pulseCounter, FALLING);
  // attachInterrupt(digitalPinToInterrupt(NUMBER_STOP_PIN), numStop, FALLING); 
}

int tmp1 = 0;

void loop() {
  long now = millis();

  dialEnable.process(now);
  dialPulse.process(now);
  buttonCall.process(now);

  // int w = gsm.println("derp");
  // Serial.print("Written: ");
  // Serial.println(w);
  // delay(500);
  updateSerial();
  if(tmp1++ % 10 == 0) {
    int bs = digitalRead(12);
    String bss = "BS->" + String(bs);
    bss = bss + "<-BS";
    // Serial.println(bss);
  }
  delay(5);
  return;
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

  if (digitalRead(PHONE_PICKED_UP_PIN) == LOW) {
    pickupPhone();
  } else {
    hungupPhone();
  }

  if (phonePickedUp && isDialing && (millis() >= millisSinceLastNumStop+NUMBER_TIMEOUT_MS)) {
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

  // if (isOnCall && !isCallInProgress()) {
  //   Serial.println("Call not in progress");
  //   gsm.println("ATH");
  //   isOnCall = false;
  //   flushSerialBuffer();
  // }
}
