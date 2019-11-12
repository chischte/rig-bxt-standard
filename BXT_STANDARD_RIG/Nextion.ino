/*
 * *****************************************************************************
 * nextion.ino
 * configuration of the Nextion touch display
 * Michael Wettstein
 * November 2018, Zürich
 * *****************************************************************************
 * an XLS-sheet to generate Nextion events can be found here:
 * https://github.com/chischte/user-interface/NEXTION/
 * *****************************************************************************
 * CONFIGURING THE LIBRARY:
 * Include the nextion library (the official one) https://github.com/itead/ITEADLIB_Arduino_Nextion
 * Make sure you edit the NexConfig.h file on the library folder to set the correct serial port for the display.
 * By default it's set to Serial1, which most arduino boards don't have.
 * Change "#define nexSerial Serial1" to "#define nexSerial Serial" if you are using arduino uno, nano, etc.
 * *****************************************************************************
 * NEXTION SWITCH STATES LIST
 * Every nextion switch button needs a switchstate variable (bool)
 * to update the screen after a page change (all buttons)
 * to control switchtoggle (dualstate buttons)
 * to prevent screen flickering (momentary buttons)
 * *****************************************************************************
 * VARIOUS COMMANDS:
 * Serial2.print("click bt1,1");//CLICK BUTTON
 * send_to_nextion();
 * A switch (Dual State Button)will be toggled with this command, a Button will be set permanently pressed)
 * Serial2.print("click b3,0"); releases a push button again, has no effect on a Dual State Button
 * send_to_nextion();
 * Serial2.print("vis t0,0");//HIDE OBJECT
 * send_to_nextion();
 * *****************************************************************************
 */

//******************************************************************************
// DECLARATION OF VARIABLES:
//******************************************************************************
int CurrentPage;
byte nexPrevCycleStep;

// SWITCHSTATES:
bool nexStateKlemmzylinder;
bool nexStateWippenhebel;
bool nexStateSpanntaste;
bool nexStateSchlittenZylinder;
bool nexStateMesserzylinder;
bool nex_state_ZylRevolverschieber;
bool nexStateMachineRunning;
bool nex_state_strapDetected;
bool nexPrevStepMode = true;
//******************************************************************************
bool stopwatch_running;
bool resetStopwatchActive;
unsigned int stopped_button_pushtime;
long nexPrevCoolingTime;
long nexPrevShorttimeCounter;
long nexPrevLongtimeCounter;
long button_push_stopwatch;
long counterResetStopwatch;
//******************************************************************************
// DECLARATION OF OBJECTS TO BE READ FROM NEXTION
//******************************************************************************
// PAGE 0:
NexPage nex_page0 = NexPage(0, 0, "page0");
//PAGE 1 - LEFT SIDE:
NexPage nex_page1 = NexPage(1, 0, "page1");
NexButton nex_but_stepback = NexButton(1, 6, "b1");
NexButton nex_but_stepnxt = NexButton(1, 7, "b2");
NexButton nex_but_reset_cycle = NexButton(1, 5, "b0");
NexDSButton nex_switch_play_pause = NexDSButton(1, 3, "bt0");
NexDSButton nex_switch_mode = NexDSButton(1, 4, "bt1");
// PAGE 1 - RIGHT SIDE
NexDSButton nexBandKlemmZylinder = NexDSButton(1, 13, "bt3");
NexDSButton nexWippenZylinder = NexDSButton(1, 10, "bt5");
NexButton nexSchlittenZylinder = NexButton(1, 1, "b6");
NexButton nexSpanntastenZylinder = NexButton(1, 9, "b4");
NexButton nexMesserZylinder = NexButton(1, 14, "b5");
NexButton nexSchweisstastenZylinder = NexButton(1, 8, "b3");

// PAGE 2 - LEFT SIDE:
NexPage nex_page2 = NexPage(2, 0, "page2");
NexButton nex_but_slider1_left = NexButton(2, 5, "b1");
NexButton nex_but_slider1_right = NexButton(2, 6, "b2");

