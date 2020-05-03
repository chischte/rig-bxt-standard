#include <Arduino.h>
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
#include <Insomnia.h>       // https://github.com/chischte/insomnia-delay-library
#include <EEPROM_Counter.h> // https://github.com/chischte/eeprom-counter-library
#include <EEPROM_Logger.h>  // https://github.com/chischte/eeprom-logger-library.git
#include <Nextion.h>        // https://github.com/itead/ITEADLIB_Arduino_Nextion

#include <StateController.h> // contains all machine states

//******************************************************************************
// DEFINE NAMES AND SEQUENCE OF STEPS FOR THE MAIN CYCLE:
//******************************************************************************
enum mainCycleSteps
{
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
String cycleName[] = {"WIPPENHEBEL", "KLEMME LOESEN", "ZURUECKFAHREN", "BAND VORSCHIEBEN",
                      "SCHNEIDEN", "KLEMMEN", "SPANNEN", "SCHWEISSEN"};

//******************************************************************************
// DEFINE NAMES AND SET UP VARIABLES FOR THE CYCLE COUNTER:
//******************************************************************************
enum counter
{
  longtimeCounter,  //
  shorttimeCounter, //
  coolingTime,      // [s]
  endOfCounterEnum
};

int counterNoOfValues = endOfCounterEnum;

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
 * Include the nextion library (the official one):
 * https://github.com/itead/ITEADLIB_Arduino_Nextion
 * Make sure you edit the NexConfig.h file on the library folder to set the
 * correct serial port for the display.
 * By default it's set to Serial1, which most arduino boards don't have.
 * Change "#define nexSerial Serial1" to "#define nexSerial Serial"
 * if you are using arduino uno, nano, etc.
 * *****************************************************************************
 * NEXTION SWITCH STATES LIST
 * Every nextion switch button needs a switchstate variable (bool)
 * to update the screen after a page change (all buttons)
 * to control switchtoggle (dualstate buttons)
 * to prevent screen flickering (momentary buttons)
 * *****************************************************************************
 * VARIOUS COMMANDS:
 * CLICK A BUTTON:
 * Serial2.print("click bt1,1");
 * send_to_nextion();
 * A switch (Dual State Button)will be toggled with this command
 * a Button will be set permanently pressed)
 * Serial2.print("click b3,0");
 * send_to_nextion();
 * releases a push button again but has no effect on a dual state Button
 * HIDE AN OBJECT:
 * Serial2.print("vis t0,0");
 * send_to_nextion();
 * *****************************************************************************
 */

//******************************************************************************
// DECLARATION OF VARIABLES:
//******************************************************************************
int CurrentPage;
byte nexPrevCycleStep;
byte errorLogPage = 0;

// SWITCHSTATES:
bool nexStateKlemmzylinder;
bool nexStateWippenhebel;
bool nexStateSpanntaste;
bool nexStateSchlittenZylinder;
bool nexStateMesserzylinder;
bool nexStateMachineRunning;
bool nexStateSchweisstaste;
bool nexPrevStepMode = true;
//******************************************************************************
bool resetStopwatchActive;
unsigned int stoppedButtonPushtime;
long nexPrevCoolingTime;
long nexPrevShorttimeCounter;
long nexPrevLongtimeCounter;
long buttonPushStopwatch;
long counterResetStopwatch;
//******************************************************************************
// DECLARATION OF OBJECTS TO BE READ FROM NEXTION
//******************************************************************************
// PAGE 0:
NexPage nexPage0 = NexPage(0, 0, "page0");
//PAGE 1 - LEFT SIDE:
NexPage nexPage1 = NexPage(1, 0, "page1");
NexButton nexButStepback = NexButton(1, 6, "b1");
NexButton nexButStepNext = NexButton(1, 7, "b2");
NexButton nexButResetCycle = NexButton(1, 5, "b0");
NexDSButton NexSwitchPlayPause = NexDSButton(1, 3, "bt0");
NexDSButton NexSwitchMode = NexDSButton(1, 4, "bt1");
// PAGE 1 - RIGHT SIDE
NexDSButton nexBandKlemmZylinder = NexDSButton(1, 13, "bt3");
NexDSButton nexWippenZylinder = NexDSButton(1, 10, "bt5");
NexButton nexSchlittenZylinder = NexButton(1, 1, "b6");
NexButton nexSpanntastenZylinder = NexButton(1, 9, "b4");
NexButton nexMesserZylinder = NexButton(1, 14, "b5");
NexButton nexSchweisstastenZylinder = NexButton(1, 8, "b3");

