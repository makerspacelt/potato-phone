#include <Arduino.h>
/*
  Rotary phone number counter using interrupts
  More info to come later
*/

//-------------
const int DIALPAD_PIN = 3;
const int NUMBER_STOP_PIN = 2;
const int PHONE_PICKED_UP_PIN = 4;
//-------------
String phoneNumber = "";
int pulses = 0;
boolean phonePickedUp = false;
//-------------

void pulseCounter() {
  if (phonePickedUp) {
    pulses += 1;
  }
}

void numStop() {
  if (phonePickedUp) {
    phoneNumber += (pulses == 10) ? 0 : pulses;
    Serial.println(phoneNumber);
    pulses = 0;
  }
}

void pickupPhone() {
  phonePickedUp = true;
}

void hungupPhone() {
  phonePickedUp = false;
  phoneNumber = "";
  pulses = 0;
}

void setup() {
  Serial.begin(9600);
  pinMode(DIALPAD_PIN, INPUT);
  pinMode(NUMBER_STOP_PIN, INPUT);
  pinMode(PHONE_PICKED_UP_PIN, INPUT);
  digitalWrite(PHONE_PICKED_UP_PIN, HIGH);
  attachInterrupt(digitalPinToInterrupt(DIALPAD_PIN), pulseCounter, FALLING);
  attachInterrupt(digitalPinToInterrupt(NUMBER_STOP_PIN), numStop, FALLING );
}

void loop() {
  if (digitalRead(PHONE_PICKED_UP_PIN) == LOW) {
    pickupPhone();
  } else {
    hungupPhone();
  }
}
