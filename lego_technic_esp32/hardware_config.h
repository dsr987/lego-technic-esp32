#pragma once

// ---------- TB6612FNG — моторы C и D ----------
#define TB_STBY   4

#define TB_AIN1   16
#define TB_AIN2   17
#define TB_PWMA   18

#define TB_BIN1   19
#define TB_BIN2   23
#define TB_PWMB   25

// ---------- I2C: OLED + PCA9685 FeatherWing ----------
#define I2C_SDA   21
#define I2C_SCL   22

// ---------- Каналы LEDC для TB6612FNG C/D ----------
#define LEDC_CH_C 6
#define LEDC_CH_D 7
