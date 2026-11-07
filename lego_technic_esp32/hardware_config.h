#pragma once

// ---------- TB6612FNG — моторы C и D, Hardware 2.0 ----------
#define TB_STBY   25

#define TB_AIN1   18
#define TB_AIN2   19
#define TB_PWMA   23

#define TB_BIN1   17
#define TB_BIN2   16
#define TB_PWMB   4

// ---------- Сервоприводы и аналоговые входы ----------
#define SERVO_PIN           27
#define BATT_PIN            34
#define SERVO_FEEDBACK_PIN  35

// Целевая схема делителя батареи: Rверх=100 кОм, Rниз=47 кОм; коэффициент см. в battery_monitor.cpp.
// На GPIO34 предусмотреть керамический фильтрующий конденсатор 100 нФ между точкой ADC и GND.
// Внешняя подтяжка обратной связи позиционного сервопривода остаётся: GPIO35 -> GND, 10 кОм.

// ---------- I2C: OLED + PCA9685 FeatherWing ----------
#define I2C_SDA   21
#define I2C_SCL   22

// Частота PWM отдельного TB6612FNG (каналы C/D).
constexpr uint16_t TB6612FNG_PWM_FREQUENCY_HZ = 1600;

// ---------- Каналы LEDC для TB6612FNG C/D ----------
#define LEDC_CH_C 6
#define LEDC_CH_D 7