// PAGE 2 - RIGHT SIDE:
NexButton nex_but_reset_shorttimeCounter = NexButton(2, 16, "b4");
//*****************************************************************************
// END OF OBJECT DECLARATION
//*****************************************************************************
char buffer[100] = { 0 }; // This is needed only if you are going to receive a text from the display. You can remove it otherwise.
//*****************************************************************************
// TOUCH EVENT LIST //DECLARATION OF TOUCH EVENTS TO BE MONITORED
//*****************************************************************************
NexTouch *nex_listen_list[] = { //
            // PAGE 0:
            &nex_page0,
            // PAGE 1 LEFT:
            &nex_page1, &nex_but_stepback, &nex_but_stepnxt, &nex_but_reset_cycle,
            &nex_switch_play_pause, &nex_switch_mode,
            // PAGE 1 RIGHT:
            &nexMesserZylinder, &nexWippenZylinder, &nexBandKlemmZylinder, &nexSchlittenZylinder,
            &nexSpanntastenZylinder, &nexSchweisstastenZylinder,
            // PAGE 2 LEFT:
            &nex_page2, &nex_but_slider1_left, &nex_but_slider1_right,
            // PAGE 2 RIGHT:
            &nex_but_reset_shorttimeCounter,
            // END OF LISTEN LIST:
            NULL };
