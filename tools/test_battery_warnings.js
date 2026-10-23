#!/usr/bin/env node
'use strict';

// Проверяем точные функции конечного автомата, встроенные в web_page.cpp.
// Для проверки не нужны ESP32 и работающий сервер WebSocket.
const assert = require('node:assert/strict');
const fs = require('node:fs');
const path = require('node:path');
const vm = require('node:vm');

const sourcePath = path.join(__dirname, '..', 'lego_technic_esp32', 'web_page.cpp');
const source = fs.readFileSync(sourcePath, 'utf8');
const match = source.match(
  /\/\* BATTERY_WARNING_STATE_START \*\/([\s\S]*?)\/\* BATTERY_WARNING_STATE_END \*\//
);

assert.ok(match, 'Маркеры конечного автомата предупреждений батареи должны оставаться в web_page.cpp');

const sandbox = {};
vm.runInNewContext(
  match[1] + '\nthis.testApi = { nextBatteryWarningLevel, shouldShowBatteryWarning };',
  sandbox,
  { filename: sourcePath }
);

const { nextBatteryWarningLevel: nextLevel, shouldShowBatteryWarning: shouldShow } = sandbox.testApi;

// Пороги: норма, предупреждение 10%, критическое предупреждение 5%, блокировка управления 1%.
assert.equal(nextLevel(100, 0), 0);
assert.equal(nextLevel(11, 0), 0);
assert.equal(nextLevel(10, 0), 1);
assert.equal(nextLevel(5, 0), 2);
assert.equal(nextLevel(1, 0), 3);

// Гистерезис: предупреждения не мигают на границах восстановления.
assert.equal(nextLevel(12, 1), 1);
assert.equal(nextLevel(13, 1), 0);
assert.equal(nextLevel(6, 2), 2);
assert.equal(nextLevel(7, 2), 1);
assert.equal(nextLevel(2, 3), 3);
assert.equal(nextLevel(3, 3), 2);

// Некорректные проценты не должны создавать критическое состояние.
assert.equal(nextLevel(Number.NaN, 0), 0);
assert.equal(nextLevel(-1, 0), 0);
assert.equal(nextLevel(101, 0), 0);

// Скрытие предупреждения затрагивает только его уровень. Более серьёзное предупреждение и
// блокировка при 1% остаются видимыми, даже если менее серьёзное предупреждение скрыли.
assert.equal(shouldShow(1, 0), true);
assert.equal(shouldShow(1, 1), false);
assert.equal(shouldShow(2, 1), true);
assert.equal(shouldShow(2, 2), false);
assert.equal(shouldShow(3, 3), true);
assert.equal(shouldShow(0, 0), false);

console.log('УСПЕХ: пороги предупреждений батареи, гистерезис, скрытие и критическая блокировка');
