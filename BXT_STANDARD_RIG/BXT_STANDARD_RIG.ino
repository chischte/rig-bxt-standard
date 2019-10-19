/*
 * *****************************************************************************
 * BXT STANDARD RIG
 * *****************************************************************************
 * Program to control the BXT standard Rig
 * *****************************************************************************
 * Michael Wettstein
 * October 2019, Zürich
 * *****************************************************************************
 * TODO:
 * SYSTEM RESET FUNKTION HINZUFÜGEN:
 * Add interrupt for start/stop button on connector IN1.
 *
 * TIMEOUTFUNKTION HINZUFÜGEN
 * nach Ablauf der Timeoutzeit von 40" soll ein reset durchgeführt werden
 * es soll auf dem eeprom gespeichert werden in welchem schritt der reset wie oft durchgeführt wurde
 * nach dem 3. timeout soll das rig ausschalten und ein error blink soll laufen
 * wenn die machine nicht läuft soll alle 3 sekunden ausgegeben werden bei welchem Schritt wie oft ein
 * reset durchgeführt wurde.
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
// DECLARATION OF VARIABLES / DATA TYPES
//******************************************************************************
byte cycleStep = 0;
bool autoMode = 0; // Betriebsmodus 0 = Step, 1 = Automatik
bool stepModeRunning = false;
bool autoModeRunning = false;
volatile bool machineRunning = false;
bool rigResetStage = 0;
const byte startStopInterruptPin = CONTROLLINO_IN1;
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

///////////////////////////////////////////////////////////////
//TODO INSERT TIMERS HERE
///////////////////////////////////////////////////////////////
//Insomnia nextStepTimer;
//Insomnia errorBlinkTimer;

unsigned long resetTimeoutTime = 40000; // reset rig after 40 seconds inactivity
Insomnia resetTimeout(resetTimeoutTime);
Insomnia errorPrintTimeout(2000);
//******************************************************************************

void TestRigReset() {
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
      rigResetStage = 2;
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

void SwitchToNextStep() {
  stepModeRunning = false;
  cycleStep++;
  if (cycleStep == numberOfMainCycleSteps)
    cycleStep = 0;
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

void RunResetTimeout() {
  static byte timeoutCounter;
  byte maxNoOfTimeoutsInARow = 3;
  //DETECTING THE END RIGHT ENDSWITCH RESETS THE TIMEOUT
  if (EndSwitchRight.switchedHigh()) {
    resetTimeout.resetTime();
    timeoutCounter = 0;
  }
  if (resetTimeout.timedOut()) {
    eepromErrorLog.countOneUp(cycleStep); // count the error in the eeprom log
    timeoutCounter++;
    rigResetStage = 0; // rig reset will run
    resetTimeout.resetTime();
  }
  if (timeoutCounter == (maxNoOfTimeoutsInARow - 1)) {
    // MACHINE RESETS AND STOPS RUNNING
    autoModeRunning = false;
    // ERROR BLINK STARTS
    timeoutCounter = 0;
  }
}
void toggleMachineRunningISR() {

  static unsigned long last_interrupt_time = 0;
  unsigned long interrupt_time = millis();
  unsigned long interruptDebounceTime = 200;
  if (interrupt_time - last_interrupt_time > interruptDebounceTime) {
    autoModeRunning = !autoModeRunning; //im Auto-Modus wird ein- oder ausgeschaltet
    resetTimeout.resetTime();
    stepModeRunning = true; // im Step-Modus wird der nächste Schritt gestartet
    last_interrupt_time = interrupt_time;
  }
}
//******************************************************************************
// MAIN TEST CYCLE:
//******************************************************************************
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
//******************************************************************************
void setup() {
  //******************************************************************************
  StartButton.setDebounceTime(100);
  EndSwitchLeft.setDebounceTime(100);
  EndSwitchRight.setDebounceTime(100);
  ModeSwitch.setDebounceTime(200);
  StrapDetectionSensor.setDebounceTime(500);
  attachInterrupt(digitalPinToInterrupt(startStopInterruptPin), toggleMachineRunningISR, RISING);
  Serial.begin(115200);
  Serial.println("EXIT SETUP");
}

//******************************************************************************
void loop() {
  //******************************************************************************
  // DETEKTIEREN OB DER SCHALTER AUF STEP- ODER AUTO-MODUS EINGESTELLT IST:
  autoMode = ModeSwitch.requestButtonState();

  // DETEKTIEREN OB DER START-KNOPF ERNEUT GEDRÜCKT WIRD:
  if (StartButton.switchedHigh()) {
    autoModeRunning = !autoModeRunning; //im Auto-Modus wird ein- oder ausgeschaltet
    resetTimeout.resetTime();
    stepModeRunning = true; // im Step-Modus wird der nächste Schritt gestartet
  }

  // ABFRAGEN DER BANDDETEKTIERUNG:
  bool strapDetected = !StrapDetectionSensor.requestButtonState();
  // IM AUTO MODUS FALLS KEIN BAND DETEKTIERT RIG AUSSCHALTEN:
  if (!strapDetected) {
    autoModeRunning = false;
    cycleStep = 0;
    ResetCylinderStates();
  }

  TestRigReset(); //if reset flag is set 0

  // BESTIMMEN OB TEST RIG LÄUFT ODER NICHT
  machineRunning = ((autoMode && autoModeRunning) || (!autoMode && stepModeRunning));

  // ERKENNEN OB DIE MASCHINE EINGESCHALTET WURDE:
  static bool previousMachineRunningState = false;
  if (previousMachineRunningState != machineRunning) {
    if (machineRunning) {
      resetTimeout.resetTime();  // reset timeout after switch on
    }
    previousMachineRunningState = machineRunning;
  }

// AUFRUFEN DER UNTERFUNKTIONEN JE NACHDEM OB DAS RIG LÄUFT ODER NICHT:
  if (machineRunning) {
    RunResetTimeout();
    RunMainTestCycle();
  } else {
    PrintErrorLog();
  }
}
