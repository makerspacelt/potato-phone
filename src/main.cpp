#include <Arduino.h>
/*
  Rotary phone number counter using interrupts
  More info to come later
*/

//-------------
const int DIALPAD_PIN = 3;
const int NUMBER_STOP_PIN = 2;
const int PHONE_PICKED_UP_PIN = 4;
const int NUMBER_TIMEOUT_MS = 4000;
//-------------
String phoneNumber = "";
int pulses = 0;
boolean phonePickedUp = false;
boolean isDialing = false;
boolean isOnCall = false;
boolean isCalling = false;
unsigned long millisSinceLastNumStop = 0;
//-------------

void flushSerialBuffer() {
  Serial.readString(); // discard any data
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
    Serial.println("ATA");
    // TODO: read status and react
  }
  isOnCall = true;
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
  if (isOnCall) {
    Serial.println("ATH");
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
  Serial.println(telNumCommand);
  // TODO: read status and react
}

void setup() {
  Serial.begin(9600);
  Serial.println("AT");
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
  if (Serial.available()) {
    String data = Serial.readString();
    if (data.indexOf("RING") != -1) { // incomming call
      if (phonePickedUp) {
        Serial.println("ATH"); // decline call if phone is picked up
        flushSerialBuffer();
      } else {
        Serial.println("Incomming call...");
        isCalling = true;
      }
    } else if (data.indexOf("NO CARRIER") != -1) { // call ended
      Serial.println("Call ended");
      isCalling = false;
    } else if (data.indexOf("BUSY") != -1) { // call declined
      Serial.println("Call declined");
      isCalling = false;
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
}
