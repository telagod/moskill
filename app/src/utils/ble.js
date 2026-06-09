const UUID_BASE = 'e3a1XXXX-f5e8-4c8a-9b3d-2c1f7b8a6d50'
const uuid = (short) => UUID_BASE.replace('XXXX', short)

export const UUID = {
  SVC: uuid('1B00'),
  KILL_COUNT: uuid('1B01'),
  SESSION: uuid('1B02'),
  LIFETIME: uuid('1B03'),
  KILL_LOG: uuid('1B04'),
  ENV: uuid('1B05'),
  CONFIG: uuid('1B11'),
  TIME_SYNC: uuid('1B12'),
  OTA_CTRL: uuid('1B21'),
  BATTERY: '0000180f-0000-1000-8000-00805f9b34fb',
  BAT_LEVEL: '00002a19-0000-1000-8000-00805f9b34fb',
}

const KILL_CLASSES = ['S', 'M', 'L', 'XL']
const KILL_LABELS = ['Fruit Fly', 'Mosquito', 'House Fly', 'Moth']
const KILL_ICONS = ['fruit-fly', 'mosquito', 'fly', 'moth']

let _deviceId = null
let _connected = false
let _listeners = {}

function ab2hex(buffer) {
  return Array.from(new Uint8Array(buffer)).map(b => b.toString(16).padStart(2, '0')).join('')
}

function buf2u32(buf, offset = 0) {
  const dv = new DataView(buf)
  return dv.getUint32(offset, true)
}

function buf2u16(buf, offset = 0) {
  const dv = new DataView(buf)
  return dv.getUint16(offset, true)
}

function buf2i16(buf, offset = 0) {
  const dv = new DataView(buf)
  return dv.getInt16(offset, true)
}

export function isConnected() { return _connected }
export function getDeviceId() { return _deviceId }

export function on(event, fn) {
  if (!_listeners[event]) _listeners[event] = []
  _listeners[event].push(fn)
}

export function off(event, fn) {
  if (!_listeners[event]) return
  _listeners[event] = _listeners[event].filter(f => f !== fn)
}

function emit(event, data) {
  (_listeners[event] || []).forEach(fn => fn(data))
}

export function scanDevices() {
  return new Promise((resolve, reject) => {
    const devices = []
    uni.openBluetoothAdapter({
      success() {
        uni.startBluetoothDevicesDiscovery({
          services: [UUID.SVC],
          allowDuplicatesKey: false,
          success() {
            uni.onBluetoothDeviceFound((res) => {
              res.devices.forEach(d => {
                if (d.name && d.name.includes('MosKill')) {
                  devices.push({
                    deviceId: d.deviceId,
                    name: d.name,
                    RSSI: d.RSSI,
                  })
                  emit('deviceFound', d)
                }
              })
            })
            setTimeout(() => {
              uni.stopBluetoothDevicesDiscovery()
              resolve(devices)
            }, 5000)
          },
          fail: reject,
        })
      },
      fail: reject,
    })
  })
}

export function connect(deviceId) {
  return new Promise((resolve, reject) => {
    _deviceId = deviceId
    uni.createBLEConnection({
      deviceId,
      success() {
        _connected = true
        setTimeout(() => {
          uni.getBLEDeviceServices({
            deviceId,
            success(res) {
              const svc = res.services.find(s => s.uuid.toLowerCase().includes('1b00'))
              if (svc) {
                uni.getBLEDeviceCharacteristics({
                  deviceId,
                  serviceId: svc.uuid,
                  success() {
                    syncTime().then(() => resolve()).catch(() => resolve())
                  },
                  fail: reject,
                })
              } else {
                resolve()
              }
            },
            fail: reject,
          })
        }, 500)
      },
      fail: reject,
    })

    uni.onBLEConnectionStateChange((res) => {
      if (res.deviceId === deviceId) {
        _connected = res.connected
        emit('connectionChange', res.connected)
      }
    })
  })
}

export function disconnect() {
  if (!_deviceId) return Promise.resolve()
  return new Promise((resolve) => {
    uni.closeBLEConnection({ deviceId: _deviceId, complete: () => {
      _connected = false
      _deviceId = null
      resolve()
    }})
  })
}

function readChar(charId) {
  return new Promise((resolve, reject) => {
    uni.readBLECharacteristicValue({
      deviceId: _deviceId,
      serviceId: UUID.SVC,
      characteristicId: charId,
      success() {
        uni.onBLECharacteristicValueChange((res) => {
          if (res.characteristicId.toLowerCase().includes(charId.split('-')[0].slice(-4).toLowerCase())) {
            resolve(res.value)
          }
        })
      },
      fail: reject,
    })
  })
}

function writeChar(charId, buffer) {
  return new Promise((resolve, reject) => {
    uni.writeBLECharacteristicValue({
      deviceId: _deviceId,
      serviceId: UUID.SVC,
      characteristicId: charId,
      value: buffer,
      success: resolve,
      fail: reject,
    })
  })
}

export function subscribeKillCount(callback) {
  uni.notifyBLECharacteristicValueChange({
    deviceId: _deviceId,
    serviceId: UUID.SVC,
    characteristicId: UUID.KILL_COUNT,
    state: true,
  })
  uni.onBLECharacteristicValueChange((res) => {
    if (res.characteristicId.toLowerCase().includes('1b01')) {
      const count = buf2u32(res.value)
      callback(count)
      emit('kill', count)
    }
  })
}

