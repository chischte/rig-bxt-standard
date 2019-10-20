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

// SET UP EEPROM COUNTER:
//******************************************************************************
// SETUP EEPROM ERROR LOG:
// create one storage slot for every mainCycleStep to count and
// store how often there has been a timeout.
int numberOfEepromValues = (endOfMainCycleEnum - 1);
int eepromSize = 4096;
EEPROM_Counter eepromErrorLog(eepromSize, numberOfEepromValues);

//******************************************************************************
// DECLARATION OF GLOBAL VARIABLES
//******************************************************************************
byte cycleStep = 0;
bool rigResetStage = 0;
bool machineRunning = false;
// FOR THE INTERRUPT SERVICE ROUTINE:
volatile bool stepModeRunning = false;
volatile bool autoModeRunning = false;
volatile bool errorBlinkState = false;
// FOR THE PINS:
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

Debounce StartButton(CONTROLLINO_A3);
Debounce ModeSwitch(CONTROLLINO_A2);
Debounce EndSwitchLeft(CONTROLLINO_A5);
Debounce EndSwitchRight(CONTROLLINO_A0);
Debounce StrapDetectionSensor(A1);

Insomnia errorBlinkTimer;
Insomnia resetTimeout(40*1000L); // reset rig after 40 seconds inactivity
Insomnia errorPrintTimeout(2000);
//******************************************************************************

void SwitchToNextStep() {
  stepModeRunning = false;
  cycleStep++;
  if (cycleStep == numberOfMainCycleSteps)
    cycleStep = 0;
  Serial.print("Current Step: ");
  Serial.println(cycleName[cycleStep]);
}

unsigned long ReadCoolingPot() {
  int potVal = analogRead(CONTROLLINO_A4);
  unsigned long coolingTime = map(potVal, 1023, 0, 4000, 14000); // Abkühlzeit min 4, max 14 Sekunden
  return coolingTime;
}

void PrintErrorLog() {
  if (!machineRunning) {
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
  eepromErrorLog.countOneUp(cycleStep); // count the error in the eeprom log of the current step
}

void CheckResetTimeout() {
  static byte shutOffCounter;
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
      // machine resets
      rigResetStage = 0; // rig reset will run
      resetTimeout.resetTime();
    } else {
      // machine stops running
      autoModeRunning = false;
      errorBlinkState = 1; // error blink starts
      shutOffCounter = 0;
    }
  }
}

void ResetTestRig() {
  static bool runAgainAfterReset;
  if (rigResetStage == 0) {
    runAgainAfterReset = autoModeRunning;
    autoModeRunning = false;
    cycleStep = 0;
    rigResetStage = 1;
  }
  if (rigResetStage == 1) {
    WippenhebelZylinder.stroke(1500, 0);
    if (WippenhebelZylinder.stroke_completed()) {
      ResetCylinderStates();
      rigResetStage = 2; // reset completed
      autoModeRunning = runAgainAfterReset;
    }
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
  static unsigned long last_interrupt_time;
  unsigned long interrupt_time = millis();
  unsigned long interruptDebounceTime = 200;
  if (interrupt_time - last_interrupt_time > interruptDebounceTime) {
    autoModeRunning = !autoModeRunning; //im Auto-Modus wird ein- oder ausgeschaltet
    stepModeRunning = true; // im Step-Modus wird der nächste Schritt gestartet
    errorBlinkState = false;
    last_interrupt_time = interrupt_time;
  }
}

void GenerateErrorBlink() {
  if (errorBlinkTimer.delayTimeUp(800)) {
    digitalWrite(errorBlinkRelay, !digitalRead(errorBlinkRelay));
  }
}

void RunMainTestCycle() {
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
      SwitchToNextStep();
    }
    break;
    //******************************************************************************
  case WippenhebelZiehen1:
    WippenhebelZylinder.stroke(1500, 1000);
    if (WippenhebelZylinder.stroke_completed()) {
      SwitchToNextStep();
    }
    break;

  case BandVorschieben:
    SpanntastenZylinder.stroke(550, 0);
    if (SpanntastenZylinder.stroke_completed()) {
      SwitchToNextStep();
    }
    break;

  case BandKlemmen:
    BandKlemmZylinder.set(1);
    SwitchToNextStep();
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
      SwitchToNextStep();
      stepModeRunning = true; // springe direkt zum nächsten Step
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
    SwitchToNextStep();
    stepModeRunning = true; // springe direkt zum nächsten Step
    break;
    //******************************************************************************
  case BandSchneiden:
    MesserZylinder.stroke(2000, 2000);
    if (MesserZylinder.stroke_completed()) {
      SwitchToNextStep();
    }
    break;

  case Schweissen:
    SchweisstastenZylinder.stroke(500, ReadCoolingPot());
    if (SchweisstastenZylinder.stroke_completed()) {
      SwitchToNextStep();
    }
    break;

  case WippenhebelZiehen2:
    WippenhebelZylinder.stroke(1500, 1000);
    if (WippenhebelZylinder.stroke_completed()) {
      SwitchToNextStep();
    }
    break;

  case BandklemmeLoesen:
    BandKlemmZylinder.set(0);
    SwitchToNextStep();
    break;
  }
}

