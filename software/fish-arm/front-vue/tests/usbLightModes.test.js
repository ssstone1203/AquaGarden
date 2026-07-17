import test from 'node:test'
import assert from 'node:assert/strict'

import { USB_LIGHT_MODES, getUsbLightMode } from '../src/utils/usbLightModes.js'

test('USB light mode table covers every firmware mode exactly once', () => {
  // Arrange / Act
  const modeValues = USB_LIGHT_MODES.map(({ mode }) => mode)

  // Assert
  assert.deepEqual(modeValues, Array.from({ length: 27 }, (_, index) => index))
})

test('USB light mode table identifies the all-off command', () => {
  // Arrange / Act
  const offMode = getUsbLightMode(26)

  // Assert
  assert.deepEqual(offMode, {
    mode: 26,
    group: '系统',
    label: '全部关闭',
    description: '关闭灯光与喇叭',
    tone: 'off',
  })
})

test('USB light mode lookup rejects the firmware uninitialized sentinel', () => {
  // Arrange / Act / Assert
  assert.equal(getUsbLightMode(0xFF), null)
})

test('USB light mode lookup keeps missing telemetry in the unknown state', () => {
  // Arrange / Act / Assert
  assert.equal(getUsbLightMode(null), null)
  assert.equal(getUsbLightMode(undefined), null)
  assert.equal(getUsbLightMode(''), null)
})