// PAGE 2 - LEFT SIDE:
NexPage nexPage2 = NexPage(2, 0, "page2");
NexButton nexButSlider1Left = NexButton(2, 4, "b1");
NexButton nexButSlider1Right = NexButton(2, 5, "b2");

// PAGE 2 - RIGHT SIDE:
NexButton nexButResetShorttimeCounter = NexButton(2, 11, "b4");

// PAGE 3 - ERROR LOG:
NexPage nexPage3 = NexPage(3, 0, "page3");
NexButton nexButNextLog = NexButton(3, 48, "b3");
NexButton nexButResetLog = NexButton(3, 1, "b1");
NexButton nexButPrevLog = NexButton(3, 47, "b2");
//*****************************************************************************
// END OF OBJECT DECLARATION
//*****************************************************************************
char buffer[100] = {0}; // This is needed only if you are going to receive a text from the display. You can remove it otherwise.
//*****************************************************************************
// TOUCH EVENT LIST //DECLARATION OF TOUCH EVENTS TO BE MONITORED
//*****************************************************************************
NexTouch *nex_listen_list[] = { //
    // PAGE 0:
    &nexPage0,
    // PAGE 1 LEFT:
    &nexPage1, &nexButStepback, &nexButStepNext, &nexButResetCycle, &NexSwitchPlayPause,
    &NexSwitchMode,
    // PAGE 1 RIGHT:
    &nexMesserZylinder, &nexWippenZylinder, &nexBandKlemmZylinder, &nexSchlittenZylinder,
    &nexSpanntastenZylinder, &nexSchweisstastenZylinder,
    // PAGE 2 LEFT:
    &nexPage2, &nexButSlider1Left, &nexButSlider1Right,
    // PAGE 2 RIGHT:
    &nexButResetShorttimeCounter,
    // PAGE 3 - ERROR LOG:
    &nexPage3, &nexButNextLog, &nexButResetLog, &nexButPrevLog,
    // END OF LISTEN LIST:
    NULL};
//*****************************************************************************
// END OF TOUCH EVENT LIST
//*****************************************************************************

//******************************************************************************
// DEFINE NAMES AND SET UP VARIABLES FOR THE ERROR LOGGER:
//******************************************************************************
enum logger
{
  emptyLog, // marks empty logs
  toolResetError,
  shortTimeoutError,
  longTimeoutError,
  shutDownError,
  magazineEmpty,
  manualOn,
  manualOff
};

String errorCode[] = { //
    "n.a.",            //
    "STEUERUNG EIN",   //
    "AUTO RESET",      //
    "AUTO PAUSE",      //
    "AUTO STOP",       //
    "BAND LEER",       //
    "MANUELL START",   //
    "MANUELL STOP"};   //

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
long mergeCurrentTime()
{
  long mergedTime = 0;

  // GET THE CURRENT TIME:
  int hour = Controllino_GetHour();
  int minute = Controllino_GetMinute();
  long second = Controllino_GetSecond();

  // MERGE (HOUR 5bit / MINUTE 6bit / SECOND 6bit):
  mergedTime = hour;
  mergedTime = (mergedTime << 6) | minute; // move 6 bits minute
  mergedTime = (mergedTime << 6) | second; // move 6 bits second
  return mergedTime;
}

void writeErrorLog(byte errorCode)
{
  long cycleNumber = eepromCounter.getValue(shorttimeCounter);
  long logTime = mergeCurrentTime();
  errorLogger.writeLog(cycleNumber, logTime, errorCode);
}
void sendToNextion()
{
  Serial2.write(0xff);
  Serial2.write(0xff);
  Serial2.write(0xff);
}
void showInfoField()
{
  if (CurrentPage == 1)
  {
    Serial2.print("vis t4,1");
    sendToNextion();
  }
}
void hideInfoField()
{
  if (CurrentPage == 1)
  {
    Serial2.print("vis t4,0");
    sendToNextion();
  }
}

