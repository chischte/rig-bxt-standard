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

#include <Controllino.h> // https://github.com/CONTROLLINO-PLC/CONTROLLINO_Library
#include <Cylinder.h>    // https://github.com/chischte/cylinder-library
#include <Debounce.h>    // https://github.com/chischte/debounce-library

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
//******************************************************************************
// DECLARATION OF VARIABLES / DATA TYPES
//******************************************************************************
byte cycleStep = 0;
bool autoMode = 0; // Betriebsmodus 0 = Step, 1 = Automatik
bool stepModeRunning = false;
bool autoModeRunning = false;

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
//******************************************************************************

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

unsigned long ReadCoolingPot(){
  int potVal = analogRead(CONTROLLINO_A4);
  unsigned long coolingTime = map(potVal, 1023, 0, 4000, 14000); // Abkühlzeit min 4, max 14 Sekunden
  return coolingTime;
}

void setup() {
  ResetCylinderStates();
  Serial.begin(115200);
  Serial.println("EXIT SETUP");
}

void loop() {

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

  //******************************************************************************
  // MAIN CYCLE:
  //******************************************************************************
  if ((autoMode && autoModeRunning) || (!autoMode && stepModeRunning)) {
    switch (cycleStep) {

    case BremszylinderZurueckfahren:
      if (!EndSwitchLeft.requestButtonState()) { // Bremszylinder ist nicht in Startposition
        BremsZylinder.set(1);
      } else {
        BremsZylinder.set(0);
        SwitchToNextStep();
      }
      break;

    case WippenhebelZiehen1:
      WippenhebelZylinder.stroke(1500, 1000);
      if (WippenhebelZylinder.stroke_completed()) {
        SwitchToNextStep();
      }
      break;

    case BandVorschieben:
      SpanntastenZylinder.stroke(450, 250);
      if (SpanntastenZylinder.stroke_completed()) {
        SwitchToNextStep();
      }
      break;

    case BandKlemmen:
      BandKlemmZylinder.set(1);
      SwitchToNextStep();
      break;

    case BandSpannen:
      SpanntastenZylinder.set(1);
      if (EndSwitchRight.requestButtonState()) {
        SwitchToNextStep();
        stepModeRunning = true; // springe direkt zum nächsten Step
      }
      break;

    case SpannkraftAufbau:
      SpanntastenZylinder.stroke(800, 0); // kurze Pause für Spannkraftaufbau
      if (SpanntastenZylinder.stroke_completed()) {
        SwitchToNextStep();
      }
      break;

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
}
