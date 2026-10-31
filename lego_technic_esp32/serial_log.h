#pragma once
#include <Arduino.h>
#include <stdint.h>
void serialLogBegin(unsigned long baudRate);
void serialLogPrintln(const char *message);
void serialLogPrintf(const char *format, ...);
String serialLogReadJson(uint32_t afterId);
