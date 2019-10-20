/*
 * MainCycleController.h
 *
 *  Created on: Oct 20, 2019
 *      Author: realslimshady
 */

#ifndef MAINCYCLECONTROLLER_H_
#define MAINCYCLECONTROLLER_H_

class MainCycleController {
public:
  // FUNCTIONS:
  MainCycleController(int numberOfSteps);
  void setMachineRunning(bool machineState);
  void switchToNextStep();
  bool machineIsRunning();
  void setCycleStepTo(int cycleStep);
  bool stepSwitchHappened();
  int currentCycleStep();

  // VARIABLES:
  // n.a.

private:
  // FUNCTIONS:
  // n.a.

  // VARIABLES:

  int _numberOfSteps;
  int _currentCycleStep;
  bool _machineRunning;
  int _previousCycleStep;
};
#endif /* MAINCYCLECONTROLLER_H_ */
