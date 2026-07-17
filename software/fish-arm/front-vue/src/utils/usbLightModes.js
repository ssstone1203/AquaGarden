export const USB_LIGHT_MODES = Object.freeze([
  { mode: 0, group: '常亮', label: '红灯常亮', description: '红色持续警示', tone: 'red' },
  { mode: 1, group: '常亮', label: '黄灯常亮', description: '黄色持续警示', tone: 'yellow' },
  { mode: 2, group: '常亮', label: '绿灯常亮', description: '绿色持续指示', tone: 'green' },
  { mode: 3, group: '常亮', label: '红灯 + 喇叭', description: '红色持续警示并鸣响', tone: 'red' },
  { mode: 4, group: '常亮', label: '白灯常亮', description: '白色持续指示', tone: 'white' },
  { mode: 5, group: '常亮', label: '青灯常亮', description: '青色持续指示', tone: 'cyan' },
  { mode: 6, group: '常亮', label: '紫灯常亮', description: '紫色持续指示', tone: 'purple' },
  { mode: 7, group: '常亮', label: '蓝灯常亮', description: '蓝色持续指示', tone: 'blue' },
  { mode: 8, group: '慢闪', label: '红灯慢闪', description: '红色低频闪烁', tone: 'red' },
  { mode: 9, group: '慢闪', label: '黄灯慢闪', description: '黄色低频闪烁', tone: 'yellow' },
  { mode: 10, group: '慢闪', label: '绿灯慢闪', description: '绿色低频闪烁', tone: 'green' },
  { mode: 11, group: '慢闪', label: '红灯慢闪 + 喇叭', description: '红色低频闪烁并鸣响', tone: 'red' },
  { mode: 12, group: '慢闪', label: '白灯慢闪', description: '白色低频闪烁', tone: 'white' },
  { mode: 13, group: '慢闪', label: '青灯慢闪', description: '青色低频闪烁', tone: 'cyan' },
  { mode: 14, group: '慢闪', label: '紫灯慢闪', description: '紫色低频闪烁', tone: 'purple' },
  { mode: 15, group: '慢闪', label: '蓝灯慢闪', description: '蓝色低频闪烁', tone: 'blue' },
  { mode: 16, group: '快闪', label: '红灯快闪', description: '红色高频闪烁', tone: 'red' },
  { mode: 17, group: '快闪', label: '黄灯快闪', description: '黄色高频闪烁', tone: 'yellow' },
  { mode: 18, group: '快闪', label: '绿灯快闪', description: '绿色高频闪烁', tone: 'green' },
  { mode: 19, group: '快闪', label: '红灯快闪 + 喇叭', description: '最高优先级声光报警', tone: 'red' },
  { mode: 20, group: '快闪', label: '白灯快闪', description: '白色高频闪烁', tone: 'white' },
  { mode: 21, group: '快闪', label: '青灯快闪', description: '青色高频闪烁', tone: 'cyan' },
  { mode: 22, group: '快闪', label: '紫灯快闪', description: '紫色高频闪烁', tone: 'purple' },
  { mode: 23, group: '快闪', label: '蓝灯快闪', description: '蓝色高频闪烁', tone: 'blue' },
  { mode: 24, group: '喇叭', label: '仅开喇叭', description: '保持灯光状态并开启喇叭', tone: 'horn' },
  { mode: 25, group: '喇叭', label: '关闭喇叭', description: '保持灯光状态并关闭喇叭', tone: 'horn-off' },
  { mode: 26, group: '系统', label: '全部关闭', description: '关闭灯光与喇叭', tone: 'off' },
])

export const USB_LIGHT_MODE_GROUPS = Object.freeze(
  [...new Set(USB_LIGHT_MODES.map(({ group }) => group))].map((group) => ({
    group,
    modes: USB_LIGHT_MODES.filter((item) => item.group === group),
  })),
)

export function getUsbLightMode(mode) {
  if (mode == null || mode === '') return null
  const parsed = Number(mode)
  if (!Number.isInteger(parsed)) return null
  return USB_LIGHT_MODES.find((item) => item.mode === parsed) ?? null
}
