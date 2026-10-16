#pragma once

// Моторы управляются через PCA9685 + два TB6612FNG на I2C Motor Shield.
// OLED и Motor Shield используют общую I2C-шину ESP32.
#define I2C_SDA_PIN 21
#define I2C_SCL_PIN 22
