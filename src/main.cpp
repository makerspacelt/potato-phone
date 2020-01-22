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
//-------------
String phoneNumber = "";
int pulses = 0;
boolean phonePickedUp = false;
boolean isDialing = false;
boolean isOnCall = false;
long millisSinceLastNumStop = 0;
SoftwareSerial mySerial(7, 8);
//-------------

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

void pickupPhone() {
  phonePickedUp = true;
}

void hungupPhone() {
  phonePickedUp = false;
  isDialing = false;
  isOnCall = false;
  millisSinceLastNumStop = 0;
  phoneNumber = "";
  pulses = 0;
}

void updateSerial()
{
  delay(500);
  while (Serial.available()) 
  {
    mySerial.write(Serial.read());//Forward what Serial received to Software Serial Port
  }
  while(mySerial.available()) 
  {
    Serial.write(mySerial.read());//Forward what Software Serial received to Serial Port
  }
}

void setup() {
  Serial.begin(9600);

  mySerial.begin(9600);
  delay(1000);
  updateSerial();
  mySerial.println("AT"); //Handshaking with SIM900
  updateSerial();
  mySerial.println("ATD+ 860546993;");
  updateSerial();
  delay(10000);
  mySerial.println("ATH");
  updateSerial();

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
  if (phonePickedUp && isDialing && (millis() >= millisSinceLastNumStop+NUMBER_TIMEOUT_MS)) {
    isDialing = false;
    isOnCall = true;
    Serial.println("CALLING...");
  }
}
