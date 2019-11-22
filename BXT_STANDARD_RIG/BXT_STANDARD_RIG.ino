/*
 * *****************************************************************************
 * BXT STANDARD RIG
 * *****************************************************************************
 * Program to control the BXT standard Rig
 * *****************************************************************************
 * Michael Wettstein
 * October 2019, Zürich
 * *****************************************************************************
 */

#include <Controllino.h>     // https://github.com/CONTROLLINO-PLC/CONTROLLINO_Library
#include <Cylinder.h>        // https://github.com/chischte/cylinder-library
#include <Debounce.h>        // https://github.com/chischte/debounce-library
#include <Insomnia.h>        // https://github.com/chischte/insomnia-delay-library
#include <EEPROM_Counter.h>  // https://github.com/chischte/eeprom-counter-library
#include <EEPROM_Logger.h>   // https://github.com/chischte/eeprom-logger-library.git
#include <Nextion.h>         // https://github.com/itead/ITEADLIB_Arduino_Nextion

#include <StateController.h> // contains all machine states

//******************************************************************************
// DEFINE NAMES AND SEQUENCE OF STEPS FOR THE MAIN CYCLE:
//******************************************************************************
enum mainCycleSteps {
  WippenhebelZiehen,
  BandklemmeLoesen,
  SchlittenZurueckfahren,
  BandVorschieben,
  BandSchneiden,
  BandKlemmen,
  BandSpannen,
  Schweissen,
  endOfMainCycleEnum
};
byte numberOfMainCycleSteps = endOfMainCycleEnum;

// DEFINE NAMES TO DISPLAY ON THE TOUCH SCREEN:
String cycleName[] = { "WIPPENHEBEL", "KLEMME LOESEN", "ZURUECKFAHREN", "BAND VORSCHIEBEN",
    "SCHNEIDEN", "KLEMMEN", "SPANNEN", "SCHWEISSEN" };

//******************************************************************************
// DEFINE NAMES AND SET UP VARIABLES FOR THE CYCLE COUNTER:
//******************************************************************************
enum counter {
  longtimeCounter,   //
  shorttimeCounter,  //
  coolingTime,       // [s]
  endOfCounterEnum
};

int counterNoOfValues = endOfCounterEnum;

//******************************************************************************
// DEFINE NAMES AND SET UP VARIABLES FOR THE ERROR LOGGER:
//******************************************************************************
enum logger {
  emptyLog,        // marks empty logs
  toolResetError,
  shortTimeoutError,
  longTimeoutError,
  shutDownError,
  magazineEmpty,
  manualOn,
  manualOff
};

String errorCode[] = {        //
        "n.a.",               //
            "STEUERUNG EIN",  //
            "AUTO RESET",     //
            "AUTO PAUSE",     //
            "AUTO STOP",      //
            "BAND LEER",      //
            "MANUELL START",     //
            "MANUELL STOP" };  //

int loggerNoOfLogs = 50;

//******************************************************************************
// DECLARATION OF VARIABLES
//******************************************************************************
bool strapDetected;
bool errorBlinkState = false;
byte timeoutDetected = 0;
int cycleTimeInSeconds = 30; // estimated value for the timout timer

// INTERRUPT SERVICE ROUTINE:
// volatile bool toggleMachineState = false;
// eepromErrorLog.setAllZero(); // to reset the error counter
//****************************************************************************** */

// PINS:

//******************************************************************************
// GENERATE INSTANCES OF CLASSES:
//******************************************************************************
Cylinder SchlittenZylinder(CONTROLLINO_D3);
Cylinder BandKlemmZylinder(CONTROLLINO_D4);
Cylinder SpanntastenZylinder(CONTROLLINO_D5);
Cylinder SchweisstastenZylinder(CONTROLLINO_D6);
Cylinder WippenhebelZylinder(CONTROLLINO_D7);
Cylinder MesserZylinder(CONTROLLINO_D8);
Cylinder ErrorBlinkRelay(CONTROLLINO_R0);
Cylinder EmergencyLight(CONTROLLINO_R1);

Debounce EndSwitchRight(CONTROLLINO_A0);
Debounce StrapDetectionSensor(A1);
Debounce EndSwitchLeft(CONTROLLINO_A2);

Insomnia errorBlinkTimer;
unsigned long blinkDelay = 600;
Insomnia resetTimeout; // reset rig after 40 seconds inactivity
Insomnia resetDelay;
Insomnia coolingDelay;
Insomnia nexResetButtonTimeout;

StateController stateController(numberOfMainCycleSteps);

EEPROM_Counter eepromCounter;
EEPROM_Logger errorLogger;

//******************************************************************************

