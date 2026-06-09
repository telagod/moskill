<template>
  <view class="splash-container">
    <view class="splash-content">
      <view class="logo-section">
        <text class="mosquito-anim">{{ mosquitoFrame }}</text>
        <text class="logo-text">MOS<text class="logo-accent">KILL</text></text>
        <text class="logo-subtitle">Smart Insect Eliminator</text>
      </view>

      <view class="pulse-ring" :class="{ active: !connected }"></view>

      <view class="action-section">
        <button class="btn-primary connect-btn" @tap="handleConnect" :disabled="connecting">
          <text v-if="connecting" class="btn-loading">⟳</text>
          <text>{{ connected ? 'Go to Dashboard' : 'Connect Device' }}</text>
        </button>
      </view>
    </view>

    <view class="version-footer">
      <text class="text-small">v1.0.0 · MosKill BLE</text>
    </view>
  </view>
</template>

<script>
import { ref, onMounted, onUnmounted, computed } from 'vue'
import { useStore } from 'vuex'
import { isConnected } from '@/utils/ble.js'

export default {
  setup() {
    const store = useStore()
    const connecting = ref(false)
    const mosquitoFrame = ref('🦟')
    const frames = ['🦟', '✦', '🦟', '⚡', '🦟', '💀']
    let frameIdx = 0
    let animTimer = null

    const connected = computed(() => store.state.connected)

    onMounted(() => {
      animTimer = setInterval(() => {
        frameIdx = (frameIdx + 1) % frames.length
        mosquitoFrame.value = frames[frameIdx]
      }, 600)

      // Auto-navigate if already connected
      if (isConnected() && store.state.connected) {
        setTimeout(() => {
          uni.switchTab({ url: '/pages/dashboard/dashboard' })
        }, 800)
      }
    })

    onUnmounted(() => {
      if (animTimer) clearInterval(animTimer)
    })

    function handleConnect() {
      if (connected.value) {
        uni.switchTab({ url: '/pages/dashboard/dashboard' })
      } else {
        connecting.value = true
        uni.navigateTo({
          url: '/pages/connect/connect',
          complete() {
            connecting.value = false
          }
        })
      }
    }

    return { connected, connecting, mosquitoFrame, handleConnect }
  }
}
</script>

<style scoped>
.splash-container {
  min-height: 100vh;
  display: flex;
  flex-direction: column;
  align-items: center;
  justify-content: center;
  background: radial-gradient(ellipse at center, #1a1a2e 0%, #0f0f23 70%);
  position: relative;
  overflow: hidden;
}

.splash-content {
  display: flex;
  flex-direction: column;
  align-items: center;
  justify-content: center;
  flex: 1;
}

.logo-section {
  display: flex;
  flex-direction: column;
  align-items: center;
  margin-bottom: 80rpx;
}

.mosquito-anim {
  font-size: 120rpx;
  margin-bottom: 40rpx;
  animation: float 3s ease-in-out infinite;
}

@keyframes float {
  0%, 100% { transform: translateY(0rpx); }
  50% { transform: translateY(-20rpx); }
}

.logo-text {
  font-size: 96rpx;
  font-weight: 800;
  color: #ffffff;
  letter-spacing: 8rpx;
  text-shadow: 0 0 40rpx rgba(230, 57, 70, 0.3);
}

.logo-accent {
  color: #e63946;
}

.logo-subtitle {
  font-size: 28rpx;
  color: #666;
  margin-top: 16rpx;
  letter-spacing: 4rpx;
  text-transform: uppercase;
}

.pulse-ring {
  width: 200rpx;
  height: 200rpx;
  border-radius: 50%;
  margin-bottom: 80rpx;
}

.pulse-ring.active {
  animation: pulse 2s ease-out infinite;
  border: 4rpx solid rgba(230, 57, 70, 0.4);
}

@keyframes pulse {
  0% { transform: scale(0.8); opacity: 1; border-color: rgba(230, 57, 70, 0.6); }
  100% { transform: scale(2); opacity: 0; border-color: rgba(230, 57, 70, 0); }
}

.action-section {
  width: 600rpx;
}

.connect-btn {
  width: 100%;
  height: 100rpx;
  display: flex;
  flex-direction: row;
  align-items: center;
  justify-content: center;
  gap: 16rpx;
}

.btn-loading {
  animation: spin 1s linear infinite;
  display: inline-block;
}

@keyframes spin {
  from { transform: rotate(0deg); }
  to { transform: rotate(360deg); }
}

.version-footer {
  position: absolute;
  bottom: 60rpx;
  text-align: center;
}
</style>
