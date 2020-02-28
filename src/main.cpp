#include <Arduino.h>
#include <SoftwareSerial.h>

/*
  Rotary phone number counter using interrupts
  More info to come later
*/

//-------------
const int DIALPAD_PIN = 3;
const int NUMBER_STOP_PIN = 2;
const int PHONE_PICKED_UP_PIN = 4;
const int NUMBER_TIMEOUT_MS = 4000;
const int RING_TIMEOUT_MS = 6000;
//-------------
String phoneNumber = "";
int pulses = 0;
boolean phonePickedUp = false;
boolean isDialing = false;
boolean isOnCall = false;
boolean isCalling = false;
unsigned long millisSinceLastNumStop = 0;
unsigned long millisSinceLastRing = 0;
SoftwareSerial gsm(10, 11);
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
  if (phonePickedUp && !isOnCall) {
    pulses += 1;
  }
}

void numStop() {
  if (phonePickedUp && !isOnCall) {
    if (isDialing) {
      millisSinceLastNumStop = 0;
    } else {
      isDialing = true;
    }
    phoneNumber += (pulses == 10) ? 0 : pulses;
    Serial.println(phoneNumber);
    pulses = 0;
    millisSinceLastNumStop = millis();
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
  phonePickedUp = true;
  if (isCalling) {
    answerCall();
  }
}

void hungupPhone() {
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
  // telNumCommand = "ATD86;";
  gsm.println(telNumCommand);
  String data = Serial.readString();
  if (data.indexOf("OK") == -1) {
    Serial.println("ATH"); // something wrong, best to hung up here
  }
}

void setup() {
  Serial.begin(9600);
  gsm.begin(9600);
  gsm.println("AT");
  delay(1000);
  flushSerialBuffer();
  // callNumber();

  pinMode(DIALPAD_PIN, INPUT);
  pinMode(NUMBER_STOP_PIN, INPUT);
  pinMode(PHONE_PICKED_UP_PIN, INPUT);
  digitalWrite(PHONE_PICKED_UP_PIN, HIGH);
  attachInterrupt(digitalPinToInterrupt(DIALPAD_PIN), pulseCounter, FALLING);
  attachInterrupt(digitalPinToInterrupt(NUMBER_STOP_PIN), numStop, FALLING);
}

void loop() {
  if (gsm.available()) {
    String data = gsm.readString();
    Serial.println(data);
    if (data.indexOf("RING") != -1) { // incomming call
      // if (phonePickedUp) {
      //   Serial.println("Phone already picked up");
      //   Serial.println("ATH"); // decline call if phone is picked up
      //   flushSerialBuffer();
      // } else {
        Serial.println("Incomming call...");
        isCalling = true;
        millisSinceLastRing = millis();
      // }
    } else if (data.indexOf("NO CARRIER") != -1) { // call ended
      Serial.println("Call ended");
      // isCalling = false;
      // millisSinceLastRing = 0;
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
