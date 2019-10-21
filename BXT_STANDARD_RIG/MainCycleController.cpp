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

void MainCycleController::setAutoMode() {
  _autoMode = true;
  _stepMode = false;
}
bool MainCycleController::autoMode() {
  return _autoMode;
}

void MainCycleController::setStepMode() {
_stepMode=true;
_autoMode=false;
}

bool MainCycleController::stepMode() {
  return _stepMode;
}

void MainCycleController::setMachineRunningState(bool machineState) {
  _machineRunning = machineState;
}

void MainCycleController::toggleMachineRunningState() {
  _machineRunning = !_machineRunning;
}

bool MainCycleController::machineRunning() {
  bool machineRunning = _machineRunning;
  return machineRunning;
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

void MainCycleController::setCycleStepTo(int cycleStep) {
  _currentCycleStep = cycleStep;
}

int MainCycleController::currentCycleStep() {
  int currentCycleStep = _currentCycleStep;
  return currentCycleStep;
}

