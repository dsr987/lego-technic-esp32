#pragma once

#include <Arduino.h>
#include <ESPAsyncWebServer.h>

// Обработка входящих команд и событий WebSocket
void handleWsMessage(uint8_t *data, size_t len);

void onWsEvent(
  AsyncWebSocket *server,
  AsyncWebSocketClient *client,
  AwsEventType type,
  void *arg,
  uint8_t *data,
  size_t len
);