export function subscribeEnv(callback) {
  uni.notifyBLECharacteristicValueChange({
    deviceId: _deviceId,
    serviceId: UUID.SVC,
    characteristicId: UUID.ENV,
    state: true,
  })
  uni.onBLECharacteristicValueChange((res) => {
    if (res.characteristicId.toLowerCase().includes('1b05')) {
      const temp = buf2i16(res.value, 0) / 10.0
      const humi = buf2u16(res.value, 2) / 10.0
      callback({ temp, humi })
    }
  })
}

export async function readSession() {
  const buf = await readChar(UUID.SESSION)
  const dv = new DataView(buf)
  return {
    sessionId: dv.getUint32(0, true),
    startTime: dv.getUint32(4, true),
    duration: dv.getUint32(8, true),
    killsTotal: dv.getUint16(12, true),
    killsS: dv.getUint16(14, true),
    killsM: dv.getUint16(16, true),
    killsL: dv.getUint16(18, true),
    killsXL: dv.getUint16(20, true),
    maxStreak: dv.getUint16(22, true),
    energy: dv.getUint32(24, true),
    avgTemp: dv.getInt16(28, true) / 10.0,
    avgHumi: dv.getUint16(30, true) / 10.0,
    hvOnTime: dv.getUint16(32, true),
    efficiency: dv.getUint16(34, true) / 100.0,
  }
}

export async function readLifetime() {
  const buf = await readChar(UUID.LIFETIME)
  const dv = new DataView(buf)
  const hourly = []
  for (let i = 0; i < 24; i++) hourly.push(dv.getUint8(40 + i))
  const daily = []
  for (let i = 0; i < 7; i++) daily.push(dv.getUint8(64 + i))
  return {
    totalKills: dv.getUint32(0, true),
    killsS: dv.getUint32(4, true),
    killsM: dv.getUint32(8, true),
    killsL: dv.getUint32(12, true),
    killsXL: dv.getUint32(16, true),
    totalSessions: dv.getUint32(20, true),
    totalActive: dv.getUint32(24, true),
    bestStreak: dv.getUint32(28, true),
    bestSession: dv.getUint32(32, true),
    killRate: dv.getUint32(36, true) / 100.0,
    hourly,
    daily,
    firstUse: dv.getUint32(72, true),
  }
}

export async function readConfig() {
  const buf = await readChar(UUID.CONFIG)
  const dv = new DataView(buf)
  return {
    sensitivity: dv.getUint8(0),
    ledBrightness: dv.getUint8(1),
    buzzerVolume: dv.getUint8(2),
    buzzerOnKill: dv.getUint8(3) === 1,
    ledOnKill: dv.getUint8(4) === 1,
    streakEffects: dv.getUint8(5) === 1,
  }
}

export async function writeConfig(cfg) {
  const buf = new ArrayBuffer(8)
  const dv = new DataView(buf)
  dv.setUint8(0, cfg.sensitivity)
  dv.setUint8(1, cfg.ledBrightness)
  dv.setUint8(2, cfg.buzzerVolume)
  dv.setUint8(3, cfg.buzzerOnKill ? 1 : 0)
  dv.setUint8(4, cfg.ledOnKill ? 1 : 0)
  dv.setUint8(5, cfg.streakEffects ? 1 : 0)
  return writeChar(UUID.CONFIG, buf)
}

export async function syncTime() {
  const epoch = Math.floor(Date.now() / 1000)
  const buf = new ArrayBuffer(4)
  new DataView(buf).setUint32(0, epoch, true)
  return writeChar(UUID.TIME_SYNC, buf)
}

export async function otaStart(fileSize) {
  const buf = new ArrayBuffer(5)
  const dv = new DataView(buf)
  dv.setUint8(0, 0x01)
  dv.setUint32(1, fileSize, true)
  return writeChar(UUID.OTA_CTRL, buf)
}

export async function otaWriteChunk(offset, data) {
  const buf = new ArrayBuffer(5 + data.byteLength)
  const dv = new DataView(buf)
  dv.setUint8(0, 0x02)
  dv.setUint32(1, offset, true)
  new Uint8Array(buf, 5).set(new Uint8Array(data))
  return writeChar(UUID.OTA_CTRL, buf)
}

export async function otaVerify(crc32) {
  const buf = new ArrayBuffer(5)
  const dv = new DataView(buf)
  dv.setUint8(0, 0x03)
  dv.setUint32(1, crc32, true)
  return writeChar(UUID.OTA_CTRL, buf)
}

export async function otaAbort() {
  const buf = new ArrayBuffer(1)
  new DataView(buf).setUint8(0, 0x04)
  return writeChar(UUID.OTA_CTRL, buf)
}

export function crc32(data) {
  let crc = 0xFFFFFFFF
  const bytes = new Uint8Array(data)
  for (let i = 0; i < bytes.length; i++) {
    crc ^= bytes[i]
    for (let j = 0; j < 8; j++) {
      crc = (crc >>> 1) ^ (0xEDB88320 & -(crc & 1))
    }
  }
  return (crc ^ 0xFFFFFFFF) >>> 0
}

export { KILL_CLASSES, KILL_LABELS, KILL_ICONS }
