/*
  Rotary phone number counter using interrupts
  More info to come later
*/

const int dialpadPin = 2;
const int numberStopPin = 3;
String phoneNumber = "";
int num = 0;

void pulseCounter() {
  num += 1;
}

void endStop() {
  phoneNumber += num == 10 ? 0 : num;
  Serial.println(phoneNumber);
  num = 0;
}

void setup() {
  Serial.begin(9600);
  pinMode(dialpadPin, INPUT);
  pinMode(numberStopPin, INPUT);
  attachInterrupt(digitalPinToInterrupt(dialpadPin), pulseCounter, FALLING);
  attachInterrupt(digitalPinToInterrupt(numberStopPin), endStop, FALLING);
}

void loop() {
  // skip
}