void printCurrentStep() {
  Serial.print(stateController.currentCycleStep());
  Serial.print(" ");
  Serial.println(cycleName[stateController.currentCycleStep()]);
}

void runTimeoutManager() {

  static byte timeoutCounter;
  // RESET TIMOUT TIMER:
  if (EndSwitchRight.switchedHigh()) {
    resetTimeout.resetTime();
    timeoutCounter = 0;
  }
  // DETECT TIMEOUT:
  if (!timeoutDetected) {
    if (resetTimeout.timedOut()) {
      timeoutCounter++;
      timeoutDetected = 1;
    }
  } else {
    resetTimeout.resetTime();
  }
  // 1st TIMEOUT - RESET IMMEDIATELY:
  if (timeoutDetected && timeoutCounter == 1) {
    writeErrorLog(shortTimeoutError);
    Serial.println("TIMEOUT 1 > RESET");
    stateController.setRunAfterReset(1);
    stateController.setResetMode(1);
    timeoutDetected = 0;
  }
  // 2nd TIMEOUT - WAIT AND RESET:
  if (timeoutDetected && timeoutCounter == 2) {
    static byte subStep = 1;
    if (subStep == 1) {
      Serial.println("TIMEOUT 2 > WAIT & RESET");
      errorBlinkState = 1;
      blinkDelay = 600;
      writeErrorLog(longTimeoutError);
      subStep++;
    }
    if (subStep == 2) {
      showInfoField();
      int remainingPause = resetDelay.remainingDelayTime() / 1000;
      printOnTextField("TIMEOUT " + String(remainingPause) + " s", "t4");
      if (resetDelay.delayTimeUp(3 * 60 * 1000L)) {
        errorBlinkState = 0;
        stateController.setRunAfterReset(1);
        stateController.setResetMode(1);
        timeoutDetected = 0;
        subStep = 1;
      }
    }
  }
  // 3rd TIMEOUT - SHUT OFF:
  if (timeoutDetected && timeoutCounter == 3) {
    showInfoField();
    printOnTextField("SHUT DOWN!", "t4");
    writeErrorLog(shutDownError);
    Serial.println("TIMEOUT 3 > STOP");
    stopTestRig();
    stateController.setCycleStepTo(0);
    blinkDelay = 2000;
    errorBlinkState = 1; // error blink starts
    timeoutCounter = 0;
    timeoutDetected = 0;
  }
}

void resetTestRig() {
  static byte resetStage = 1;
  stateController.setMachineRunningState(0);

  if (resetStage == 1) {
    resetCylinderStates();
    errorBlinkState = 0;
    clearTextField("t4");
    hideInfoField();
    stateController.setCycleStepTo(0);
    resetStage++;
  }
  if (resetStage == 2) {
    WippenhebelZylinder.stroke(1500, 0);
    if (WippenhebelZylinder.stroke_completed()) {
      resetStage++;
    }
  }
  if (resetStage == 3) {
    stateController.setCycleStepTo(0);
    stateController.setResetMode(0);
    bool runAfterReset = stateController.runAfterReset();
    stateController.setMachineRunningState(runAfterReset);
    resetStage = 1;
  }
}

void stopTestRig() {
  resetCylinderStates();
  stateController.setStepMode();
  stateController.setMachineRunningState(false);
}

void resetCylinderStates() {
  SchlittenZylinder.set(0);
  SpanntastenZylinder.set(0);
  SchweisstastenZylinder.set(0);
  WippenhebelZylinder.set(0);
  MesserZylinder.set(0);
  BandKlemmZylinder.set(0);
}

void generateErrorBlink() {
  if (errorBlinkState) {
    if (errorBlinkTimer.delayTimeUp(blinkDelay)) {
      ErrorBlinkRelay.toggle();
      EmergencyLight.toggle();
    }
  } else {
    ErrorBlinkRelay.set(0);
    EmergencyLight.set(1);
  }

}

