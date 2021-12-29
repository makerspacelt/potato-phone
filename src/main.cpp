#include <Arduino.h>
#include <AltSoftSerial.h>
#include <InputDebounce.h>
#include <TaskScheduler.h>

/*
  Rotary phone number counter using interrupts
  More info to come later
*/

const short RINGER_MOTOR_SPEED = 255;
#define RINGER_ENABLE 11 //default: A0
#define RINGER_CURRENT_SENSE A7  //default: A2
#define RINGER_FWD 6 //default: 7
#define RINGER_REV 7 //default: 8
#define RINGER_PWM 5 //default 5


//-------------
const int DIALPAD_PIN = 3; // Yellow wire
const int NUMBER_STOP_PIN = 2; // Blue wire
const int PHONE_PICKED_UP_PIN = 4;
const int NUMBER_TIMEOUT_MS = 4000;
const int RING_TIMEOUT_MS = 6000;

const int DIAL_DEBOUNCE_DELAY = 20;
const int BUTTON_DEBOUNCE_DELAY = 10;

const int GSM_UPDATE_RATE_DT = 250;
const int STATUS_UPDATE_TIMEOUT = 500;
//-------------

enum PAS {
  READY,  UNAVAILABLE, UNKNOWN,  RINGING,  CALL_IN_PROGRESS,  ASLEEP
};

static InputDebounce dialEnable;
static InputDebounce dialPulse;
static InputDebounce buttonCall;
static InputDebounce buttonPickup;

Scheduler ts;
void smackBell();
boolean enableBell();
void disableBell();
Task ringTask(30 * TASK_MILLISECOND, TASK_FOREVER, &smackBell, &ts, false, &enableBell, &disableBell);

void updateGsmState();
Task updateGsmStageTask(GSM_UPDATE_RATE_DT * TASK_MILLISECOND, TASK_FOREVER, &updateGsmState, &ts);

//-------------
String phoneNumber = "";
int pulses = 0;

boolean phonePickedUp = false;
boolean isDialing = false;
boolean isOnCall = false;

PAS phoneStatus = READY;

unsigned long millisAtLastNumStop = 0;

AltSoftSerial gsm(9, 8);
//-------------

void queryPhoneActivity() {
  gsm.println("AT+CPAS");
}

void flushSerialBuffer() {
  // gsm.readString();
}

void pulseCounter() {
  Serial.println("Pulse detected " + String(millis()));
  if (phonePickedUp && phoneStatus == READY) {
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
  // while (gsm.available()) {
  //   Serial.write(gsm.read());
  // }
}

void answerCall() {
  if (phoneStatus == RINGING) {
    gsm.println("ATA");
  }
  // isOnCall = true;
}

void pickupPhone(uint8_t pinIn) {
  Serial.println("Pickup phone");

  phonePickedUp = true;
  if (phoneStatus == RINGING) {
    answerCall();
  }
  phoneNumber = "";
  pulses = 0;
}

void hungupPhone(uint8_t pinIn) {
  Serial.println("Hungup phone");

  phonePickedUp = false;
  isDialing = false;
  if (phoneStatus == CALL_IN_PROGRESS) {
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
  // Serial.println("Smack");
  smackSide = !smackSide;
    if (smackSide) {
      digitalWrite(RINGER_FWD, HIGH);
      digitalWrite(RINGER_REV, LOW);
    } else {
      digitalWrite(RINGER_FWD, LOW);
      digitalWrite(RINGER_REV, HIGH);
    }
  analogWrite(RINGER_PWM, RINGER_MOTOR_SPEED); 
}

boolean enableBell() {
  Serial.println("Enable bell");
  digitalWrite(RINGER_ENABLE, HIGH);

  return true;
}

void disableBell() {
  digitalWrite(RINGER_ENABLE, LOW);
  Serial.println("Disable bell");
}

boolean waitingForStatusUpdate = false;

unsigned long lastStatusUpdateReceivedAt = 0;

void updateGsmState() {
  // Serial.println("update gsm state");

  boolean updatedFromGsm = false;
  if (gsm.available()) {
    String data = gsm.readString();
    // if(!(data.length() > 0)) {
      Serial.println(data + " " + millis());
    // }
    if (data.indexOf("RING") != -1) { 
      updatedFromGsm = true;// incomming call
      if (phonePickedUp) {
        Serial.println("Phone already picked up");
        // Serial.println("ATH"); // decline call if phone is picked up
        flushSerialBuffer();
      } else {
        Serial.println("Incomming call...");
      }
    }/* else if (data.indexOf("NO CARRIER") != -1) { // call ended
      Serial.println("Call ended");
    } else if (data.indexOf("BUSY") != -1) { // call declined
      Serial.println("Call declined");
    } */else if (data.indexOf("+CPAS") != -1) {
      updatedFromGsm = true;
      waitingForStatusUpdate = false;
      lastStatusUpdateReceivedAt = millis();
      if(data.indexOf("0") != -1) {
        phoneStatus = READY;
      } else if (data.indexOf("1") != -1) {
        phoneStatus = UNAVAILABLE;
      } else if (data.indexOf("2") != -1) {
        phoneStatus = UNKNOWN;
      } else if (data.indexOf("3") != -1) {
        phoneStatus = RINGING;
      } else if (data.indexOf("4") != -1) {
        phoneStatus = CALL_IN_PROGRESS;
      } else if (data.indexOf("5") != -1) {
        phoneStatus = ASLEEP;
      }
    }
  } else if (!waitingForStatusUpdate || ((millis() - lastStatusUpdateReceivedAt) > STATUS_UPDATE_TIMEOUT)) {
    // Serial.println("query phone activity");
    queryPhoneActivity();
    waitingForStatusUpdate = true;
  }

  if(!updatedFromGsm) {
    return;
  }

  if (phonePickedUp && isDialing && (millis() >= millisAtLastNumStop + NUMBER_TIMEOUT_MS)) {
    isDialing = false;
    isOnCall = true;
    callNumber();
  }

  // this check is needed in case we missed reading when other party hung up when we didn't answer
  // if (isCalling && phoneStatus != CALL_IN_PROGRESS) {
  //   Serial.println("not calling");
  //   gsm.println("ATH");
  //   isCalling = false;
  //   flushSerialBuffer();
  // }

  if (ringTask.isEnabled() != (phoneStatus == RINGING)) {
    Serial.println("Set ringing to " + (phoneStatus == RINGING));
    if(phoneStatus == RINGING) {
      ringTask.enable();
    } else {
      ringTask.disable();
    }
  }
}

void setup() {
  Serial.begin(9600);

  gsm.setTimeout(10);
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

  pinMode(RINGER_ENABLE, OUTPUT);
  pinMode(RINGER_FWD, OUTPUT);
  pinMode(RINGER_REV, OUTPUT);
  pinMode(RINGER_PWM, OUTPUT);
  pinMode(RINGER_CURRENT_SENSE, OUTPUT);

  digitalWrite(RINGER_ENABLE, LOW);

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

  updateGsmStageTask.enable();
}

void loop() {
  long now = millis();

  dialEnable.process(now);
  dialPulse.process(now);
  buttonCall.process(now);
  buttonPickup.process(now);

  updateSerial();

  ts.execute();
}
