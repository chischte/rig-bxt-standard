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
 * Add interrupt for start/stop button.
 * // attachInterrupt(digitalPinToInterrupt(pin), ISR, mode)
 //
 //const byte digital_output = CONTROLLINO_D0;
 //const byte interruptPin = CONTROLLINO_IN1;
 //volatile byte state = LOW;
 //
 //void setup() {
 // pinMode(digital_output, OUTPUT);
 // pinMode(interruptPin, INPUT);
 // attachInterrupt(digitalPinToInterrupt(interruptPin), blink, CHANGE);
 //}
 //
 //void loop() {
 // digitalWrite(digital_output, state);
 //}
 //
 //void blink() {
 // state = !state;
 //}
 *
 * ALSO DEBOUNCE THE INTERRUPT BUTTON:
 //    * void my_interrupt_handler()
 //{
 // static unsigned long last_interrupt_time = 0;
 // unsigned long interrupt_time = millis();
 // // If interrupts come faster than 200ms, assume it's a bounce and ignore
 // if (interrupt_time - last_interrupt_time > 200)
 // {
 //   ... do your thing
 // last_interrupt_time = interrupt_time;
 // }
 //}
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
bool machineRunning = false;
bool rigResetCompleted = 1;
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
//Insomnia resetTimeout;
Insomnia errorPrintTimeout(2000);
//******************************************************************************

void TestRigReset() {
  if (rigResetCompleted == 0) {
    machineRunning = false;
    cycleStep = 0;
    WippenhebelZylinder.stroke(1500, 0);
    if (WippenhebelZylinder.stroke_completed()) {
      ResetCylinderStates();
      rigResetCompleted = 1;
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
      for (int i = 0; i < numberOfMainCycleSteps; i++) {
        // PRINT OUT ERROR LOG

      }
      errorPrintTimeout.resetTime();
    }
  }
}
void RunResetTimeout() {
  static byte timeoutCounter;
  byte maxNoOfTimeoutsInARow = 3;
  if (EndSwitchRight.switchedHigh()) {
    timeoutCounter = 0;
  }
  //
  //IF (TIMEOUT TIMED OUT, RUN RESET LOOP){
  timeoutCounter++;
  //}
  if (timeoutCounter == (maxNoOfTimeoutsInARow - 1)) {
    // MACHINE RESETS AND STOPS RUNNING
    // ERROR BLINK STARTS
    timeoutCounter = 0;
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
    stepModeRunning = true; // im Step-Modus wird der nächste Schritt gestartet
  }

  // ABFRAGEN DER BANDDETEKTIERUNG:
  bool strapDetected = !StrapDetectionSensor.requestButtonState();
  // IM AUTO MODUS FALLS KEIN BAND DETEKTIERT RIG AUSSCHALTEN:
  if (autoMode && !strapDetected) {
    autoModeRunning = false;
    cycleStep = 0;
    ResetCylinderStates();
  }
  void TestRigReset(); //if reset flag is set 0

  // BESTIMMEN OB TEST RIG LÄUFT ODER NICHT
  machineRunning = ((autoMode && autoModeRunning) || (!autoMode && stepModeRunning));

  // AUFRUFEN DER UNTERFUNKTIONEN JE NACHDEM OB DAS RIG LÄUFT ODER NICHT:
  if (machineRunning) {
    RunMainTestCycle();
    RunResetTimeout();

  } else {
    PrintErrorLog();
  }

}