//*****************************************************************************
// END OF TOUCH EVENT LIST
//*****************************************************************************
void printOnTextField(String text, String textField) {
  Serial2.print(textField);
  Serial2.print(".txt=");
  Serial2.print("\"");
  Serial2.print(text);
  Serial2.print("\"");
  send_to_nextion();
}
void clearTextField(String textField) {
  Serial2.print(textField);
  Serial2.print(".txt=");
  Serial2.print("\"");
  Serial2.print("");    // erase text
  Serial2.print("\"");
  send_to_nextion();
}
void printOnValueField(int value, String valueField) {
  Serial2.print(valueField);
  Serial2.print(".val=");
  Serial2.print(value);
  send_to_nextion();
}
void send_to_nextion() {
  Serial2.write(0xff);
  Serial2.write(0xff);
  Serial2.write(0xff);
}
void showInfoField() {
  if (CurrentPage == 1) {
    Serial2.print("vis t4,1");
    send_to_nextion();
  }
}
void hideInfoField() {
  if (CurrentPage == 1) {
    Serial2.print("vis t4,0");
    send_to_nextion();
  }
}
//*****************************************************************************
void nextionSetup()
//*****************************************************************************
{
  Serial2.begin(9600);

  // RESET NEXTION DISPLAY: (refresh display after PLC restart)
  send_to_nextion(); // needed for unknown reasons
  Serial2.print("rest");
  send_to_nextion();

  //*****************************************************************************
  // INCREASE BAUD RATE DOES NOT WORK YET
  // CHECK IF LIBRARY ALWAY SWITCHES BACK TO 9600
  //*****************************************************************************
  /*
   delay(100);
   send_to_nextion();
   Serial2.print("baud=38400");
   send_to_nextion();
   delay(100);
   Serial2.begin(38400);
   */
  //*****************************************************************************
  // REGISTER THE EVENT CALLBACK FUNCTIONS
  //*****************************************************************************
  // PAGE 0 PUSH ONLY:
  nex_page0.attachPush(nex_page0PushCallback);
  // PAGE 1 PUSH ONLY:
  nex_page1.attachPush(nex_page1PushCallback);
  nex_but_stepback.attachPush(nex_but_stepbackPushCallback);
  nex_but_stepnxt.attachPush(nex_but_stepnxtPushCallback);
  nex_but_reset_cycle.attachPush(nex_but_reset_cyclePushCallback);
  nex_but_stepback.attachPush(nex_but_stepbackPushCallback);
  nex_but_stepnxt.attachPush(nex_but_stepnxtPushCallback);
  nex_switch_mode.attachPush(nex_switch_modePushCallback);
  nex_switch_play_pause.attachPush(nex_switch_play_pausePushCallback);
  nexWippenZylinder.attachPush(nexWippenZylinderPushCallback);
  nexBandKlemmZylinder.attachPush(nexBandKlemmZylinderPushCallback);
  // PAGE 1 PUSH AND POP:
  nexSpanntastenZylinder.attachPush(nexSpanntastenZylinderPushCallback);
  nexSpanntastenZylinder.attachPop(nexSpanntastenZylinderPopCallback);
  nexSchweisstastenZylinder.attachPush(nexSchweisstastenZylinderPushCallback);
  nexSchweisstastenZylinder.attachPop(nexSchweisstastenZylinderPopCallback);
  nexMesserZylinder.attachPush(nexMesserZylinderPushCallback);
  nexMesserZylinder.attachPop(nexMesserZylinderPopCallback);
  nexSchlittenZylinder.attachPush(nexSchlittenZylinderPushCallback);
  nexSchlittenZylinder.attachPop(nexSchlittenZylinderPopCallback);
  // PAGE 2 PUSH ONLY:
  nex_page2.attachPush(nex_page2PushCallback);
  nex_but_slider1_left.attachPush(nex_but_slider1_leftPushCallback);
  nex_but_slider1_right.attachPush(nex_but_slider1_rightPushCallback);
  // PAGE 2 PUSH AND POP:
  nex_but_reset_shorttimeCounter.attachPush(nex_but_reset_shorttimeCounterPushCallback);
  nex_but_reset_shorttimeCounter.attachPop(nex_but_reset_shorttimeCounterPopCallback);

  //*****************************************************************************
  // END OF REGISTER
  //*****************************************************************************
  delay(2000);
  sendCommand("page 1");  //SWITCH NEXTION TO PAGE X
  send_to_nextion();

}  // END OF NEXTION SETUP
//*****************************************************************************
void NextionLoop()
//*****************************************************************************
{
  nexLoop(nex_listen_list); //check for any touch event
  //*****************************************************************************
  if (CurrentPage == 1) {
    //*******************
    // PAGE 1 - LEFT SIDE:
    //*******************
    // UPDATE CYCLE NAME:
    if (nexPrevCycleStep != stateController.currentCycleStep()) {
      nexPrevCycleStep = stateController.currentCycleStep();
      printOnTextField(
              (stateController.currentCycleStep() + 1)
                      + (" " + cycleName[stateController.currentCycleStep()]), "t0");
    }
    // UPDATE SWITCHSTATE "PLAY"/"PAUSE":
    if (nexStateMachineRunning != stateController.machineRunning()) {
      Serial2.print("click bt0,1");
      send_to_nextion();
      nexStateMachineRunning = stateController.machineRunning();
    }

    // UPDATE SWITCHSTATE "STEP"/"AUTO"-MODE:
    if (nexPrevStepMode != stateController.stepMode()) {
      Serial2.print("click bt1,1");
      send_to_nextion();
      nexPrevStepMode = stateController.stepMode();
    }

    // DISPLAY IF MAGAZINE IS EMPTY:
    if (nex_state_strapDetected != strapDetected) {
      if (!strapDetected) {
        showInfoField();
        printOnTextField("MAGAZIN LEER!", "t4");
      } else {
        clearTextField("t4");
        hideInfoField();
      }
      nex_state_strapDetected = strapDetected;
    }
    //*******************
    // PAGE 1 - RIGHT SIDE:
    //*******************

    // UPDATE SWITCHBUTTON (dual state):
    if (BandKlemmZylinder.request_state() != nexStateKlemmzylinder) {
      Serial2.print("click bt3,1");
      send_to_nextion();
      nexStateKlemmzylinder = !nexStateKlemmzylinder;
    }
    // UPDATE SWITCHBUTTON (dual state):
    if (WippenhebelZylinder.request_state() != nexStateWippenhebel) {
      Serial2.print("click bt5,1");
      send_to_nextion();
      nexStateWippenhebel = !nexStateWippenhebel;
    }

    // UPDATE BUTTON (momentary):
    if (SchlittenZylinder.request_state() != nexStateSchlittenZylinder) {
      if (SchlittenZylinder.request_state()) {
        Serial2.print("click b6,1");
      } else {
        Serial2.print("click b6,0");
      }
      send_to_nextion();
      nexStateSchlittenZylinder = SchlittenZylinder.request_state();
    }

    // UPDATE BUTTON (momentary):
    if (SpanntastenZylinder.request_state() != nexStateSpanntaste) {
      if (SpanntastenZylinder.request_state()) {
        Serial2.print("click b4,1");
      } else {
        Serial2.print("click b4,0");
      }
      send_to_nextion();
      nexStateSpanntaste = SpanntastenZylinder.request_state();
    }

    // UPDATE BUTTON (momentary):
    if (MesserZylinder.request_state() != nexStateMesserzylinder) {
      if (MesserZylinder.request_state()) {
        Serial2.print("click b5,1");
      } else {
        Serial2.print("click b5,0");
      }
      send_to_nextion();
      nexStateMesserzylinder = MesserZylinder.request_state();
    }

    // UPDATE BUTTON (momentary):
    if (SchweisstastenZylinder.request_state() != nex_state_ZylRevolverschieber) {
      if (SchweisstastenZylinder.request_state()) {
        Serial2.print("click b3,1");
      } else {
        Serial2.print("click b3,0");
      }
      send_to_nextion();
      nex_state_ZylRevolverschieber = SchweisstastenZylinder.request_state();
    }

  }    //END PAGE 1
//*****************************************************************************
  if (CurrentPage == 2) {
    //*******************
    // PAGE 2 - LEFT SIDE
    //*******************

    if (nexPrevCoolingTime != eepromCounter.getValue(coolingTime)) {
      printOnTextField(String(eepromCounter.getValue(coolingTime)) + " ms", "t4");
      nexPrevCoolingTime = eepromCounter.getValue(coolingTime);
    }

    //*******************
    // PAGE 2 - RIGHT SIDE
    //*******************
    if (nexPrevLongtimeCounter != eepromCounter.getValue(longtimeCounter)) {

      printOnTextField(String(eepromCounter.getValue(longtimeCounter)), "t10");
      //printOnTextField((eepromCounter.getValue(longtimeCounter) + ("")), "t10");
      nexPrevLongtimeCounter = eepromCounter.getValue(longtimeCounter);
    }
    if (nexPrevShorttimeCounter != eepromCounter.getValue(shorttimeCounter)) {
      printOnTextField(String(eepromCounter.getValue(shorttimeCounter)), "t12");
      nexPrevShorttimeCounter = eepromCounter.getValue(shorttimeCounter);
    }
    if (resetStopwatchActive) {
      if (millis() - counterResetStopwatch > 5000) {
        eepromCounter.set(longtimeCounter, 0);
      }
    }
  }    // END PAGE 2
}    // END OF NEXTION LOOP
//*****************************************************************************
// TOUCH EVENT FUNCTIONS //PushCallback = Press event //PopCallback = Release event
//*****************************************************************************
//*************************************************
// TOUCH EVENT FUNCTIONS PAGE 1 - LEFT SIDE
//*************************************************
void nex_switch_play_pausePushCallback(void *ptr) {
  stateController.toggleMachineRunningState();
  nexStateMachineRunning = !nexStateMachineRunning;
}
void nex_switch_modePushCallback(void *ptr) {
  if (stateController.autoMode()) {
    stateController.setStepMode();
  } else {
    stateController.setAutoMode();
  }
  nexPrevStepMode = stateController.stepMode();
}
void nex_but_stepbackPushCallback(void *ptr) {
  if (stateController.currentCycleStep() > 0) {
    stateController.setCycleStepTo(stateController.currentCycleStep() - 1);
  }
}
void nex_but_stepnxtPushCallback(void *ptr) {
  stateController.switchToNextStep();
}
void nex_but_reset_cyclePushCallback(void *ptr) {
  stateController.setResetMode(1);
  stateController.setStepMode();
  stateController.setRunAfterReset(0);
  clearTextField("t4");
}
//*************************************************
// TOUCH EVENT FUNCTIONS PAGE 1 - RIGHT SIDE
//*************************************************

