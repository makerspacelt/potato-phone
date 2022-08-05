#include <Arduino.h>
#include <AltSoftSerial.h>
#include <InputDebounce.h>
#include <TaskScheduler.h>

/*
  Rotary phone number counter using interrupts
  More info to come later
*/

// #define TONE_ENABLE 5
#define TONE_ENABLE 12
#define RINGER_ENABLE 4
#define STATUS_LED 3


//-------------
#define DIALPAD_PIN 11 // Yellow wire
#define NUMBER_STOP_PIN 10 // Blue wire
#define PHONE_PICKED_UP_PIN 2

#define NUMBER_TIMEOUT_MS 4000
#define RING_TIMEOUT_MS 6000

#define DIAL_DEBOUNCE_DELAY 20
#define BUTTON_DEBOUNCE_DELAY 10

#define GSM_UPDATE_RATE_DT 250
#define STATUS_UPDATE_TIMEOUT 500
//-------------

enum PAS {
  READY,  UNAVAILABLE, UNKNOWN,  RINGING,  CALL_IN_PROGRESS,  ASLEEP
};

enum ToneMode {
  OFF, ON, WAITING_FOR_ANSWER, CALLED_NUMBER_BUSY
};

static InputDebounce dialEnable;
static InputDebounce dialPulse;
static InputDebounce buttonPickup;

Scheduler ts;

void updateGsmState();
Task updateGsmStageTask(GSM_UPDATE_RATE_DT * TASK_MILLISECOND, TASK_FOREVER, &updateGsmState, &ts);

ToneMode toneMode = OFF;
void updateTone();
Task updateToneTask(10 * TASK_MILLISECOND, TASK_FOREVER, &updateTone, &ts);

unsigned long statusLedLastUpdateTimestamp = 0;
void updateStatusLed();
Task updateStatusLedTask(10 * TASK_MILLISECOND, TASK_FOREVER, &updateStatusLed,  &ts);

//-------------
String phoneNumber = "";
int pulses = 0;

boolean triedDialing = false;
boolean triedCalling = false;

boolean phonePickedUp = false;
boolean isDialing = false;
boolean isBellEnabled = false;

PAS phoneStatus = UNKNOWN;

unsigned long millisAtLastNumStop = 0;

AltSoftSerial gsm(9, 8);
//-------------

void queryPhoneActivity() {
  gsm.println("AT+CPAS");
}

void pulseCounter() {
  Serial.println("Pulse detected " + String(millis()));
  if (phonePickedUp && phoneStatus == READY) {
    pulses += 1;
  }
}

void endDialPulseCount(uint8_t pinIn) {
  // Serial.println("Num stop called");
  if (isDialing) {
    millisAtLastNumStop = 0;
  } else {
    isDialing = true;
  }
  phoneNumber += (pulses == 10) ? 0 : pulses;
  Serial.println("Phone number: " + phoneNumber);
  pulses = 0;
  millisAtLastNumStop = millis();
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
}

void pickupPhone(uint8_t pinIn) {
  Serial.println("Pickup phone");

  phonePickedUp = true;
  if (phoneStatus == READY) {
    toneMode = ON;
  } else {
    toneMode = OFF;
  }
  if (phoneStatus == RINGING) {
    answerCall();
  }
  phoneNumber = "";
  pulses = 0;
}

void hungupPhone(uint8_t pinIn) {
  Serial.println("Hungup phone");

  phonePickedUp = false;
  toneMode = OFF;
  isDialing = false;
  triedDialing = false;
  triedCalling = false;
  if (phoneStatus == CALL_IN_PROGRESS) {
    Serial.println("Hungup phone while call in progress, send ATH");
    gsm.println("ATH");
  }
  millisAtLastNumStop = 0;
  phoneNumber = "";
  pulses = 0;
}

void callNumber() {
  if (triedCalling) {
    return;
  }
  triedCalling = true;

  String telNumCommand = "ATD"+phoneNumber+";";
  Serial.println("Calling: " + telNumCommand);
  gsm.flush(); //
  gsm.println(telNumCommand);
  gsm.flush();
}

void startDialPulseCount(uint8_t pinIn) {
  Serial.println("Start pulse count");
  pulses = 0;
}

