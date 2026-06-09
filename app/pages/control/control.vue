<template>
  <view class="container">
    <!-- Sensitivity -->
    <view class="card">
      <text class="card-title">Sensitivity</text>
      <view class="sensitivity-selector">
        <view
          class="sens-option"
          v-for="opt in sensitivityOptions"
          :key="opt.value"
          :class="{ active: config.sensitivity === opt.value }"
          @tap="config.sensitivity = opt.value"
        >
          <text class="sens-icon">{{ opt.icon }}</text>
          <text class="sens-label">{{ opt.label }}</text>
        </view>
      </view>
      <view class="sens-indicator">
        <view class="sens-bar" :style="{ width: ((config.sensitivity + 1) / 3 * 100) + '%' }"></view>
      </view>
    </view>

    <!-- LED Brightness -->
    <view class="card">
      <text class="card-title">LED Brightness</text>
      <view class="slider-wrap">
        <text class="slider-icon">🌑</text>
        <slider
          :value="config.ledBrightness"
          :min="0"
          :max="255"
          :step="1"
          activeColor="#e63946"
          backgroundColor="#333"
          block-color="#ff6b6b"
          block-size="24"
          @change="config.ledBrightness = $event.detail.value"
        />
        <text class="slider-icon">☀️</text>
      </view>
      <text class="slider-value text-small">{{ config.ledBrightness }}/255</text>
    </view>

    <!-- Buzzer Volume -->
    <view class="card">
      <text class="card-title">Buzzer Volume</text>
      <view class="volume-selector">
        <view
          class="vol-option"
          v-for="opt in volumeOptions"
          :key="opt.value"
          :class="{ active: config.buzzerVolume === opt.value }"
          @tap="config.buzzerVolume = opt.value"
        >
          <text class="vol-label">{{ opt.label }}</text>
        </view>
      </view>
    </view>

    <!-- Toggle Switches -->
    <view class="card">
      <text class="card-title">Effects</text>
      <view class="toggle-list">
        <view class="toggle-item flex-between">
          <view class="toggle-info">
            <text class="toggle-label">🔊 Buzzer on Kill</text>
          </view>
          <switch :checked="config.buzzerOnKill" @change="config.buzzerOnKill = $event.detail.value" color="#e63946" />
        </view>
        <view class="toggle-item flex-between">
          <view class="toggle-info">
            <text class="toggle-label">💡 LED on Kill</text>
          </view>
          <switch :checked="config.ledOnKill" @change="config.ledOnKill = $event.detail.value" color="#e63946" />
        </view>
        <view class="toggle-item flex-between">
          <view class="toggle-info">
            <text class="toggle-label">✨ Streak Effects</text>
          </view>
          <switch :checked="config.streakEffects" @change="config.streakEffects = $event.detail.value" color="#e63946" />
        </view>
      </view>
    </view>

    <!-- Actions -->
    <view class="card">
      <text class="card-title">Actions</text>
      <view class="action-list">
        <button class="btn-primary mb-20" @tap="applyConfig" :loading="applying">
          ✅ Apply Configuration
        </button>
        <button class="btn-secondary mb-20" @tap="doSyncTime">
          🕐 Sync Time
        </button>
      </view>
    </view>

    <!-- Device Info -->
    <view class="card">
      <text class="card-title">Device Information</text>
      <view class="info-list">
        <view class="info-item flex-between">
          <text class="info-label">Device Name</text>
          <text class="info-value">{{ deviceName }}</text>
        </view>
        <view class="info-item flex-between">
          <text class="info-label">Device ID</text>
          <text class="info-value info-id">{{ deviceIdShort }}</text>
        </view>
        <view class="info-item flex-between">
          <text class="info-label">Connection</text>
          <text class="info-value" :class="connected ? 'text-green' : 'text-red'">
            {{ connected ? 'Connected' : 'Disconnected' }}
          </text>
        </view>
        <view class="info-item flex-between">
          <text class="info-label">Battery</text>
          <text class="info-value">{{ battery }}%</text>
        </view>
      </view>
    </view>

    <!-- Bottom Actions -->
    <view class="bottom-buttons">
      <button class="btn-secondary mb-20" @tap="goOTA">
        📦 Firmware Update
      </button>
      <button class="btn-danger" @tap="confirmDisconnect">
        🔌 Disconnect Device
      </button>
    </view>
  </view>
