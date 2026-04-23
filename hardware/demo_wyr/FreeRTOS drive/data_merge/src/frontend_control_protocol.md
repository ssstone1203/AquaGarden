# Frontend Control Protocol

UART downlink frame format:

- Frame: `5A A5 LEN CMD PAYLOAD... CRC16_LO CRC16_HI`
- CRC16: Modbus polynomial `0xA001`, init `0xFFFF`, over bytes `[SYNC0..payload]`
- `LEN = 1 + payload_len` (contains `CMD` byte length)

## Pump Commands

### `0x01` Set Manual Pump (legacy + still supported)

- Payload: `[manual_mode, pwm_percent]`
- `manual_mode`: `0` = auto mode, `1` = manual mode
- `pwm_percent`: `0..100` (firmware clamps >100 to 100)

Behavior:

- `manual_mode=1, pwm>0` -> pump runs in manual mode with given PWM
- `manual_mode=1, pwm=0` -> pump stops in manual mode
- `manual_mode=0` -> return to auto linkage mode

### `0x04` Pump Start (recommended for start button)

- Payload:
  - Optional `[pwm_percent]`
  - Can be empty `[]`
- Behavior:
  - Forces manual mode on
  - If payload provided, updates PWM (`0..100`)
  - If current manual PWM is `0`, firmware sets default to `60%`

### `0x05` Pump Stop (recommended for stop button)

- Payload: `[]`
- Behavior: forces manual mode on and sets PWM to `0`

### `0x06` Set Pump PWM (recommended for speed slider)

- Payload: `[pwm_percent]`
- Behavior: forces manual mode on and sets PWM (`0..100`)

### `0x07` Set Pump Auto

- Payload: `[]`
- Behavior: exits manual mode, returns pump control to sensor linkage logic

## Sensor / Linkage Config Commands

### `0x02` Soil Config

- Payload: `[threshold, hysteresis]`
- Both values are clamped to `0..100`

### `0x03` Linkage Config

- Payload: `[enable_bits, temp_high_i16_x10_lo, temp_high_i16_x10_hi, wqi_low]`
- `enable_bits`: bit0 soil, bit1 water temperature, bit2 water quality
- `temp_high`: signed int16 in `x10`, then clamped to `0.0..100.0`
- `wqi_low`: clamped to `0..100`

## Quick Frontend Mapping

- Start button -> send `0x04`
- Stop button -> send `0x05`
- PWM slider change -> send `0x06` with slider value
- "Auto/Manual" toggle:
  - Auto -> `0x07`
  - Manual -> `0x04` (optionally with current slider PWM)
