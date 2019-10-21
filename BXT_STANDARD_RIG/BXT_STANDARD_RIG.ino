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

#include <Controllino.h>    // https://github.com/CONTROLLINO-PLC/CONTROLLINO_Library
#include <Cylinder.h>       // https://github.com/chischte/cylinder-library
#include <Debounce.h>       // https://github.com/chischte/debounce-library
#include <EEPROM_Counter.h> // https://github.com/chischte/eeprom-counter-library
#include <Insomnia.h>       // https://github.com/chischte/insomnia-delay-library
#include "MainCycleController.h" // contains all machine states

//******************************************************************************
// DEFINE NAMES AND SEQUENCE OF STEPS FOR THE MAIN CYCLE:
//******************************************************************************
enum mainCycleSteps {
  BremszylinderZurueckfahren,
  WippenhebelZiehen1,
  BandVorschieben,
  BandKlemmen,
  BandSpannen,
  SpannkraftAufbau,
  BandSchneiden,
  Schweissen,
  WippenhebelZiehen2,
  BandklemmeLoesen,
  endOfMainCycleEnum
};
byte numberOfMainCycleSteps = endOfMainCycleEnum;

// DEFINE NAMES TO DISPLAY ON THE TOUCH SCREEN:
String cycleName[] = { "BremszylinderZurueckfahren", "WippenhebelZiehen1", "BandVorschieben",
        "BandKlemmen", "BandSpannen", "SpannkraftAufbau", "BandSchneiden", "Schweissen",
        "WippenhebelZiehen2", "BandklemmeLoesen" };

//******************************************************************************
// SETUP EEPROM ERROR LOG:
//******************************************************************************
// create one storage slot for every mainCycleStep to count timeout errors
int numberOfEepromValues = (endOfMainCycleEnum - 1);
int eepromSize = 4096;
EEPROM_Counter eepromErrorLog(eepromSize, numberOfEepromValues);

//******************************************************************************
// DECLARATION OF VARIABLES
//******************************************************************************
// INTERRUPT SERVICE ROUTINE:
volatile bool toggleMachineState = false;
volatile bool errorBlinkState = false;

// PINS:
const byte startStopInterruptPin = CONTROLLINO_IN1;
const byte errorBlinkRelay = CONTROLLINO_R0;
//******************************************************************************
// GENERATE INSTANCES OF CLASSES:
//******************************************************************************
Cylinder BandKlemmZylinder(6);
Cylinder SpanntastenZylinder(7);
Cylinder BremsZylinder(5);
Cylinder SchweisstastenZylinder(8);
Cylinder WippenhebelZylinder(9);
Cylinder MesserZylinder(10);

Debounce ModeSwitch(CONTROLLINO_A2);
Debounce EndSwitchLeft(CONTROLLINO_A5);
Debounce EndSwitchRight(CONTROLLINO_A0);
Debounce StrapDetectionSensor(A1);

Insomnia errorBlinkTimer;
Insomnia resetTimeout(40 * 1000L); // reset rig after 40 seconds inactivity
Insomnia errorPrintTimeout(2000);

MainCycleController mainCycleController(numberOfMainCycleSteps);
//******************************************************************************

unsigned long ReadCoolingPot() {
  int potVal = analogRead(CONTROLLINO_A4);
  unsigned long coolingTime = map(potVal, 1023, 0, 4000, 14000); // Abkühlzeit min 4, max 14 Sekunden
  return coolingTime;
}

void PrintErrorLog() {
  if (!mainCycleController.machineRunning()) {
    if (errorPrintTimeout.timedOut()) {
      Serial.println("ERROR LIST: ");
      for (int i = 0; i < numberOfMainCycleSteps; i++) {
        Serial.print(cycleName[i]);
        Serial.print(" caused ");
        Serial.print(eepromErrorLog.getValue(i));
        Serial.println(" errors by timeout");
      }
      errorPrintTimeout.resetTime();
    }
  }
}

void WriteErrorLog() {
  eepromErrorLog.countOneUp(mainCycleController.currentCycleStep()); // count the error in the eeprom log of the current step
}