String splitLoggedTime(long loggedTime)
{
  // SPLIT (HOUR 5bit / MINUTE 6bit / SECOND 6bit):
  byte bitMaskHour = 0b00011111;
  byte bitMaskMinute = 0b111111;
  byte bitMaskSecond = 0b111111;
  int hour = (loggedTime >> 12) & bitMaskHour;
  int minute = (loggedTime >> 6) & bitMaskMinute;
  int second = (loggedTime & bitMaskSecond);
  String hourString = String(hour);
  if (hour < 10)
  {
    hourString = "0" + hourString;
  }
  String minuteString = String(minute);
  if (minute < 10)
  {
    minuteString = "0" + minuteString;
  }
  String secondString = String(second);
  if (second < 10)
  {
    secondString = "0" + secondString;
  }
  String loggedTimeString = hourString + ":" + minuteString;
  return loggedTimeString;
}
void printOnTextField(String text, String textField)
{
  Serial2.print(textField);
  Serial2.print(".txt=");
  Serial2.print("\"");
  Serial2.print(text);
  Serial2.print("\"");
  sendToNextion();
}
void printErrorLog(byte logNumber, byte lineNumber)
{
  static byte numberOfFirstField = 7;
  static byte fieldsPerLog = 4;
  byte fieldNumber;
  fieldNumber = numberOfFirstField + (lineNumber * fieldsPerLog);

  //PRINT LOG NUMBER:
  String fieldNumberString = "t" + String(fieldNumber);
  printOnTextField(String(logNumber + 1), fieldNumberString);
  fieldNumber++;

  // GET STRUCT OF LOGGER LIBRARY
  EEPROM_Logger::LogStruct logStruct;
  // GET VALUES FROM ERROR LOGGER
  logStruct = errorLogger.readLog(logNumber);

  //PRINT CYCLE NUMBER:
  fieldNumberString = "t" + String(fieldNumber);
  printOnTextField(String(logStruct.logCycleNumber), fieldNumberString);
  fieldNumber++;

  //PRINT ERROR TIME:
  fieldNumberString = "t" + String(fieldNumber);
  String timeString = splitLoggedTime(logStruct.logCycleTime);
  printOnTextField(timeString, fieldNumberString);
  fieldNumber++;

  //PRINT ERROR NAME:
  fieldNumberString = "t" + String(fieldNumber);
  printOnTextField(errorCode[logStruct.logErrorCode], fieldNumberString);
  fieldNumber++;
}
void printLogPage()
{
  static byte logsPerPage = 10;

  for (int i = 0; i < logsPerPage; i++)
  {
    byte logNumber = errorLogPage * logsPerPage + i;
    byte lineNumber = i;
    printErrorLog(logNumber, lineNumber);
  }
}

void resetCylinderStates()
{
  SchlittenZylinder.set(0);
  SpanntastenZylinder.set(0);
  SchweisstastenZylinder.set(0);
  WippenhebelZylinder.set(0);
  MesserZylinder.set(0);
  BandKlemmZylinder.set(0);
}
void stopTestRig()
{
  resetCylinderStates();
  stateController.setStepMode();
  stateController.setMachineRunningState(false);
}
void clearTextField(String textField)
{
  Serial2.print(textField);
  Serial2.print(".txt=");
  Serial2.print("\"");
  Serial2.print(""); // erase text
  Serial2.print("\"");
  sendToNextion();
}
void printOnValueField(int value, String valueField)
{
  Serial2.print(valueField);
  Serial2.print(".val=");
  Serial2.print(value);
  sendToNextion();
}
void printCurrentStep()
{
  Serial.print(stateController.currentCycleStep());
  Serial.print(" ");
  Serial.println(cycleName[stateController.currentCycleStep()]);
}

