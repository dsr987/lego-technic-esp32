
#pragma once

// ---------- TB6612FNG №1 — моторы A и B ----------
#define TB_STBY   4

#define TB_AIN1   16
#define TB_AIN2   17
#define TB_PWMA   18

#define TB_BIN1   19
#define TB_BIN2   21
#define TB_PWMB   22

// ---------- TB6612FNG №2 — моторы C и D ----------
#define TB2_AIN1  13
#define TB2_AIN2  14
#define TB2_PWMA  23

#define TB2_BIN1  32
#define TB2_BIN2  33
#define TB2_PWMB  2

// ---------- Каналы LEDC ----------
#define LEDC_CH_A 4
#define LEDC_CH_B 5
#define LEDC_CH_C 6
#define LEDC_CH_D 7

// ---------- Свет: временные GPIO для прототипа ----------
// GPIO5 и GPIO15 — предварительные значения, а не утверждённая распиновка.
// Способ управления светом и окончательные выводы будут выбраны после
// определения схемы: прямое управление, транзисторные ключи или PCA9685.
#define LED_FRONT_PIN 5  // ВРЕМЕННО: передний свет
#define LED_REAR_PIN  15 // ВРЕМЕННО: задний свет

// Временное предположение для текущей программной реализации.
// Уточнить после выбора реального драйвера и электрической схемы.
#define LED_ACTIVE_LOW 1