void pulseDial(int8_t pinIn) {
  Serial.println("Pulse dial");
  toneMode = OFF;
  triedDialing = true;
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

void enableBell() {
  Serial.println("Enable bell");
  isBellEnabled = true;
  digitalWrite(RINGER_ENABLE, HIGH);
}

void disableBell() {
  digitalWrite(RINGER_ENABLE, LOW);
  isBellEnabled = false;
  Serial.println("Disable bell");
}

boolean waitingForStatusUpdate = false;

unsigned long lastStatusUpdateReceivedAt = 0;

void updateGsmState() {
  // Serial.println("update gsm state");

  boolean updatedFromGsm = false;
  if (gsm.available()) {
    String data = gsm.readString();
    data.trim();
    // if(data.length() > 0) {
      // Serial.println(data + " " + millis());
      Serial.println(data);
    // }
    if (data.indexOf("RING") != -1) { 
      updatedFromGsm = true;// incomming call
      if (phonePickedUp) {
        Serial.println("Phone already picked up, send ATH");
        gsm.println("ATH"); // decline call if phone is picked up
      } else {
        Serial.println("Incomming call...");
      }
    }/* else if (data.indexOf("NO CARRIER") != -1) { // call ended
      Serial.println("Call ended");
    }*/ else if (data.indexOf("BUSY") != -1) { // call declined
      Serial.println("Call declined");
      toneMode = CALLED_NUMBER_BUSY;
    } else if (data.indexOf("+CPAS") != -1) {
      updatedFromGsm = true;
      waitingForStatusUpdate = false;
      lastStatusUpdateReceivedAt = millis();
      if(data.indexOf("0") != -1) {
        phoneStatus = READY;
        if (!triedDialing && phonePickedUp) {
          toneMode = ON;
        }
      } else if (data.indexOf("1") != -1) {
        phoneStatus = UNAVAILABLE;
        toneMode = OFF;
      } else if (data.indexOf("2") != -1) {
        phoneStatus = UNKNOWN;
        toneMode = OFF;
      } else if (data.indexOf("3") != -1) {
        if (phonePickedUp && phoneStatus != RINGING) {
          Serial.println("Someone called while phone picked up, send ATH");
          gsm.println("ATH");
        } else {
          phoneStatus = RINGING;
          toneMode = OFF;
        }
      } else if (data.indexOf("4") != -1) {
        phoneStatus = CALL_IN_PROGRESS;
        toneMode = OFF;
      } else if (data.indexOf("5") != -1) {
        phoneStatus = ASLEEP;
        toneMode = OFF;
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
    callNumber();
  }

  if (isBellEnabled != (phoneStatus == RINGING)) {
    Serial.println("Set ringing to " + (phoneStatus == RINGING));
    if(phoneStatus == RINGING) {
      enableBell();
    } else {
      disableBell();
    }
  }
}

int toneState = LOW;
ToneMode prevMode = OFF;

long toneStarted = 0;
int busyTonePlayedTimes = 0;

const long WAITING_TONE_TONE_LENGTH = 1500;
const long WAITING_TONE_SILENCE_LENGT = 1000;
const long WAITING_TONE_PERIOD = WAITING_TONE_TONE_LENGTH + WAITING_TONE_SILENCE_LENGT;

const int BUSY_TONE_COUNT = 3;
const long BUSY_TONE_TONE_LENGTH = 1000;
const long BUSY_TONE_SILENCE_LENGTH = 500;
const long BUSY_TONE_PERIOD = BUSY_TONE_TONE_LENGTH + BUSY_TONE_SILENCE_LENGTH;

void updateTone() {
  if(prevMode != toneMode) {
    // Serial.println("New tone mode");
    prevMode = toneMode;
    toneState = LOW;
    busyTonePlayedTimes = 0;
    toneStarted = millis();
  }
  long now = millis();
  if(toneMode == OFF) {
    toneState = LOW;
  } else if (toneMode == ON) {
    toneState = HIGH;
  } else if (toneMode == WAITING_FOR_ANSWER) {
    int currentPeriod = (now - toneStarted) % WAITING_TONE_PERIOD;
    toneState = currentPeriod > WAITING_TONE_TONE_LENGTH ? LOW : HIGH;
  } else if (toneMode == CALLED_NUMBER_BUSY) {
    int timesPlayed = (now - toneStarted) / BUSY_TONE_PERIOD;
    if (timesPlayed >= BUSY_TONE_COUNT) {
      toneState = LOW;
      toneMode = OFF;
    } else {
      int currentPeriod = (now - toneStarted) % BUSY_TONE_PERIOD;
      toneState = currentPeriod > BUSY_TONE_TONE_LENGTH ? LOW : HIGH;
    }
  }

  // Serial.print(toneMode);
  // Serial.print(" ");
  // Serial.println(toneState);
  digitalWrite(TONE_ENABLE, toneState);
}

byte statusLedPhase = 0;
short statusLedTimeRemaining = 0;

const short statusReadyPhases[] = {4000, 100};
const short statusUnavailablePhases[] = {100, 100};
const short statusUnknownPhases[] = {500, 100};
const short statusRingingPhases[] = {300, 1000};
const short statusCallInProgssPhases[] = {1000, 100, 100, 100};
const short statusAsleepPhases[] = {8000, 1000, 100, 100};

void updateNPhaseLedStatus(byte phaseCount, const short phases[]) {
  statusLedPhase++;
  if (statusLedPhase >= phaseCount) {
    statusLedPhase = 0;
  }

  digitalWrite(STATUS_LED, statusLedPhase % 2 == 0 ? LOW : HIGH);
  statusLedTimeRemaining = phases[statusLedPhase];
}

void updateStatusLed() {
  unsigned long lastUpdate = statusLedLastUpdateTimestamp;
  statusLedLastUpdateTimestamp = millis();
  unsigned short dt = statusLedLastUpdateTimestamp - lastUpdate;
  statusLedTimeRemaining -= dt;
  if (statusLedTimeRemaining > 0) {
    return;
  }

  switch (phoneStatus) {
    case READY:
      updateNPhaseLedStatus(2, statusReadyPhases);
    break;
    case UNAVAILABLE:
      updateNPhaseLedStatus(2, statusUnavailablePhases);
    break;
    case UNKNOWN:
      updateNPhaseLedStatus(2, statusUnknownPhases);
    break;
    case RINGING:
      updateNPhaseLedStatus(2, statusRingingPhases);
    break;
    case CALL_IN_PROGRESS:
      updateNPhaseLedStatus(4, statusCallInProgssPhases);
    break;
    case ASLEEP:
      updateNPhaseLedStatus(4, statusAsleepPhases);
    break;
  }

}

void setup() {
  Serial.begin(9600);

  gsm.setTimeout(10);
  gsm.begin(9600);

  pinMode(DIALPAD_PIN, INPUT);
  pinMode(NUMBER_STOP_PIN, INPUT);
  pinMode(PHONE_PICKED_UP_PIN, INPUT);

  pinMode(TONE_ENABLE, OUTPUT);

  pinMode(RINGER_ENABLE, OUTPUT);
  pinMode(STATUS_LED, OUTPUT);

  digitalWrite(TONE_ENABLE, LOW);
  digitalWrite(RINGER_ENABLE, LOW);


  phonePickedUp = digitalRead(PHONE_PICKED_UP_PIN) == LOW;

  dialEnable.registerCallbacks(NULL, endDialPulseCount);
  dialPulse.registerCallbacks(pulseDial, NULL);
  buttonPickup.registerCallbacks(pickupPhone, hungupPhone, NULL, NULL);


  dialEnable.setup(NUMBER_STOP_PIN, DIAL_DEBOUNCE_DELAY, InputDebounce::PIM_EXT_PULL_UP_RES, NULL, InputDebounce::ST_NORMALLY_CLOSED);
  dialPulse.setup(DIALPAD_PIN, DIAL_DEBOUNCE_DELAY, InputDebounce::PIM_EXT_PULL_UP_RES);
  buttonPickup.setup(PHONE_PICKED_UP_PIN, InputDebounce::PIM_EXT_PULL_UP_RES);

  ts.startNow();

  updateGsmStageTask.enable();
  updateToneTask.enable();
  updateStatusLedTask.enable();
  
}

void loop() {
  long now = millis();

  dialEnable.process(now);
  dialPulse.process(now);
  buttonPickup.process(now);

  updateSerial();

  ts.execute();
}