void setup() {
  StartButton.setDebounceTime(100);
  EndSwitchLeft.setDebounceTime(100);
  EndSwitchRight.setDebounceTime(100);
  ModeSwitch.setDebounceTime(200);
  StrapDetectionSensor.setDebounceTime(500);
  attachInterrupt(digitalPinToInterrupt(startStopInterruptPin), ToggleMachineRunningISR, RISING);
  Serial.begin(115200);
  Serial.println("EXIT SETUP");
  pinMode(startStopInterruptPin, INPUT);
  pinMode(errorBlinkRelay, OUTPUT);
}

void loop() {

  static bool autoMode;
  static bool stepMode;
  // DETEKTIEREN OB DER SCHALTER AUF STEP- ODER AUTO-MODUS EINGESTELLT IST:
  autoMode = ModeSwitch.requestButtonState();
  stepMode = !autoMode;

  // DEAKTIVIERT DA NEU EIN INTERUPT PIN VERWENDET WIRD:
  // DETEKTIEREN OB DER START-KNOPF ERNEUT GEDRÜCKT WIRD:
  //  if (StartButton.switchedHigh()) {
  //    autoModeRunning = !autoModeRunning; //im Auto-Modus wird ein- oder ausgeschaltet
  //    stepModeRunning = true; // im Step-Modus wird der nächste Schritt gestartet
  //  }

  // ABFRAGEN DER BANDDETEKTIERUNG:
  bool strapDetected = !StrapDetectionSensor.requestButtonState();
  // IM AUTO MODUS FALLS KEIN BAND DETEKTIERT RIG AUSSCHALTEN:
  if (!strapDetected) {
    autoModeRunning = false;
    cycleStep = 0;
    errorBlinkState = 1;
    ResetCylinderStates();
  }

  // DER TIMEOUT TIMER LÄUFT NUR AB WENN DER AUTO MODUS LÄUFT:
  if (!autoModeRunning) {
    resetTimeout.resetTime();
  }

  CheckResetTimeout(); // check if a timeout has happened

  ResetTestRig(); //if reset flag is set to 0

  // BESTIMMEN OB TEST RIG LAUFEN DARF ODER NICHT:
  machineRunning = ((autoMode && autoModeRunning) || (stepMode && stepModeRunning));

//  // ERKENNEN OB DAS RIG EINGESCHALTET WURDE:
//  static bool previousMachineRunningState = false;
//  if (previousMachineRunningState != machineRunning) {
//    if (machineRunning) {
//      resetTimeout.resetTime();  // reset timeout after switch on
//      errorBlinkState = 0; // disable error blink
//    }
//    previousMachineRunningState = machineRunning;
//  }

// AUFRUFEN DER UNTERFUNKTIONEN JE NACHDEM OB DAS RIG LÄUFT ODER NICHT:
  if (machineRunning) {
    RunMainTestCycle();
  } else {
    PrintErrorLog();
  }
}
