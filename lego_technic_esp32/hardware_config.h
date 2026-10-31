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

// Исторический делитель батареи Hardware 1.0: Rверх=100 кОм, Rниз=39 кОм.
// Внешняя подтяжка обратной связи позиционного сервопривода: GPIO35 -> GND, 10 кОм.

// ---------- I2C: OLED + PCA9685 FeatherWing ----------
#define I2C_SDA   21
#define I2C_SCL   22

// ---------- Каналы LEDC для TB6612FNG C/D ----------
#define LEDC_CH_C 6
#define LEDC_CH_D 7
