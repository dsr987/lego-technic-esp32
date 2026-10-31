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

enum class PositionalServoFeedbackZone : uint8_t {
  Right = 0,
  RightToCenter = 1,
  Center = 2,
  CenterToLeft = 3,
  Left = 4,
  Unknown = 5
};

struct PositionalServoStatus {
  PositionalServoState state;
  PositionalServoTarget target;
  int feedbackMv;
  int targetMv;
  bool targetValid;
  bool feedbackValid;
  bool targetReached;
  PositionalServoFeedbackZone feedbackZone;
};

void positionalServoBegin();
bool positionalServoRequest(PositionalServoTarget target);
void positionalServoUpdate();
void positionalServoCancel();
bool positionalServoIsMoving();
void positionalServoSetSpeedPercent(int percent);
int positionalServoGetSpeedPercent();
void positionalServoGetStatus(PositionalServoStatus &status);
