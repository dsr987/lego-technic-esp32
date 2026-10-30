#pragma once

// Автоматическое позиционирование мотора C по аналоговой обратной связи.
enum class PositionalServoTarget : uint8_t {
  Left,
  Center,
  Right
};

void positionalServoBegin();
bool positionalServoRequest(PositionalServoTarget target);
void positionalServoUpdate();
void positionalServoCancel();
bool positionalServoIsMoving();
