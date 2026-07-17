import test from 'node:test'
import assert from 'node:assert/strict'

import { clampControlValue, shouldAdoptReportedSetpoint } from '../src/utils/controlSetpoints.js'

test('clampControlValue keeps the previous target for an empty input', () => {
  // Arrange
  const previousTarget = 65

  // Act
  const target = clampControlValue('', 0, 100, previousTarget)

  // Assert
  assert.equal(target, previousTarget)
})

test('shouldAdoptReportedSetpoint rejects pump telemetry after user editing', () => {
  // Arrange / Act
  const shouldAdopt = shouldAdoptReportedSetpoint({
    touched: true,
    pending: false,
    reported: 0,
    acceptZero: false,
  })

  // Assert
  assert.equal(shouldAdopt, false)
})

test('shouldAdoptReportedSetpoint rejects rail position after user editing', () => {
  // Arrange / Act
  const shouldAdopt = shouldAdoptReportedSetpoint({
    touched: true,
    pending: false,
    reported: 0,
  })

  // Assert
  assert.equal(shouldAdopt, false)
})

test('shouldAdoptReportedSetpoint accepts a valid initial running pump value', () => {
  // Arrange / Act
  const shouldAdopt = shouldAdoptReportedSetpoint({
    touched: false,
    pending: false,
    reported: 72,
    acceptZero: false,
  })

  // Assert
  assert.equal(shouldAdopt, true)
})
