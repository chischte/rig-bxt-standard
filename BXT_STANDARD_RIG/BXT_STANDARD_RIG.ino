/*
 * *****************************************************************************
 * BXT STANDARD RIG
 * *****************************************************************************
 * Program to control the BXT Standard Rig
 * *****************************************************************************
 * Michael Wettstein
 * October 2019, Zürich
 * *****************************************************************************
 */

#include <SPI.h>
#include <Controllino.h>
#include <Cylinder.h>       // https://github.com/chischte/cylinder-library
#include <Debounce.h>       // https://github.com/chischte/debounce-library

//*****************************************************************************
// DEFINE NAMES AND SEQUENCE OF STEPS FOR THE MAIN CYCLE:
//*****************************************************************************
enum mainCycleSteps {
  BremszylinderZurueckfahren,
  WippenhebelZiehen1,
  BandVorschieben,
  BandKlemmen,
  BandSpannen,
  SpannzylinderLoesen,
  BandSchneiden,
  Schweissen,
  WippenhebelZiehen2,
  BandklemmeLoesen
};
//*****************************************************************************
// DECLARATION OF VARIABLES / DATA TYPES
//*****************************************************************************
byte cycleStep = 0;
bool autoMode = false;  // Betriebsmodus 0 = Step, 1 = Automatik
bool stepCompleted = true;
bool machineRunning = false;		// Betriebsstatus
unsigned long coolTime; // Abkühlzeit

//*****************************************************************************
// GENERATE INSTANCES OF CLASSES:
//*****************************************************************************
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
//*****************************************************************************

void ResetCylinderStates() {
  BremsZylinder.set(0);
  SpanntastenZylinder.set(0);
  SchweisstastenZylinder.set(0);
  WippenhebelZylinder.set(0);
  MesserZylinder.set(0);
  BandKlemmZylinder.set(0);
}

void setup() {
  ResetCylinderStates();
}

void loop() {

  // DETEKTIEREN OB STEP ODER AUTO MODUS EINGESTELLT IST AKTIV IST:
  autoMode = ModeSwitch.requestButtonState();

  // IM AUTO MODUS SPRINGT DAS RIG AUTOMATISCH ZUM NÄCHSTEN SCHRITT:
  if (autoMode) {
    stepCompleted = false;
  }

  // IM STEP MODUS STOPPT DAS RIG NACH JEDEM SCHRITT:
  if (!autoMode && stepCompleted) {
    machineRunning = false;
  }

  // DETEKTIEREN OB DER START KNOPF GEDRÜCKT WIRD:
  if (StartButton.switchedHigh()) {
    if (autoMode) {
      machineRunning = !machineRunning; //im Auto-Modus wird ein- oder ausgeschaltet
    }
    if (!autoMode) {
      machineRunning = true;
      stepCompleted = false; // im Step-Modus wird der nächste Schritt gestartet
    }
  }

  // ABFRAGEN DER BANDDETEKTIERUNG:
  bool strapDetected = !StrapDetectionSensor.requestButtonState();
  // IM AUTO MODUS FALLS KEIN BAND DETEKTIERT RIG AUSSCHALTEN
  if (!strapDetected && autoMode) {
    machineRunning = false;
    cycleStep = 0;
    ResetCylinderStates();	//alle Zylinder ausschalten
  }

  // LESEN DES POTENTIOMETERS UND BERECHNEN DER ABKÜHLZEIT
  int potVal = analogRead(CONTROLLINO_A4);
  coolTime = (3500 + (potVal * 14));

  //*****************************************************************************
  // MAIN CYCLE:
  //*****************************************************************************
  if (machineRunning && !stepCompleted) {
    switch (cycleStep) {

    case BremszylinderZurueckfahren:
      if (!EndSwitchLeft.requestButtonState()) { // Bremszylinder ist nicht in Ausgangslage
        BremsZylinder.set(1);
      } else {
        BremsZylinder.set(0);
        stepCompleted = true;
        cycleStep++;
      }
      break;

    case WippenhebelZiehen1:
      WippenhebelZylinder.stroke(1500, 1000);
      if (WippenhebelZylinder.stroke_completed()) {
        stepCompleted = true;
        cycleStep++;
      }
      break;

    case BandVorschieben:
      SpanntastenZylinder.stroke(450, 250);
      if (SpanntastenZylinder.stroke_completed()) {
        stepCompleted = true;
        cycleStep++;
      }
      break;

    case BandKlemmen:
      BandKlemmZylinder.stroke(500, 500);
      if (BandKlemmZylinder.stroke_completed()) {
        stepCompleted = true;
        cycleStep++;
      }
      break;

    case BandSpannen:
      SpanntastenZylinder.set(1);
      if (EndSwitchRight.requestButtonState()) {
        stepCompleted = true;
        cycleStep++;
      }
      break;

    case SpannzylinderLoesen:
      SpanntastenZylinder.stroke(800, 0);
      if (SpanntastenZylinder.stroke_completed()) {
        stepCompleted = true;
        cycleStep++;
      }
      break;

    case BandSchneiden:
      MesserZylinder.stroke(2000, 2000);
      if (MesserZylinder.stroke_completed()) {
        stepCompleted = true;
        cycleStep++;
      }
      break;

    case Schweissen:
      SchweisstastenZylinder.stroke(500, coolTime);
      if (SchweisstastenZylinder.stroke_completed()) {
        stepCompleted = true;
        cycleStep++;
      }
      break;

    case WippenhebelZiehen2:
      WippenhebelZylinder.stroke(1500, 1000);
      if (WippenhebelZylinder.stroke_completed()) {
        stepCompleted = true;
        cycleStep++;
      }
      break;

    case BandklemmeLoesen:
      BandKlemmZylinder.stroke(300, 1000);
      if (BandKlemmZylinder.stroke_completed()) {
        stepCompleted = true;
        cycleStep = 0;
      }
      break;
    }
  }
}
