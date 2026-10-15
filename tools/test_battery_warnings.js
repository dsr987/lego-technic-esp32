#!/usr/bin/env node
'use strict';

// Exercise the exact pure state-machine functions embedded in web_page.cpp.
// This does not need an ESP32 or a running WebSocket server.
const assert = require('node:assert/strict');
const fs = require('node:fs');
const path = require('node:path');
const vm = require('node:vm');

const sourcePath = path.join(__dirname, '..', 'lego_technic_esp32', 'web_page.cpp');
const source = fs.readFileSync(sourcePath, 'utf8');
const match = source.match(
  /\/\* BATTERY_WARNING_STATE_START \*\/([\s\S]*?)\/\* BATTERY_WARNING_STATE_END \*\//
);

assert.ok(match, 'Battery warning state-machine markers must remain in web_page.cpp');

const sandbox = {};
vm.runInNewContext(
  match[1] + '\nthis.testApi = { nextBatteryWarningLevel, shouldShowBatteryWarning };',
  sandbox,
  { filename: sourcePath }
);

const { nextBatteryWarningLevel: nextLevel, shouldShowBatteryWarning: shouldShow } = sandbox.testApi;

// Thresholds: normal, 10% warning, 5% critical warning, 1% control lock.
assert.equal(nextLevel(100, 0), 0);
assert.equal(nextLevel(11, 0), 0);
assert.equal(nextLevel(10, 0), 1);
assert.equal(nextLevel(5, 0), 2);
assert.equal(nextLevel(1, 0), 3);

// Hysteresis: warnings do not flicker at their recovery boundaries.
assert.equal(nextLevel(12, 1), 1);
assert.equal(nextLevel(13, 1), 0);
assert.equal(nextLevel(6, 2), 2);
assert.equal(nextLevel(7, 2), 1);
assert.equal(nextLevel(2, 3), 3);
assert.equal(nextLevel(3, 3), 2);

// Invalid percentages cannot manufacture a critical state.
assert.equal(nextLevel(Number.NaN, 0), 0);
assert.equal(nextLevel(-1, 0), 0);
assert.equal(nextLevel(101, 0), 0);

// Dismissing a warning hides only that severity. A more severe warning and
// the 1% lock remain visible even if a less severe warning had been dismissed.
assert.equal(shouldShow(1, 0), true);
assert.equal(shouldShow(1, 1), false);
assert.equal(shouldShow(2, 1), true);
assert.equal(shouldShow(2, 2), false);
assert.equal(shouldShow(3, 3), true);
assert.equal(shouldShow(0, 0), false);

console.log('PASS: battery warning thresholds, hysteresis, dismissal, and critical lock');
