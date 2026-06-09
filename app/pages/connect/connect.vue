<template>
  <view class="container">
    <!-- Scanning Header -->
    <view class="scan-header">
      <view class="scan-status" v-if="scanning">
        <view class="scan-dot"></view>
        <text class="scan-text">Scanning for MosKill devices...</text>
      </view>
      <view class="scan-status" v-else-if="devices.length === 0 && !error">
        <text class="scan-text text-small">No devices found</text>
      </view>
      <view class="scan-status" v-else-if="!error">
        <text class="scan-text">{{ devices.length }} device{{ devices.length > 1 ? 's' : '' }} found</text>
      </view>
    </view>

    <!-- Error State -->
    <view class="card error-card" v-if="error">
      <text class="error-icon">⚠️</text>
      <text class="error-title">Connection Error</text>
      <text class="error-msg">{{ error }}</text>
      <button class="btn-primary mt-20" @tap="startScan">Retry</button>
    </view>

    <!-- Scanning Animation -->
    <view class="scan-anim-container" v-if="scanning && devices.length === 0">
      <view class="radar-ring ring-1"></view>
      <view class="radar-ring ring-2"></view>
      <view class="radar-ring ring-3"></view>
      <text class="radar-center">📡</text>
    </view>

    <!-- Device List -->
    <view class="device-list">
      <view
        class="card device-card"
        v-for="device in devices"
        :key="device.deviceId"
        @tap="connectDevice(device)"
        :class="{ connecting: connectingId === device.deviceId }"
      >
        <view class="flex-between">
          <view class="device-info">
            <text class="device-name">{{ device.name }}</text>
            <text class="device-id text-small">{{ device.deviceId.slice(0, 17) }}...</text>
          </view>
          <view class="device-right">
            <view class="rssi-bars">
              <view class="rssi-bar" :class="{ active: device.RSSI > -90 }"></view>
              <view class="rssi-bar" :class="{ active: device.RSSI > -75 }"></view>
              <view class="rssi-bar" :class="{ active: device.RSSI > -60 }"></view>
              <view class="rssi-bar" :class="{ active: device.RSSI > -45 }"></view>
            </view>
            <text class="rssi-value text-small">{{ device.RSSI }}dBm</text>
          </view>
        </view>

        <!-- Connecting Indicator -->
        <view class="connecting-overlay" v-if="connectingId === device.deviceId">
          <view class="connect-spinner"></view>
          <text class="connecting-text">Connecting...</text>
        </view>
      </view>
    </view>

    <!-- Rescan Button -->
    <view class="bottom-actions" v-if="!scanning">
      <button class="btn-secondary" @tap="startScan">
        <text>🔄 Scan Again</text>
      </button>
    </view>
  </view>
</template>

<script>
import { ref, onMounted, onUnmounted } from 'vue'
import { useStore } from 'vuex'
import { scanDevices, connect, on, off } from '@/utils/ble.js'

export default {
  setup() {
    const store = useStore()
    const scanning = ref(false)
    const devices = ref([])
    const error = ref('')
    const connectingId = ref(null)

    function onDeviceFound(device) {
      const exists = devices.value.find(d => d.deviceId === device.deviceId)
      if (!exists && device.name && device.name.includes('MosKill')) {
        devices.value.push({
          deviceId: device.deviceId,
          name: device.name,
          RSSI: device.RSSI
        })
      }
    }

    async function startScan() {
      scanning.value = true
      error.value = ''
      devices.value = []
      on('deviceFound', onDeviceFound)

      try {
        const found = await scanDevices()
        // Merge any found from the resolved promise
        found.forEach(d => {
          if (!devices.value.find(x => x.deviceId === d.deviceId)) {
            devices.value.push(d)
          }
        })
      } catch (e) {
        if (e.errCode === 10001 || e.errMsg?.includes('not available')) {
          error.value = 'Bluetooth is turned off. Please enable Bluetooth in Settings.'
        } else if (e.errMsg?.includes('auth')) {
          error.value = 'Bluetooth permission denied. Please grant permission in Settings.'
        } else {
          error.value = e.errMsg || e.message || 'Failed to scan for devices'
        }
      } finally {
        scanning.value = false
        off('deviceFound', onDeviceFound)
      }
    }

    async function connectDevice(device) {
      if (connectingId.value) return
      connectingId.value = device.deviceId
      error.value = ''

      try {
        await connect(device.deviceId)
        store.commit('SET_CONNECTED', {
          connected: true,
          deviceId: device.deviceId,
          deviceName: device.name
        })
        uni.showToast({ title: 'Connected!', icon: 'success' })
        setTimeout(() => {
          uni.switchTab({ url: '/pages/dashboard/dashboard' })
        }, 500)
      } catch (e) {
        error.value = 'Failed to connect: ' + (e.errMsg || e.message || 'Unknown error')
        connectingId.value = null
      }
    }

    onMounted(() => {
      startScan()
    })

    onUnmounted(() => {
      off('deviceFound', onDeviceFound)
    })

    return { scanning, devices, error, connectingId, startScan, connectDevice }
  }
}
</script>