void runMainTestCycle() {
  int cycleStep = stateController.currentCycleStep();
  static byte subStep = 1;

  switch (cycleStep) {

  case WippenhebelZiehen:
    WippenhebelZylinder.stroke(1500, 1000);
    if (WippenhebelZylinder.stroke_completed()) {
      stateController.switchToNextStep();
    }
    break;

  case BandklemmeLoesen:
    BandKlemmZylinder.set(0);
    stateController.switchToNextStep();
    break;

  case SchlittenZurueckfahren:
    SchlittenZylinder.stroke(2000, 0);
    if (SchlittenZylinder.stroke_completed()) {
      stateController.switchToNextStep();
    }
    break;

  case BandVorschieben:
    SpanntastenZylinder.stroke(1300, 0);
    if (SpanntastenZylinder.stroke_completed()) {
      stateController.switchToNextStep();
    }
    break;

  case BandKlemmen:
    BandKlemmZylinder.set(1);
    stateController.switchToNextStep();
    break;

  case BandSpannen:

    if (subStep == 1) {
      SpanntastenZylinder.set(1);
      if (EndSwitchRight.requestButtonState()) {
        subStep = 2;
      }
    }
    if (subStep == 2) {
      SpanntastenZylinder.stroke(800, 0); // kurze Pause für Spannkraftaufbau
      if (SpanntastenZylinder.stroke_completed()) {
        subStep = 1;
        stateController.switchToNextStep();
      }
    }
    break;

  case BandSchneiden:
    MesserZylinder.stroke(2500, 2000);
    if (MesserZylinder.stroke_completed()) {
      stateController.switchToNextStep();
    }
    break;

  case Schweissen:
    if (subStep == 1) {
      SchweisstastenZylinder.stroke(500, 0);
      if (SchweisstastenZylinder.stroke_completed()) {
        subStep++;
      }
    }
    if (subStep == 2) {
      unsigned long pauseTime = eepromCounter.getValue(coolingTime) * 1000;
      showInfoField();
      if (coolingDelay.delayTimeUp(pauseTime)) {
        hideInfoField();
        eepromCounter.countOneUp(shorttimeCounter);
        eepromCounter.countOneUp(longtimeCounter);
        subStep = 1;
        stateController.switchToNextStep();
      }
    }
    break;
  }
}

void writeErrorLog(byte errorCode) {
  long cycleNumber = eepromCounter.getValue(shorttimeCounter);
  long logTime = mergeCurrentTime();
  errorLogger.writeLog(cycleNumber, logTime, errorCode);
}

void setup() {

  // SET DATE AND TIME:
  //                       (0-31/0-7/mm/YY/hh/mm/ss)
  //Controllino_SetTimeDate(13, 3, 11, 19, 16, 00, 00);
  Controllino_RTC_init(0);

  // SETUP COUNTER AND LOGGER:
  eepromCounter.setup(0, 1023, counterNoOfValues);
  errorLogger.setup(1024, 4095, loggerNoOfLogs);
  // SET OR RESET COUNTER AND LOGGER:
  // eepromCounter.set(longtimeCounter, 13390);
  nextionSetup();
  EndSwitchLeft.setDebounceTime(100);
  EndSwitchRight.setDebounceTime(100);
  StrapDetectionSensor.setDebounceTime(500);
  Serial.begin(115200);
  printCurrentStep();
  // CREATE A SETUP ENTRY IN THE LOG:
  writeErrorLog(toolResetError);
  stateController.setStepMode();
  resetTimeout.setTime((eepromCounter.getValue(coolingTime) + cycleTimeInSeconds) * 1000);
  EmergencyLight.set(1);
  Serial.println(" ");
  Serial.println("EXIT SETUP");
}

void loop() {

  nextionLoop();

  // ABFRAGEN DER BANDDETEKTIERUNG, AUSSCHALTEN FALLS KEIN BAND:
  strapDetected = !StrapDetectionSensor.requestButtonState();
  if (!strapDetected) {
    errorBlinkState = 1;
    if (StrapDetectionSensor.switchedHigh()) {
      stopTestRig();
      showInfoField();
      printOnTextField("BAND LEER!", "t4");
      writeErrorLog(magazineEmpty);
    }
  }

  // DER TIMEOUT TIMER LÄUFT NUR AB, WENN DAS RIG IM AUTO MODUS LÄUFT:
  if (!(stateController.machineRunning() && stateController.autoMode())) {
    resetTimeout.resetTime();
  }

  // TIMEOUT ÜBERWACHEN, FEHLERSPEICHER SCHREIBEN, RESET ODER STOP EINLEITEN:
  runTimeoutManager();

  // FALLS RESET AKTIVIERT, TEST RIG RESETEN,
  if (stateController.resetMode()) {
    resetTestRig();
  }

  // ERROLR BLINK FALLS AKTIVIERT:
  generateErrorBlink();

  //IM STEP MODE HÄLT DAS RIG NACH JEDEM SCHRITT AN:
  if (stateController.stepSwitchHappened()) {
    if (stateController.stepMode()) {
      stateController.setMachineRunningState(false);
    }
    //PrintCurrentStep(); // zeigt den nächsten Step
  }

  // AUFRUFEN DER UNTERFUNKTIONEN JE NACHDEM OB DAS RIG LÄUFT ODER NICHT:
  if (stateController.machineRunning()) {
    runMainTestCycle();
  }
}
