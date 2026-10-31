#pragma once

#include <Arduino.h>

// Автоматическое позиционирование мотора C по аналоговой обратной связи.
enum class PositionalServoTarget : uint8_t {
  Left,
  Center,
  Right
};

enum class PositionalServoState : uint8_t {
  Idle,
  Moving,
  WaitingForDirectionChange,
  Fault
};

struct PositionalServoStatus {
  PositionalServoState state;
  PositionalServoTarget target;
  int feedbackMv;
  int targetMv;
  bool targetValid;
  bool feedbackValid;
  bool targetReached;
};

void positionalServoBegin();
bool positionalServoRequest(PositionalServoTarget target);
void positionalServoUpdate();
void positionalServoCancel();
bool positionalServoIsMoving();
void positionalServoSetSpeedPercent(int percent);
int positionalServoGetSpeedPercent();
void positionalServoGetStatus(PositionalServoStatus &status);