<style scoped>
.scan-header {
  padding: 20rpx 0 40rpx;
}

.scan-status {
  display: flex;
  flex-direction: row;
  align-items: center;
  gap: 16rpx;
}

.scan-dot {
  width: 16rpx;
  height: 16rpx;
  border-radius: 50%;
  background: #e63946;
  animation: blink 1s ease-in-out infinite;
}

@keyframes blink {
  0%, 100% { opacity: 1; }
  50% { opacity: 0.3; }
}

.scan-text {
  font-size: 30rpx;
  color: #aaa;
}

.error-card {
  text-align: center;
  border: 1rpx solid rgba(230, 57, 70, 0.3);
}

.error-icon {
  font-size: 64rpx;
  display: block;
  margin-bottom: 16rpx;
}

.error-title {
  font-size: 34rpx;
  font-weight: 600;
  color: #e63946;
  display: block;
  margin-bottom: 12rpx;
}

.error-msg {
  font-size: 28rpx;
  color: #888;
  display: block;
  line-height: 1.5;
}

.scan-anim-container {
  display: flex;
  align-items: center;
  justify-content: center;
  height: 400rpx;
  position: relative;
}

.radar-ring {
  position: absolute;
  border-radius: 50%;
  border: 2rpx solid rgba(230, 57, 70, 0.3);
  animation: radar-expand 2s ease-out infinite;
}

.ring-1 { width: 100rpx; height: 100rpx; animation-delay: 0s; }
.ring-2 { width: 100rpx; height: 100rpx; animation-delay: 0.6s; }
.ring-3 { width: 100rpx; height: 100rpx; animation-delay: 1.2s; }

@keyframes radar-expand {
  0% { transform: scale(1); opacity: 1; }
  100% { transform: scale(4); opacity: 0; }
}

.radar-center {
  font-size: 60rpx;
  z-index: 2;
}

.device-list {
  margin-top: 20rpx;
}

.device-card {
  position: relative;
  overflow: hidden;
  transition: transform 0.2s, box-shadow 0.2s;
}

.device-card:active {
  transform: scale(0.98);
}

.device-card.connecting {
  opacity: 0.7;
}

.device-info {
  display: flex;
  flex-direction: column;
  gap: 8rpx;
}

.device-name {
  font-size: 32rpx;
  font-weight: 600;
  color: #fff;
}

.device-id {
  font-size: 22rpx;
}

.device-right {
  display: flex;
  flex-direction: column;
  align-items: flex-end;
  gap: 8rpx;
}

.rssi-bars {
  display: flex;
  flex-direction: row;
  align-items: flex-end;
  gap: 4rpx;
}

.rssi-bar {
  width: 8rpx;
  background: #333;
  border-radius: 2rpx;
}

.rssi-bar:nth-child(1) { height: 12rpx; }
.rssi-bar:nth-child(2) { height: 20rpx; }
.rssi-bar:nth-child(3) { height: 28rpx; }
.rssi-bar:nth-child(4) { height: 36rpx; }

.rssi-bar.active {
  background: #e63946;
}

.rssi-value {
  font-size: 22rpx;
  color: #666;
}

.connecting-overlay {
  position: absolute;
  top: 0;
  left: 0;
  right: 0;
  bottom: 0;
  display: flex;
  flex-direction: row;
  align-items: center;
  justify-content: center;
  gap: 16rpx;
  background: rgba(15, 15, 35, 0.85);
  border-radius: 24rpx;
}

.connect-spinner {
  width: 36rpx;
  height: 36rpx;
  border: 4rpx solid rgba(230, 57, 70, 0.3);
  border-top-color: #e63946;
  border-radius: 50%;
  animation: spin 0.8s linear infinite;
}

@keyframes spin {
  to { transform: rotate(360deg); }
}

.connecting-text {
  color: #e63946;
  font-size: 28rpx;
  font-weight: 500;
}

.bottom-actions {
  margin-top: 40rpx;
}
</style>
