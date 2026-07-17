export function clampControlValue(value, min, max, fallback) {
  const fallbackNumber = Number(fallback)
  const safeFallback = Number.isFinite(fallbackNumber)
    ? Math.max(min, Math.min(max, fallbackNumber))
    : min

  if (value === '' || value == null) return safeFallback
  const parsed = Number(value)
  if (!Number.isFinite(parsed)) return safeFallback
  return Math.max(min, Math.min(max, parsed))
}

export function shouldAdoptReportedSetpoint({ touched, pending, reported, acceptZero = true }) {
  const parsed = Number(reported)
  if (touched || pending || !Number.isFinite(parsed)) return false
  return acceptZero || parsed > 0
}