</template>

<script>
import { reactive, computed, onMounted, ref } from 'vue'
import { useStore } from 'vuex'
import { writeConfig, readConfig, syncTime, disconnect, isConnected } from '@/utils/ble.js'

export default {
  setup() {
    const store = useStore()
    const applying = ref(false)

    const config = reactive({
      sensitivity: 1,
      ledBrightness: 128,
      buzzerVolume: 2,
      buzzerOnKill: true,
      ledOnKill: true,
      streakEffects: true,
    })

    const sensitivityOptions = [
      { value: 0, label: 'Low', icon: '🐢' },
      { value: 1, label: 'Med', icon: '🦊' },
      { value: 2, label: 'High', icon: '⚡' },
    ]

    const volumeOptions = [
      { value: 0, label: 'Off' },
      { value: 1, label: 'Low' },
      { value: 2, label: 'Med' },
      { value: 3, label: 'High' },
    ]

    const connected = computed(() => store.state.connected)
    const battery = computed(() => store.state.battery || 100)
    const deviceName = computed(() => store.state.deviceName || 'MosKill')
    const deviceIdShort = computed(() => {
      const id = store.state.deviceId || ''
      return id.length > 17 ? id.slice(0, 17) + '...' : id
    })

    async function applyConfig() {
      if (!isConnected()) {
        uni.showToast({ title: 'Not connected', icon: 'none' })
        return
      }
      applying.value = true
      try {
        await writeConfig({
          sensitivity: config.sensitivity,
          ledBrightness: config.ledBrightness,
          buzzerVolume: config.buzzerVolume,
          buzzerOnKill: config.buzzerOnKill,
          ledOnKill: config.ledOnKill,
          streakEffects: config.streakEffects,
        })
        store.commit('SET_CONFIG', { ...config })
        uni.showToast({ title: 'Config applied!', icon: 'success' })
      } catch (e) {
        uni.showToast({ title: 'Failed to apply', icon: 'none' })
      } finally {
        applying.value = false
      }
    }

    async function doSyncTime() {
      if (!isConnected()) {
        uni.showToast({ title: 'Not connected', icon: 'none' })
        return
      }
      try {
        await syncTime()
        uni.showToast({ title: 'Time synced!', icon: 'success' })
      } catch (e) {
        uni.showToast({ title: 'Sync failed', icon: 'none' })
      }
    }

    function goOTA() {
      uni.navigateTo({ url: '/pages/ota/ota' })
    }

    function confirmDisconnect() {
      uni.showModal({
        title: 'Disconnect',
        content: 'Are you sure you want to disconnect from the device?',
        confirmColor: '#e63946',
        success(res) {
          if (res.confirm) {
            doDisconnect()
          }
        }
      })
    }

    async function doDisconnect() {
      try {
        await disconnect()
        store.commit('SET_CONNECTED', { connected: false, deviceId: null, deviceName: '' })
        uni.reLaunch({ url: '/pages/index/index' })
      } catch (e) {
        uni.showToast({ title: 'Disconnect failed', icon: 'none' })
      }
    }

    onMounted(async () => {
      // Load current config from device
      if (isConnected()) {
        try {
          const cfg = await readConfig()
          config.sensitivity = cfg.sensitivity
          config.ledBrightness = cfg.ledBrightness
          config.buzzerVolume = cfg.buzzerVolume
          config.buzzerOnKill = cfg.buzzerOnKill
          config.ledOnKill = cfg.ledOnKill
          config.streakEffects = cfg.streakEffects
          store.commit('SET_CONFIG', cfg)
        } catch (e) {
          // Use store defaults
          const storeConfig = store.state.config
          Object.assign(config, storeConfig)
        }
      } else {
        const storeConfig = store.state.config
        Object.assign(config, storeConfig)
      }
    })

    return {
      config, applying, sensitivityOptions, volumeOptions,
      connected, battery, deviceName, deviceIdShort,
      applyConfig, doSyncTime, goOTA, confirmDisconnect
    }
  }
}
</script>

