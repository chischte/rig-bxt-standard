/*
 * StateController.cpp
 *
 *  Created on: Oct 20, 2019
 *      Author: realslimshady
 */

#include "StateController.h"

StateController::StateController(int numberOfSteps) {
  _numberOfSteps = numberOfSteps;
}
//***************************************************************************
//LIBRARY FUNCTIONS:
//***************************************************************************

void StateController::setAutoMode() {
  _autoMode = true;
  _stepMode = false;
}
bool StateController::autoMode() {
  return _autoMode;
}

void StateController::setStepMode() {
  _stepMode = true;
  _autoMode = false;
}

bool StateController::stepMode() {
  return _stepMode;
}

void StateController::setMachineRunningState(bool machineState) {
  _machineRunning = machineState;
}

void StateController::toggleMachineRunningState() {
  _machineRunning = !_machineRunning;
}

bool StateController::machineRunning() {
  bool machineRunning = _machineRunning;
  return machineRunning;
}

void StateController::switchToNextStep() {
  _currentCycleStep++;
  if (_currentCycleStep == _numberOfSteps) {
    _currentCycleStep = 0;
  }
}

int StateController::currentCycleStep() {
  int currentCycleStep = _currentCycleStep;
  return currentCycleStep;
}

bool StateController::stepSwitchHappened() {
  bool stepHasChanged = (_previousCycleStep != currentCycleStep());
  _previousCycleStep = currentCycleStep();
  return stepHasChanged;
}

void StateController::setCycleStepTo(int cycleStep) {
  _currentCycleStep = cycleStep;
}

void StateController::setResetMode(bool resetMode) {
  _resetMode = resetMode;
}

bool StateController::resetMode() {
bool resetMode=_resetMode;
return resetMode;
}

void StateController::setRunAfterReset(bool runAfterReset) {
_runAfterReset=runAfterReset;
}

bool StateController::runAfterReset() {
  bool runAfterReset = _runAfterReset;
  return runAfterReset;
}