void nexWippenZylinderPushCallback(void *ptr) {
  WippenhebelZylinder.toggle();
  nexStateWippenhebel = !nexStateWippenhebel;
}

void nexSpanntastenZylinderPushCallback(void *ptr) {
  SpanntastenZylinder.set(1);
}
void nexSpanntastenZylinderPopCallback(void *ptr) {
  SpanntastenZylinder.set(0);
}

void nexSchweisstastenZylinderPushCallback(void *ptr) {
  SchweisstastenZylinder.set(1);
}
void nexSchweisstastenZylinderPopCallback(void *ptr) {
  SchweisstastenZylinder.set(0);
}

void nexBandKlemmZylinderPushCallback(void *ptr) {
  BandKlemmZylinder.toggle();
  nexStateKlemmzylinder = !nexStateKlemmzylinder;
}

void nexMesserZylinderPushCallback(void *ptr) {
  MesserZylinder.set(1);
}
void nexMesserZylinderPopCallback(void *ptr) {
  MesserZylinder.set(0);
}

void nexSchlittenZylinderPushCallback(void *ptr) {
  SchlittenZylinder.set(1);
}
void nexSchlittenZylinderPopCallback(void *ptr) {
  SchlittenZylinder.set(0);
}
//*************************************************
// TOUCH EVENT FUNCTIONS PAGE 2 - LEFT SIDE
//*************************************************
void nex_but_slider1_leftPushCallback(void *ptr) {
  eepromCounter.set(coolingTime, eepromCounter.getValue(coolingTime) - 50);
  if (eepromCounter.getValue(coolingTime) < 0) {
    eepromCounter.set(coolingTime, 0);
  }
}
void nex_but_slider1_rightPushCallback(void *ptr) {
  eepromCounter.set(coolingTime, eepromCounter.getValue(coolingTime) + 50);
  if (eepromCounter.getValue(coolingTime) > 5000) {
    eepromCounter.set(coolingTime, 5000);
  }
}
//*************************************************
// TOUCH EVENT FUNCTIONS PAGE 2 - RIGHT SIDE
//*************************************************
void nex_but_reset_shorttimeCounterPushCallback(void *ptr) {
  eepromCounter.set(shorttimeCounter, 0);
// RESET LONGTIME COUNTER IF RESET BUTTON IS PRESSED LONG ENOUGH:
  counterResetStopwatch = millis();
  resetStopwatchActive = true;
}
void nex_but_reset_shorttimeCounterPopCallback(void *ptr) {
  resetStopwatchActive = false;
}
//*************************************************
// TOUCH EVENT FUNCTIONS PAGE CHANGES
//*************************************************
void nex_page0PushCallback(void *ptr) {
  CurrentPage = 0;
}
void nex_page1PushCallback(void *ptr) {
  CurrentPage = 1;
  hideInfoField();

// REFRESH BUTTON STATES:
  nexPrevCycleStep = !stateController.currentCycleStep();
  nexPrevStepMode = true;
  nexStateKlemmzylinder = 0;
  nexStateWippenhebel = 0;
  nexStateSpanntaste = 0;
  nexStateSchlittenZylinder = 0;
  nexStateMesserzylinder = 0;
  nex_state_ZylRevolverschieber = 0;
  nexStateMachineRunning = 0;
  nex_state_strapDetected = !strapDetected;
}
void nex_page2PushCallback(void *ptr) {
  CurrentPage = 2;
// REFRESH BUTTON STATES:
  nexPrevCoolingTime = 0;
  nexPrevShorttimeCounter = 0;
  nexPrevLongtimeCounter = 0;
}
//*************************************************
// END OF TOUCH EVENT FUNCTIONS
//*************************************************