<style scoped>
.sensitivity-selector {
  display: flex;
  flex-direction: row;
  gap: 16rpx;
  margin-bottom: 20rpx;
}

.sens-option {
  flex: 1;
  display: flex;
  flex-direction: column;
  align-items: center;
  gap: 8rpx;
  padding: 24rpx 16rpx;
  background: rgba(255, 255, 255, 0.04);
  border-radius: 16rpx;
  border: 2rpx solid transparent;
  transition: all 0.2s;
}

.sens-option.active {
  background: rgba(230, 57, 70, 0.15);
  border-color: #e63946;
}

.sens-icon {
  font-size: 40rpx;
}

.sens-label {
  font-size: 24rpx;
  color: #aaa;
}

.sens-option.active .sens-label {
  color: #e63946;
  font-weight: 600;
}

.sens-indicator {
  height: 8rpx;
  background: #222;
  border-radius: 4rpx;
  overflow: hidden;
}

.sens-bar {
  height: 100%;
  background: linear-gradient(90deg, #e63946 0%, #ff6b6b 100%);
  border-radius: 4rpx;
  transition: width 0.3s;
}

.slider-wrap {
  display: flex;
  flex-direction: row;
  align-items: center;
  gap: 16rpx;
}

.slider-icon {
  font-size: 32rpx;
}

.slider-value {
  text-align: center;
  margin-top: 8rpx;
}

.volume-selector {
  display: flex;
  flex-direction: row;
  gap: 12rpx;
}

.vol-option {
  flex: 1;
  padding: 20rpx 16rpx;
  text-align: center;
  background: rgba(255, 255, 255, 0.04);
  border-radius: 12rpx;
  border: 2rpx solid transparent;
  transition: all 0.2s;
}

.vol-option.active {
  background: rgba(230, 57, 70, 0.15);
  border-color: #e63946;
}

.vol-label {
  font-size: 26rpx;
  color: #aaa;
}

.vol-option.active .vol-label {
  color: #e63946;
  font-weight: 600;
}

.toggle-list {
  display: flex;
  flex-direction: column;
  gap: 24rpx;
}

.toggle-item {
  padding: 8rpx 0;
}

.toggle-label {
  font-size: 30rpx;
  color: #ddd;
}

.action-list {
  display: flex;
  flex-direction: column;
}

.info-list {
  display: flex;
  flex-direction: column;
  gap: 20rpx;
}

.info-item {
  padding: 12rpx 0;
  border-bottom: 1rpx solid rgba(255, 255, 255, 0.05);
}

.info-item:last-child {
  border-bottom: none;
}

.info-label {
  font-size: 28rpx;
  color: #888;
}

.info-value {
  font-size: 28rpx;
  color: #fff;
  font-weight: 500;
}

.info-id {
  font-size: 22rpx;
  font-family: monospace;
}

.bottom-buttons {
  margin-top: 20rpx;
  padding-bottom: 40rpx;
}

.btn-danger {
  background: rgba(230, 57, 70, 0.15);
  color: #e63946;
  border: 1rpx solid rgba(230, 57, 70, 0.4);
  border-radius: 16rpx;
  padding: 24rpx 0;
  font-size: 30rpx;
  text-align: center;
  font-weight: 600;
}
</style>
