import { ref } from 'vue'

let _devMode = ref(false)
let _mockKillCount = ref(0)
let _mockInterval = null
let _listeners = {}

export function isDevMode() { return _devMode.value }

export function enableDevMode() {
  _devMode.value = true
  uni.setStorageSync('moskill_dev_mode', true)
  startMockData()
}

export function disableDevMode() {
  _devMode.value = false
  uni.removeStorageSync('moskill_dev_mode')
  stopMockData()
}

function emit(event, data) {
  (_listeners[event] || []).forEach(fn => fn(data))
}

export function onMock(event, fn) {
  if (!_listeners[event]) _listeners[event] = []
  _listeners[event].push(fn)
}

export function offMock(event, fn) {
  if (!_listeners[event]) return
  _listeners[event] = _listeners[event].filter(f => f !== fn)
}

function randomKillClass() {
  const r = Math.random()
  if (r < 0.15) return 0
  if (r < 0.7) return 1
  if (r < 0.9) return 2
  return 3
}

function startMockData() {
  _mockKillCount.value = Math.floor(Math.random() * 50)

  _mockInterval = setInterval(() => {
    if (Math.random() < 0.3) {
      _mockKillCount.value++
      emit('kill', _mockKillCount.value)
    }
    if (Math.random() < 0.1) {
      emit('env', {
        temp: 25 + Math.random() * 10 - 5,
        humi: 60 + Math.random() * 30 - 15,
      })
    }
  }, 2000)
}

function stopMockData() {
  if (_mockInterval) {
    clearInterval(_mockInterval)
    _mockInterval = null
  }
}

export function mockSession() {
  const kills = _mockKillCount.value || Math.floor(Math.random() * 100)
  const killsM = Math.floor(kills * 0.55)
  return {
    sessionId: Date.now(),
    startTime: Math.floor(Date.now() / 1000) - 3600,
    duration: 1200 + Math.floor(Math.random() * 3600),
    killsTotal: kills,
    killsS: Math.floor(kills * 0.15),
    killsM,
    killsL: Math.floor(kills * 0.2),
    killsXL: kills - Math.floor(kills * 0.15) - killsM - Math.floor(kills * 0.2),
    maxStreak: 3 + Math.floor(Math.random() * 8),
    energy: kills * 5000,
    avgTemp: 27.5,
    avgHumi: 68.3,
    hvOnTime: 600 + Math.floor(Math.random() * 1800),
    efficiency: 0.5 + Math.random() * 0.5,
  }
}

export function mockLifetime() {
  const total = 500 + Math.floor(Math.random() * 2000)
  const hourly = Array.from({ length: 24 }, (_, i) => {
    if (i >= 18 && i <= 22) return 10 + Math.floor(Math.random() * 20)
    if (i >= 6 && i <= 8) return 5 + Math.floor(Math.random() * 10)
    return Math.floor(Math.random() * 5)
  })
  const daily = Array.from({ length: 7 }, () => 30 + Math.floor(Math.random() * 50))
  return {
    totalKills: total,
    killsS: Math.floor(total * 0.12),
    killsM: Math.floor(total * 0.55),
    killsL: Math.floor(total * 0.22),
    killsXL: total - Math.floor(total * 0.12) - Math.floor(total * 0.55) - Math.floor(total * 0.22),
    totalSessions: 20 + Math.floor(Math.random() * 50),
    totalActive: 36000 + Math.floor(Math.random() * 72000),
    bestStreak: 8 + Math.floor(Math.random() * 12),
    bestSession: 30 + Math.floor(Math.random() * 70),
    killRate: 5 + Math.random() * 15,
    hourly,
    daily,
    firstUse: Math.floor(Date.now() / 1000) - 86400 * 30,
  }
}

export function mockConfig() {
  return {
    sensitivity: 1,
    ledBrightness: 128,
    buzzerVolume: 2,
    buzzerOnKill: true,
    ledOnKill: true,
    streakEffects: true,
  }
}

export function mockBattery() {
  return 60 + Math.floor(Math.random() * 35)
}

export { _devMode, _mockKillCount }
