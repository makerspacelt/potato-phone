#include <Arduino.h>
/*
  Rotary phone number counter using interrupts
  More info to come later
*/

//--------
const int DIALPAD_PIN = 3;
const int NUMBER_STOP_PIN = 2;
const int PHONE_PICKED_UP_PIN = 4;
//--------
String phoneNumber = "";
int dialedNum = 0;
boolean phonePickedUp = false;
//--------

void pulseCounter() {
  dialedNum += 1;
}

void numStop() {
  phoneNumber += (dialedNum == 10) ? 0 : dialedNum;
  Serial.println(phoneNumber);
  dialedNum = 0;
}

void pickupPhone() {
  phonePickedUp = (digitalRead(PHONE_PICKED_UP_PIN) == LOW); // phone was picked up, switch is ON
}

void setup() {
  Serial.begin(9600);
  pinMode(DIALPAD_PIN, INPUT);
  pinMode(NUMBER_STOP_PIN, INPUT);
  pinMode(PHONE_PICKED_UP_PIN, INPUT);
  digitalWrite(PHONE_PICKED_UP_PIN, HIGH); // enable internal pullup
  attachInterrupt(digitalPinToInterrupt(DIALPAD_PIN), pulseCounter, FALLING);
  attachInterrupt(digitalPinToInterrupt(NUMBER_STOP_PIN), numStop, FALLING);
  attachInterrupt(digitalPinToInterrupt(PHONE_PICKED_UP_PIN), pickupPhone, CHANGE);
}

void loop() {
  
}
