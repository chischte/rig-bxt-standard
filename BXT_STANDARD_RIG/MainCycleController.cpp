/*
 * MainCycleController.cpp
 *
 *  Created on: Oct 20, 2019
 *      Author: realslimshady
 */

#include "MainCycleController.h"

MainCycleController::MainCycleController(int numberOfSteps) {
  _numberOfSteps = numberOfSteps;
}
//***************************************************************************
//LIBRARY FUNCTIONS:
//***************************************************************************

void MainCycleController::setMachineRunning(bool machineState){
  _machineRunning=machineState;
}

void MainCycleController::switchToNextStep() {
  _currentCycleStep++;
  if (_currentCycleStep == _numberOfSteps) {
    _currentCycleStep = 0;
  }
}

bool MainCycleController::stepSwitchHappened() {
  bool stepHasChanged = (_previousCycleStep != currentCycleStep());
  _previousCycleStep = currentCycleStep();
  return stepHasChanged;
}

bool MainCycleController::machineIsRunning() {
bool machineRunning = _machineRunning;
return machineRunning;
}

void MainCycleController::setCycleStepTo(int cycleStep) {
_currentCycleStep=cycleStep;
}

int MainCycleController::currentCycleStep() {
int currentCycleStep = _currentCycleStep;
return currentCycleStep;
}