void runTimeoutManager()
{

  static byte timeoutCounter;
  // RESET TIMOUT TIMER:
  if (EndSwitchRight.switchedHigh())
  {
    resetTimeout.resetTime();
    timeoutCounter = 0;
  }
  // DETECT TIMEOUT:
  if (!timeoutDetected)
  {
    if (resetTimeout.timedOut())
    {
      timeoutCounter++;
      timeoutDetected = 1;
    }
  }
  else
  {
    resetTimeout.resetTime();
  }
  // 1st TIMEOUT - RESET IMMEDIATELY:
  if (timeoutDetected && timeoutCounter == 1)
  {
    writeErrorLog(shortTimeoutError);
    Serial.println("TIMEOUT 1 > RESET");
    stateController.setRunAfterReset(1);
    stateController.setResetMode(1);
    timeoutDetected = 0;
  }
  // 2nd TIMEOUT - WAIT AND RESET:
  if (timeoutDetected && timeoutCounter == 2)
  {
    static byte subStep = 1;
    if (subStep == 1)
    {
      Serial.println("TIMEOUT 2 > WAIT & RESET");
      errorBlinkState = 1;
      blinkDelay = 600;
      writeErrorLog(longTimeoutError);
      subStep++;
    }
    if (subStep == 2)
    {
      showInfoField();
      int remainingPause = resetDelay.remainingDelayTime() / 1000;
      printOnTextField("TIMEOUT " + String(remainingPause) + " s", "t4");
      if (resetDelay.delayTimeUp(3 * 60 * 1000L))
      {
        errorBlinkState = 0;
        stateController.setRunAfterReset(1);
        stateController.setResetMode(1);
        timeoutDetected = 0;
        subStep = 1;
      }
    }
  }
  // 3rd TIMEOUT - SHUT OFF:
  if (timeoutDetected && timeoutCounter == 3)
  {
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

void resetTestRig()
{
  static byte resetStage = 1;
  stateController.setMachineRunningState(0);

  if (resetStage == 1)
  {
    resetCylinderStates();
    errorBlinkState = 0;
    clearTextField("t4");
    hideInfoField();
    stateController.setCycleStepTo(0);
    resetStage++;
  }
  if (resetStage == 2)
  {
    WippenhebelZylinder.stroke(1500, 0);
    if (WippenhebelZylinder.stroke_completed())
    {
      resetStage++;
    }
  }
  if (resetStage == 3)
  {
    stateController.setCycleStepTo(0);
    stateController.setResetMode(0);
    bool runAfterReset = stateController.runAfterReset();
    stateController.setMachineRunningState(runAfterReset);
    resetStage = 1;
  }
}

void generateErrorBlink()
{
  if (errorBlinkState)
  {
    if (errorBlinkTimer.delayTimeUp(blinkDelay))
    {
      ErrorBlinkRelay.toggle();
      EmergencyLight.toggle();
    }
  }
  else
  {
    ErrorBlinkRelay.set(0);
    EmergencyLight.set(1);
  }
}

void runMainTestCycle()
{
  int cycleStep = stateController.currentCycleStep();
  static byte subStep = 1;

  switch (cycleStep)
  {

  case WippenhebelZiehen:
    WippenhebelZylinder.stroke(1500, 1000);
    if (WippenhebelZylinder.stroke_completed())
    {
      stateController.switchToNextStep();
    }
    break;

  case BandklemmeLoesen:
    //BandKlemmZylinder.set(0);
    BandKlemmZylinder.stroke(0, 1000); // wait a short while for the release
    if (BandKlemmZylinder.stroke_completed())
    {
      stateController.switchToNextStep();
    }
    break;

  case SchlittenZurueckfahren:
    SchlittenZylinder.stroke(2000, 0);
    if (SchlittenZylinder.stroke_completed())
    {
      stateController.switchToNextStep();
    }
    break;

  case BandVorschieben:
    SpanntastenZylinder.stroke(1300, 0);
    if (SpanntastenZylinder.stroke_completed())
    {
      stateController.switchToNextStep();
    }
    break;

  case BandKlemmen:
    BandKlemmZylinder.set(1);
    stateController.switchToNextStep();
    break;

  case BandSpannen:

    if (subStep == 1)
    {
      SpanntastenZylinder.set(1);
      if (EndSwitchRight.requestButtonState())
      {
        subStep = 2;
      }
    }
    if (subStep == 2)
    {
      SpanntastenZylinder.stroke(800, 0); // kurze Pause für Spannkraftaufbau
      if (SpanntastenZylinder.stroke_completed())
      {
        subStep = 1;
        stateController.switchToNextStep();
      }
    }
    break;

  case BandSchneiden:
    MesserZylinder.stroke(2500, 2000);
    if (MesserZylinder.stroke_completed())
    {
      stateController.switchToNextStep();
    }
    break;

  case Schweissen:
    if (subStep == 1)
    {
      SchweisstastenZylinder.stroke(500, 0);
      if (SchweisstastenZylinder.stroke_completed())
      {
        subStep++;
      }
    }
    if (subStep == 2)
    {
      unsigned long pauseTime = eepromCounter.getValue(coolingTime) * 1000;
      showInfoField();
      if (coolingDelay.delayTimeUp(pauseTime))
      {
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

//*****************************************************************************
// TOUCH EVENT FUNCTIONS //PushCallback = Press event //PopCallback = Release event
//*****************************************************************************
//*************************************************
// TOUCH EVENT FUNCTIONS PAGE 1 - LEFT SIDE
//*************************************************
void nexSwitchPlayPausePushCallback(void *ptr)
{
  stateController.toggleMachineRunningState();
  nexStateMachineRunning = !nexStateMachineRunning;

  // CREATE LOG ENTRY IF AUTO-RUN STARTS OR STOPPS
  if (stateController.autoMode())
  {
    if (stateController.machineRunning())
    {
      writeErrorLog(manualOn);
    }
    else
    {
      writeErrorLog(manualOff);
    }
  }
}
void nexSwitchModePushCallback(void *ptr)
{
  if (stateController.autoMode())
  {
    stateController.setStepMode();
  }
  else
  {
    stateController.setAutoMode();
  }
  nexPrevStepMode = stateController.stepMode();
}
void nexButStepbackPushCallback(void *ptr)
{
  if (stateController.currentCycleStep() > 0)
  {
    stateController.setCycleStepTo(stateController.currentCycleStep() - 1);
  }
}
void nexButStepNextPushCallback(void *ptr)
{
  stateController.switchToNextStep();
}
void nexButResetCyclePushCallback(void *ptr)
{
  timeoutDetected = 0;
  stateController.setResetMode(1);
  stateController.setStepMode();
  stateController.setRunAfterReset(0);
  clearTextField("t4");
  hideInfoField();
}
//*************************************************
// TOUCH EVENT FUNCTIONS PAGE 1 - RIGHT SIDE
//*************************************************

void nexWippenZylinderPushCallback(void *ptr)
{
  WippenhebelZylinder.toggle();
  nexStateWippenhebel = !nexStateWippenhebel;
}

void nexSpanntastenZylinderPushCallback(void *ptr)
{
  SpanntastenZylinder.set(1);
}
void nexSpanntastenZylinderPopCallback(void *ptr)
{
  SpanntastenZylinder.set(0);
}

void nexSchweisstastenZylinderPushCallback(void *ptr)
{
  SchweisstastenZylinder.set(1);
}
void nexSchweisstastenZylinderPopCallback(void *ptr)
{
  SchweisstastenZylinder.set(0);
}

void nexBandKlemmZylinderPushCallback(void *ptr)
{
  BandKlemmZylinder.toggle();
  nexStateKlemmzylinder = !nexStateKlemmzylinder;
}

void nexMesserZylinderPushCallback(void *ptr)
{
  MesserZylinder.set(1);
}
void nexMesserZylinderPopCallback(void *ptr)
{
  MesserZylinder.set(0);
}

void nexSchlittenZylinderPushCallback(void *ptr)
{
  SchlittenZylinder.set(1);
}
void nexSchlittenZylinderPopCallback(void *ptr)
{
  SchlittenZylinder.set(0);
}
//*************************************************
// TOUCH EVENT FUNCTIONS PAGE 2 - LEFT SIDE
//*************************************************
void nexButSlider1LeftPushCallback(void *ptr)
{
  byte increment = 10;
  if (eepromCounter.getValue(coolingTime) <= 20)
  {
    increment = 5;
  }
  if (eepromCounter.getValue(coolingTime) <= 10)
  {
    increment = 1;
  }
  eepromCounter.set(coolingTime, eepromCounter.getValue(coolingTime) - increment);
  if (eepromCounter.getValue(coolingTime) < 4)
  {
    eepromCounter.set(coolingTime, 4);
  }
  resetTimeout.setTime((eepromCounter.getValue(coolingTime) + cycleTimeInSeconds) * 1000);
}
void nexButSlider1RightPushCallback(void *ptr)
{
  byte increment = 10;

  if (eepromCounter.getValue(coolingTime) < 20)
  {
    increment = 5;
  }
  if (eepromCounter.getValue(coolingTime) < 10)
  {
    increment = 1;
  }
  eepromCounter.set(coolingTime, eepromCounter.getValue(coolingTime) + increment);
  if (eepromCounter.getValue(coolingTime) > 120)
  {
    eepromCounter.set(coolingTime, 120);
  }
  resetTimeout.setTime((eepromCounter.getValue(coolingTime) + cycleTimeInSeconds) * 1000);
}
//*************************************************
// TOUCH EVENT FUNCTIONS PAGE 2 - RIGHT SIDE
//*************************************************
void nexButResetShorttimeCounterPushCallback(void *ptr)
{
  eepromCounter.set(shorttimeCounter, 0);

  // RESET LONGTIME COUNTER IF RESET BUTTON IS PRESSED LONG ENOUGH:
  counterResetStopwatch = millis();
  resetStopwatchActive = true;
}
void nexButResetShorttimeCounterPopCallback(void *ptr)
{
  resetStopwatchActive = false;
}
//*************************************************
// TOUCH EVENT FUNCTIONS PAGE 3 - ERROR LOG
//*************************************************
void nexButNextLogPushCallback(void *ptr)
{
  static byte noOfLogsPerPage = 10;
  byte maxLogPage = loggerNoOfLogs / noOfLogsPerPage;
  if (errorLogPage < (maxLogPage - 1))
  {
    errorLogPage++;
    printLogPage();
  }
}
void nexButResetLogPushCallback(void *ptr)
{
  nexResetButtonTimeout.setTime(1500);
  nexResetButtonTimeout.setActive(1);
}
void nexButResetLogPopCallback(void *ptr)
{
  nexResetButtonTimeout.setActive(0);
}
void nexButPrevLogPushCallback(void *ptr)
{
  if (errorLogPage > 0)
    errorLogPage--;
  printLogPage();
}

//*************************************************
// TOUCH EVENT FUNCTIONS PAGE CHANGES
//*************************************************
void nexPage0PushCallback(void *ptr)
{
  CurrentPage = 0;
}
void nexPage1PushCallback(void *ptr)
{
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
  nexStateSchweisstaste = 0;
  nexStateMachineRunning = 0;
}
void nexPage2PushCallback(void *ptr)
{
  CurrentPage = 2;
  // REFRESH BUTTON STATES:
  nexPrevCoolingTime = 0;
  nexPrevShorttimeCounter = 0;
  nexPrevLongtimeCounter = 0;
}
void nexPage3PushCallback(void *ptr)
{
  CurrentPage = 3;
  nexResetButtonTimeout.setActive(0);
  errorLogPage = 0;
  printLogPage();
}
//*************************************************
// END OF TOUCH EVENT FUNCTIONS
//*************************************************

//*****************************************************************************
void nextionSetup()
//*****************************************************************************
{
  Serial2.begin(9600);

  // RESET NEXTION DISPLAY: (refresh display after PLC restart)
  sendToNextion(); // needed for unknown reasons
  Serial2.print("rest");
  sendToNextion();

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
  nexPage0.attachPush(nexPage0PushCallback);
  // PAGE 1 PUSH ONLY:
  nexPage1.attachPush(nexPage1PushCallback);
  nexButStepback.attachPush(nexButStepbackPushCallback);
  nexButStepNext.attachPush(nexButStepNextPushCallback);
  nexButResetCycle.attachPush(nexButResetCyclePushCallback);
  nexButStepback.attachPush(nexButStepbackPushCallback);
  nexButStepNext.attachPush(nexButStepNextPushCallback);
  NexSwitchMode.attachPush(nexSwitchModePushCallback);
  NexSwitchPlayPause.attachPush(nexSwitchPlayPausePushCallback);
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
  nexPage2.attachPush(nexPage2PushCallback);
  nexButSlider1Left.attachPush(nexButSlider1LeftPushCallback);
  nexButSlider1Right.attachPush(nexButSlider1RightPushCallback);
  // PAGE 2 PUSH AND POP:
  nexButResetShorttimeCounter.attachPush(nexButResetShorttimeCounterPushCallback);
  nexButResetShorttimeCounter.attachPop(nexButResetShorttimeCounterPopCallback);
  // PAGE 3:
  nexPage3.attachPush(nexPage3PushCallback);
  nexButNextLog.attachPush(nexButNextLogPushCallback);
  nexButResetLog.attachPush(nexButResetLogPushCallback);
  nexButResetLog.attachPop(nexButResetLogPopCallback);
  nexButPrevLog.attachPush(nexButPrevLogPushCallback);
  //*****************************************************************************
  // END OF REGISTER
  //*****************************************************************************

  delay(2000);
  sendCommand("page 1"); // switch display to page x
  sendToNextion();

} // END OF NEXTION SETUP
//*****************************************************************************
void nextionLoop()
//*****************************************************************************
{
  nexLoop(nex_listen_list); // check for any touch event

  //*******************
  // PAGE 1 - LEFT SIDE:
  //*******************
  if (CurrentPage == 1)
  {

    // UPDATE CYCLE NAME:
    if (nexPrevCycleStep != stateController.currentCycleStep())
    {
      nexPrevCycleStep = stateController.currentCycleStep();
      printOnTextField(
          (stateController.currentCycleStep() + 1) + (" " + cycleName[stateController.currentCycleStep()]), "t0");
    }
    // UPDATE SWITCHSTATE "PLAY"/"PAUSE":
    if (nexStateMachineRunning != stateController.machineRunning())
    {
      Serial2.print("click bt0,1");
      sendToNextion();
      nexStateMachineRunning = stateController.machineRunning();
    }

    // UPDATE SWITCHSTATE "STEP"/"AUTO"-MODE:
    if (nexPrevStepMode != stateController.stepMode())
    {
      Serial2.print("click bt1,1");
      sendToNextion();
      nexPrevStepMode = stateController.stepMode();
    }

    // DISPLAY COOLING TIME:
    if (stateController.machineRunning())
    {
      if (stateController.currentCycleStep() == Schweissen)
      {

        int remainingPause = coolingDelay.remainingDelayTime() / 1000;
        printOnTextField(String(remainingPause) + " s", "t4");
      }
    }

    //*******************
    // PAGE 1 - RIGHT SIDE:
    //*******************

    // UPDATE SWITCHBUTTON (dual state):
    if (BandKlemmZylinder.request_state() != nexStateKlemmzylinder)
    {
      Serial2.print("click bt3,1");
      sendToNextion();
      nexStateKlemmzylinder = !nexStateKlemmzylinder;
    }
    // UPDATE SWITCHBUTTON (dual state):
    if (WippenhebelZylinder.request_state() != nexStateWippenhebel)
    {
      Serial2.print("click bt5,1");
      sendToNextion();
      nexStateWippenhebel = !nexStateWippenhebel;
    }

    // UPDATE BUTTON (momentary):
    if (SchlittenZylinder.request_state() != nexStateSchlittenZylinder)
    {
      if (SchlittenZylinder.request_state())
      {
        Serial2.print("click b6,1");
      }
      else
      {
        Serial2.print("click b6,0");
      }
      sendToNextion();
      nexStateSchlittenZylinder = SchlittenZylinder.request_state();
    }

    // UPDATE BUTTON (momentary):
    if (SpanntastenZylinder.request_state() != nexStateSpanntaste)
    {
      if (SpanntastenZylinder.request_state())
      {
        Serial2.print("click b4,1");
      }
      else
      {
        Serial2.print("click b4,0");
      }
      sendToNextion();
      nexStateSpanntaste = SpanntastenZylinder.request_state();
    }

    // UPDATE BUTTON (momentary):
    if (MesserZylinder.request_state() != nexStateMesserzylinder)
    {
      if (MesserZylinder.request_state())
      {
        Serial2.print("click b5,1");
      }
      else
      {
        Serial2.print("click b5,0");
      }
      sendToNextion();
      nexStateMesserzylinder = MesserZylinder.request_state();
    }

    // UPDATE BUTTON (momentary):
    if (SchweisstastenZylinder.request_state() != nexStateSchweisstaste)
    {
      if (SchweisstastenZylinder.request_state())
      {
        Serial2.print("click b3,1");
      }
      else
      {
        Serial2.print("click b3,0");
      }
      sendToNextion();
      nexStateSchweisstaste = SchweisstastenZylinder.request_state();
    }
  } // END PAGE 1

  //*******************
  // PAGE 2 - LEFT SIDE
  //*******************
  if (CurrentPage == 2)
  {

    if (nexPrevCoolingTime != eepromCounter.getValue(coolingTime))
    {
      printOnTextField(String(eepromCounter.getValue(coolingTime)) + " s", "t4");
      nexPrevCoolingTime = eepromCounter.getValue(coolingTime);
    }
    //*******************
    // PAGE 2 - RIGHT SIDE
    //*******************
    if (nexPrevLongtimeCounter != eepromCounter.getValue(longtimeCounter))
    {

      printOnTextField(String(eepromCounter.getValue(longtimeCounter)), "t10");
      //PrintOnTextField((eepromCounter.getValue(longtimeCounter) + ("")), "t10");
      nexPrevLongtimeCounter = eepromCounter.getValue(longtimeCounter);
    }
    if (nexPrevShorttimeCounter != eepromCounter.getValue(shorttimeCounter))
    {
      printOnTextField(String(eepromCounter.getValue(shorttimeCounter)), "t12");
      nexPrevShorttimeCounter = eepromCounter.getValue(shorttimeCounter);
    }
    if (resetStopwatchActive)
    {
      if (millis() - counterResetStopwatch > 5000)
      {
        eepromCounter.set(longtimeCounter, 0);
      }
    }
  } // END PAGE 2

  //*******************
  // PAGE 3 - ERROR LOG:
  //*******************
  if (CurrentPage == 3)
  {
    // RESET ERROR LOGS WITH LONG BUTTON PUSH:
    if (nexResetButtonTimeout.active())
    { // returns true if timeout is active
      if (nexResetButtonTimeout.timedOut())
      { // returns true if timeout time has been reached
        errorLogger.setAllZero();
        errorLogPage = 0;
        printLogPage();
        nexResetButtonTimeout.setActive(0);
      }
    }
  } // END PAGE 3
} // END OF NEXTION LOOP

void setup()
{

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

void loop()
{

  nextionLoop();

  // ABFRAGEN DER BANDDETEKTIERUNG, AUSSCHALTEN FALLS KEIN BAND:
  strapDetected = !StrapDetectionSensor.requestButtonState();
  if (!strapDetected)
  {
    errorBlinkState = 1;
    if (StrapDetectionSensor.switchedHigh())
    {
      stopTestRig();
      showInfoField();
      printOnTextField("BAND LEER!", "t4");
      writeErrorLog(magazineEmpty);
    }
  }

  // DER TIMEOUT TIMER LÄUFT NUR AB, WENN DAS RIG IM AUTO MODUS LÄUFT:
  if (!(stateController.machineRunning() && stateController.autoMode()))
  {
    resetTimeout.resetTime();
  }

  // TIMEOUT ÜBERWACHEN, FEHLERSPEICHER SCHREIBEN, RESET ODER STOP EINLEITEN:
  runTimeoutManager();

  // FALLS RESET AKTIVIERT, TEST RIG RESETEN,
  if (stateController.resetMode())
  {
    resetTestRig();
  }

  // ERROLR BLINK FALLS AKTIVIERT:
  generateErrorBlink();

  //IM STEP MODE HÄLT DAS RIG NACH JEDEM SCHRITT AN:
  if (stateController.stepSwitchHappened())
  {
    if (stateController.stepMode())
    {
      stateController.setMachineRunningState(false);
    }
    //PrintCurrentStep(); // zeigt den nächsten Step
  }

  // AUFRUFEN DER UNTERFUNKTIONEN JE NACHDEM OB DAS RIG LÄUFT ODER NICHT:
  if (stateController.machineRunning())
  {
    runMainTestCycle();
  }
}
