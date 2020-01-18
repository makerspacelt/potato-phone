#include <Arduino.h>
/*
  Rotary phone number counter using interrupts
  More info to come later
*/

//-------------
const int DIALPAD_PIN = 3;
const int NUMBER_STOP_PIN = 2;
const int PHONE_PICKED_UP_PIN = 3;
//-------------
String phoneNumber = "";
int dialedNum = 0;
boolean phonePickedUp = false;
//--PROTOTYPES--
void attachDialpadInterrupts();
void detachDialpadInterrupts();
//-------------

void pulseCounter() {
  dialedNum += 1;
}

void numStop() {
  phoneNumber += (dialedNum == 10) ? 0 : dialedNum;
  Serial.println(phoneNumber);
  dialedNum = 0;
}

void pickupPhone() {
  Serial.println("pickedup");
  phoneNumber = "";
  dialedNum = 0;
  phonePickedUp ? attachDialpadInterrupts() : detachDialpadInterrupts();
}

void attachDialpadInterrupts() {
  Serial.println("attached");
  attachInterrupt(digitalPinToInterrupt(DIALPAD_PIN), pulseCounter, FALLING);
  attachInterrupt(digitalPinToInterrupt(NUMBER_STOP_PIN), numStop, FALLING);
}

void detachDialpadInterrupts() {
  Serial.println("detached");
  detachInterrupt(digitalPinToInterrupt(DIALPAD_PIN));
  detachInterrupt(digitalPinToInterrupt(NUMBER_STOP_PIN));
}

void setup() {
  Serial.begin(9600);
  pinMode(DIALPAD_PIN, INPUT);
  pinMode(NUMBER_STOP_PIN, INPUT);
  pinMode(PHONE_PICKED_UP_PIN, INPUT);
  digitalWrite(PHONE_PICKED_UP_PIN, HIGH); // enable internal pullup
}

void loop() {
  if (digitalRead(PHONE_PICKED_UP_PIN) == LOW) {
    Serial.println("picked up");
  }
}