void PrintCurrentStep() {
  Serial.print(mainCycleController.currentCycleStep());
  Serial.print(" ");
  Serial.println(cycleName[mainCycleController.currentCycleStep()]);
}

bool DetectResetTimeout() {
  static byte shutOffCounter;
  bool triggerReset = false;
  //DETECTING THE RIGHT ENDSWITCH SHOWS THAT RIG IS MOVING AND RESETS THE TIMEOUT
  if (EndSwitchRight.switchedHigh()) {
    resetTimeout.resetTime();
    shutOffCounter = 0;
  }
  byte maxNoOfTimeoutsInARow = 3;
  if (resetTimeout.timedOut()) {
    Serial.println("TIMEOUT");
    WriteErrorLog();
    shutOffCounter++;
    if (shutOffCounter < maxNoOfTimeoutsInARow) {
      resetTimeout.resetTime();
	  triggerReset = true; // rig will reset
    } else {
      // machine stops running
      mainCycleController.setMachineRunningState(false);
      triggerReset = false; // rig will not reset
	  errorBlinkState = 1; // error blink starts
      shutOffCounter = 0;
    }
  }
  return triggerReset;
}

void ResetTestRig() {
  mainCycleController.setMachineRunningState(false);
  mainCycleController.setCycleStepTo(0);
  ResetCylinderStates();
  WippenhebelZylinder.stroke(1500, 0);
  while (!WippenhebelZylinder.stroke_completed()) {
    // wait
  }
}

void ResetCylinderStates() {
  BremsZylinder.set(0);
  SpanntastenZylinder.set(0);
  SchweisstastenZylinder.set(0);
  WippenhebelZylinder.set(0);
  MesserZylinder.set(0);
  BandKlemmZylinder.set(0);
}

void ToggleMachineRunningISR() {
  static unsigned long previousInterruptTime;
  unsigned long interruptDebounceTime = 200;
  if (millis() - previousInterruptTime > interruptDebounceTime) {
    // TEST RIG EIN- ODER AUSSCHALTEN:
    toggleMachineState = true;
  }
  previousInterruptTime = millis();
  errorBlinkState = 0;
}

void GenerateErrorBlink() {
  if (errorBlinkTimer.delayTimeUp(800)) {
    digitalWrite(errorBlinkRelay, !digitalRead(errorBlinkRelay));
  }
}

void RunMainTestCycle() {
  int cycleStep = mainCycleController.currentCycleStep();
  switch (cycleStep) {

  //******************************************************************************
  //SCHALTER DEAKTIVIERT UND ZUR FEHLERSUCHE DURCH TIMER ERSETZT
  //    case BremszylinderZurueckfahren:
  //      if (!EndSwitchLeft.requestButtonState()) { // Bremszylinder ist nicht in Startposition
  //        BremsZylinder.set(1);
  //      } else {
  //        BremsZylinder.set(0);
  //        SwitchToNextStep();
  //      }
  //      break;
  case BremszylinderZurueckfahren:
    BremsZylinder.stroke(2000, 0);
    if (BremsZylinder.stroke_completed()) {
      mainCycleController.switchToNextStep();
    }
    break;

  case WippenhebelZiehen1:
    WippenhebelZylinder.stroke(1500, 1000);
    if (WippenhebelZylinder.stroke_completed()) {
      mainCycleController.switchToNextStep();
    }
    break;

  case BandVorschieben:
    SpanntastenZylinder.stroke(550, 0);
    if (SpanntastenZylinder.stroke_completed()) {
      mainCycleController.switchToNextStep();
    }
    break;

  case BandKlemmen:
    BandKlemmZylinder.set(1);
    mainCycleController.switchToNextStep();
    break;

    //******************************************************************************
    //SCHALTER DEAKTIVIERT UND ZUR FEHLERSUCHE DURCH TIMER ERSETZT
    //    case BandSpannen:
    //         SpanntastenZylinder.set(1);
    //         if (EndSwitchRight.requestButtonState()) {
    //           SwitchToNextStep();
    //           stepModeRunning = true; // springe direkt zum nächsten Step
    //         }
    //         break;
  case BandSpannen:
    SpanntastenZylinder.stroke(3000, 500);
    if (SpanntastenZylinder.stroke_completed()) {
      mainCycleController.switchToNextStep();
    }
    break;

    //******************************************************************************
    //NICHT NOTWENDIG DA ZURZEIT ZEIT- STATT SCHALTERGESTEUERT:
    //    case SpannkraftAufbau:
    //         SpanntastenZylinder.stroke(800, 0); // kurze Pause für Spannkraftaufbau
    //         if (SpanntastenZylinder.stroke_completed()) {
    //           SwitchToNextStep();
    //         }
    //         break;
  case SpannkraftAufbau:
    mainCycleController.switchToNextStep();
    break;

  case BandSchneiden:
    MesserZylinder.stroke(2000, 2000);
    if (MesserZylinder.stroke_completed()) {
      mainCycleController.switchToNextStep();
    }
    break;

  case Schweissen:
    SchweisstastenZylinder.stroke(500, ReadCoolingPot());
    if (SchweisstastenZylinder.stroke_completed()) {
      mainCycleController.switchToNextStep();
    }
    break;

  case WippenhebelZiehen2:
    WippenhebelZylinder.stroke(1500, 1000);
    if (WippenhebelZylinder.stroke_completed()) {
      mainCycleController.switchToNextStep();
    }
    break;

  case BandklemmeLoesen:
    BandKlemmZylinder.set(0);
    mainCycleController.switchToNextStep();
    break;
  }
}

void setup() {
  EndSwitchLeft.setDebounceTime(100);
  EndSwitchRight.setDebounceTime(100);
  ModeSwitch.setDebounceTime(200);
  StrapDetectionSensor.setDebounceTime(500);
  attachInterrupt(digitalPinToInterrupt(startStopInterruptPin), ToggleMachineRunningISR, RISING);
  Serial.begin(115200);
  Serial.println("EXIT SETUP");
  //eepromErrorLog.setAllZero(); // to reset the error counter
  pinMode(startStopInterruptPin, INPUT);
  pinMode(errorBlinkRelay, OUTPUT);
}

void loop() {

// DETEKTIEREN OB DER SCHALTER AUF STEP- ODER AUTO-MODUS EINGESTELLT IST:
  if (ModeSwitch.requestButtonState()) {
    mainCycleController.setAutoMode();
  } else {
    mainCycleController.setStepMode();
  }

// MACHINE EIN- ODER AUSSCHALTEN (AUSGELÖST DURCH ISR):
  if (toggleMachineState) {
    mainCycleController.toggleMachineRunningState();
    toggleMachineState = false;
  }

// ABFRAGEN DER BANDDETEKTIERUNG:
  bool strapDetected = !StrapDetectionSensor.requestButtonState();

// FALLS KEIN BAND DETEKTIERT RIG AUSSCHALTEN UND ERROR BLINK AKTIVIEREN:
  if (!strapDetected) {
    ResetTestRig();
    errorBlinkState = 1;
  }

// DER TIMEOUT TIMER LÄUFT NUR AB WENN DAS RIG IM AUTO MODUS LÄUFT:
  if (!(mainCycleController.machineRunning() && mainCycleController.autoMode())) {
    resetTimeout.resetTime();
  }

// RESET ODER ABSCHALTEN FALLS EIN TIMEOUT DETEKTIERT WIRD:
  if (DetectResetTimeout()) {
    ResetTestRig();
  }

//IM STEP MODE HÄLT DAS RIG NACH JEDEM SCHRITT AN:
  if (mainCycleController.stepSwitchHappened()) {
    if (mainCycleController.stepMode()) {
      mainCycleController.setMachineRunningState(false);
    }
    PrintCurrentStep();
  }

// AUFRUFEN DER UNTERFUNKTIONEN JE NACHDEM OB DAS RIG LÄUFT ODER NICHT:
  if (mainCycleController.machineRunning()) {
    RunMainTestCycle();
  } else {
    SpanntastenZylinder.set(0);
    WippenhebelZylinder.set(0);
    PrintErrorLog();
  }
}
