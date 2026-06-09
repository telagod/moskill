<template>
  <view class="dashboard">
    <!-- Custom Nav Bar -->
    <view class="nav-bar">
      <view class="nav-left">
        <text class="nav-title">MosKill</text>
      </view>
      <view class="nav-right">
        <view class="nav-battery flex-row gap-20">
          <view class="battery-icon">
            <view class="battery-fill" :style="{ width: battery + '%' }"></view>
          </view>
          <text class="battery-text">{{ battery }}%</text>
        </view>
        <view class="ble-indicator" :class="{ connected: connected }">
          <text class="ble-dot">●</text>
          <text class="ble-label">{{ connected ? 'BLE' : 'OFF' }}</text>
        </view>
      </view>
    </view>

    <!-- Main Content -->
    <scroll-view scroll-y class="dash-scroll">
      <view class="container">
        <!-- Kill Counter Section -->
        <view class="card kill-counter-card">
          <view class="kill-counter-wrap">
            <text class="kill-label">{{ t('dashboard.kills') }}</text>
            <text class="kill-number text-red">{{ animatedKillCount }}</text>
            <view class="streak-row flex-row gap-20" v-if="session">
              <mk-icon name="fire" :size="32" color="#f39c12"/>
              <text class="streak-value">{{ session.maxStreak }} {{ t('dashboard.streak') }}</text>
            </view>
          </view>
        </view>

        <!-- Classification Grid -->
        <view class="card">
          <text class="card-title">{{ t('dashboard.killClassification') }}</text>
          <view class="class-grid">
            <view class="class-item" v-for="(cls, idx) in classifications" :key="cls.label">
              <mk-icon :name="cls.icon" :size="48" :color="cls.color || '#e0e0e0'"/>
              <text class="class-count">{{ cls.count }}</text>
              <text class="class-label">{{ cls.label }}</text>
            </view>
          </view>
        </view>

        <!-- Environment -->
        <view class="card">
          <text class="card-title">{{ t('dashboard.environment') }}</text>
          <view class="env-row flex-between">
            <view class="env-item">
              <mk-icon name="thermometer" :size="40" color="#e63946"/>
              <text class="env-value">{{ fmtNum(env.temp) }}°C</text>
              <text class="env-label text-small">{{ t('dashboard.temp') }}</text>
            </view>
            <view class="env-item">
              <mk-icon name="droplet" :size="40" color="#3498db"/>
              <text class="env-value">{{ fmtNum(env.humi) }}%</text>
              <text class="env-label text-small">{{ t('dashboard.humi') }}</text>
            </view>
            <view class="env-item">
              <mk-icon name="clock" :size="40" color="#2ecc71"/>
              <text class="env-value">{{ sessionTime }}</text>
              <text class="env-label text-small">{{ t('dashboard.duration') }}</text>
            </view>
          </view>
        </view>

        <!-- Quick Actions -->
        <view class="card">
          <text class="card-title">{{ t('dashboard.quickActions') }}</text>
          <view class="action-grid">
            <view class="action-btn" @tap="toggleHV">
              <mk-icon name="lightning" :size="44" color="#f39c12"/>
              <text class="action-label">{{ hvOn ? t('dashboard.hvOn') : t('dashboard.hvOff') }}</text>
            </view>
            <view class="action-btn" @tap="goStats">
              <mk-icon name="chart" :size="44" color="#3498db"/>
              <text class="action-label">{{ t('tab.stats') }}</text>
            </view>
            <view class="action-btn" @tap="goControl">
              <mk-icon name="gear" :size="44" color="#888"/>
              <text class="action-label">{{ t('tab.control') }}</text>
            </view>
          </view>
        </view>

        <!-- Kill Feed -->
        <view class="card">
          <text class="card-title">{{ t('dashboard.killFeed') }}</text>
          <view class="kill-feed">
            <view class="feed-item" v-for="(item, idx) in killFeed" :key="item.time + idx">
              <mk-icon name="skull" :size="24" color="#e63946"/>
              <text class="feed-count">Kill #{{ item.count }}</text>
              <text class="feed-time text-small">{{ formatTime(item.time) }}</text>
            </view>
            <view v-if="killFeed.length === 0" class="feed-empty">
              <text class="text-small">{{ t('dashboard.noKills') }}</text>
            </view>
          </view>
        </view>
      </view>
    </scroll-view>
  </view>
</template>

<script>
import { ref, computed, onMounted, onUnmounted, watch } from 'vue'
import { useStore } from 'vuex'
import { subscribeKillCount, subscribeEnv, isConnected, readSession } from '@/utils/ble.js'
import { useLang } from '@/utils/i18n.js'
import { onMock, offMock } from '@/utils/mock.js'

export default {
  setup() {
    const store = useStore()
    const { t } = useLang()
    const hvOn = ref(true)
    const animatedKillCount = ref(0)
    const sessionStart = ref(Date.now())
    const sessionTime = ref('00:00:00')
    let timerInterval = null
    let animFrame = null
    let killCb = null
    let envCb = null

    const connected = computed(() => store.state.connected)
    const battery = computed(() => store.state.battery || 100)
    const env = computed(() => store.state.env)
    const session = computed(() => store.state.session)
    const killCount = computed(() => store.state.killCount)
    const killFeed = computed(() => store.state.killFeed.slice(0, 10))

    const classifications = computed(() => {
      const s = session.value
      if (!s) return [
        { icon: 'fruit-fly', label: 'S', count: 0 },
        { icon: 'mosquito', label: 'M', count: 0 },
        { icon: 'fly', label: 'L', count: 0 },
        { icon: 'moth', label: 'XL', count: 0 },
      ]
      return [
        { icon: 'fruit-fly', label: 'S', count: s.killsS },
        { icon: 'mosquito', label: 'M', count: s.killsM },
        { icon: 'fly', label: 'L', count: s.killsL },
        { icon: 'moth', label: 'XL', count: s.killsXL },
      ]
    })

    // Animate kill count
    watch(killCount, (newVal) => {
      const start = animatedKillCount.value
      const diff = newVal - start
      const duration = 500
      const startTime = Date.now()

      function step() {
        const elapsed = Date.now() - startTime
        const progress = Math.min(elapsed / duration, 1)
        animatedKillCount.value = Math.round(start + diff * progress)
        if (progress < 1) {
          animFrame = setTimeout(step, 16)
        }
      }
      step()
    }, { immediate: true })

    function updateTimer() {
      const elapsed = Math.floor((Date.now() - sessionStart.value) / 1000)
      const h = String(Math.floor(elapsed / 3600)).padStart(2, '0')
      const m = String(Math.floor((elapsed % 3600) / 60)).padStart(2, '0')
      const s = String(elapsed % 60).padStart(2, '0')
      sessionTime.value = `${h}:${m}:${s}`
    }

    function fmtNum(v) {
      return typeof v === 'number' ? v.toFixed(1) : v
    }

    function formatTime(ts) {
      const d = new Date(ts)
      return d.toLocaleTimeString('en-US', { hour12: false, hour: '2-digit', minute: '2-digit', second: '2-digit' })
    }

    function toggleHV() {
      hvOn.value = !hvOn.value
      uni.showToast({ title: hvOn.value ? 'HV Enabled' : 'HV Disabled', icon: 'none' })
    }

    function goStats() {
      uni.switchTab({ url: '/pages/stats/stats' })
    }

    function goControl() {
      uni.switchTab({ url: '/pages/control/control' })
    }

    onMounted(async () => {
      timerInterval = setInterval(updateTimer, 1000)

      if (store.state.devMode) {
        const killHandler = (count) => store.commit('SET_KILL_COUNT', count)
        const envHandler = (data) => store.commit('SET_ENV', data)
        onMock('kill', killHandler)
        onMock('env', envHandler)
        // Store handlers for cleanup
        killCb = killHandler
        envCb = envHandler
      } else if (isConnected()) {
        subscribeKillCount((count) => {
          store.commit('SET_KILL_COUNT', count)
        })
        subscribeEnv((data) => {
          store.commit('SET_ENV', data)
        })

        try {
          const sess = await readSession()
          store.commit('SET_SESSION', sess)
          if (sess.startTime) {
            sessionStart.value = sess.startTime * 1000
          }
        } catch (e) {
          console.warn('Failed to read session:', e)
        }
      }
    })

    onUnmounted(() => {
      if (timerInterval) clearInterval(timerInterval)
      if (animFrame) clearTimeout(animFrame)
      if (store.state.devMode) {
        if (killCb) offMock('kill', killCb)
        if (envCb) offMock('env', envCb)
      }
    })

    return {
      t,
      connected, battery, env, session, killFeed,
      animatedKillCount, classifications, sessionTime,
      fmtNum, hvOn, toggleHV, goStats, goControl, formatTime
    }
  }
}
</script>

<style scoped>
.dashboard {
  min-height: 100vh;
  background: #0f0f23;
}

.nav-bar {
  display: flex;
  flex-direction: row;
  align-items: center;
  justify-content: space-between;
  padding: 60rpx 30rpx 20rpx;
  background: linear-gradient(180deg, #1a1a2e 0%, transparent 100%);
  position: sticky;
  top: 0;
  z-index: 100;
}

.nav-title {
  font-size: 36rpx;
  font-weight: 700;
  color: #fff;
}

.nav-right {
  display: flex;
  flex-direction: row;
  align-items: center;
  gap: 24rpx;
}

.nav-battery {
  display: flex;
  flex-direction: row;
  align-items: center;
  gap: 8rpx;
}

.battery-icon {
  width: 44rpx;
  height: 22rpx;
  border: 2rpx solid #666;
  border-radius: 4rpx;
  padding: 2rpx;
  position: relative;
}

.battery-icon::after {
  content: '';
  position: absolute;
  right: -6rpx;
  top: 6rpx;
  width: 4rpx;
  height: 10rpx;
  background: #666;
  border-radius: 0 2rpx 2rpx 0;
}

.battery-fill {
  height: 100%;
  background: #2ecc71;
  border-radius: 2rpx;
  transition: width 0.3s;
}

.battery-text {
  font-size: 22rpx;
  color: #888;
}

.ble-indicator {
  display: flex;
  flex-direction: row;
  align-items: center;
  gap: 6rpx;
  padding: 6rpx 14rpx;
  border-radius: 20rpx;
  background: rgba(255, 255, 255, 0.05);
}

.ble-indicator.connected .ble-dot {
  color: #2ecc71;
}

.ble-dot {
  font-size: 16rpx;
  color: #e63946;
}

.ble-label {
  font-size: 20rpx;
  color: #888;
}

.dash-scroll {
  height: calc(100vh - 120rpx);
}

.kill-counter-card {
  text-align: center;
  padding: 50rpx 30rpx;
  background: linear-gradient(135deg, #1a1a2e 0%, #2d1b2e 50%, #1a1a2e 100%);
  border: 1rpx solid rgba(230, 57, 70, 0.2);
}

.kill-counter-wrap {
  display: flex;
  flex-direction: column;
  align-items: center;
}

.kill-label {
  font-size: 24rpx;
  color: #888;
  letter-spacing: 6rpx;
  margin-bottom: 12rpx;
}

.kill-number {
  font-size: 128rpx;
  font-weight: 800;
  line-height: 1.1;
  text-shadow: 0 0 60rpx rgba(230, 57, 70, 0.4);
}

.streak-row {
  margin-top: 16rpx;
  display: flex;
  flex-direction: row;
  align-items: center;
  gap: 8rpx;
}

.streak-icon {
  font-size: 32rpx;
}

.streak-value {
  font-size: 28rpx;
  color: #f39c12;
  font-weight: 600;
}

.class-grid {
  display: flex;
  flex-direction: row;
  justify-content: space-around;
}

.class-item {
  display: flex;
  flex-direction: column;
  align-items: center;
  gap: 8rpx;
}

.class-icon {
  font-size: 48rpx;
}

.class-count {
  font-size: 36rpx;
  font-weight: 700;
  color: #fff;
}

.class-label {
  font-size: 22rpx;
  color: #666;
}

.env-row {
  display: flex;
  flex-direction: row;
  justify-content: space-around;
}

.env-item {
  flex: 1;
  display: flex;
  flex-direction: column;
  align-items: center;
  gap: 8rpx;
  overflow: hidden;
}

.env-icon {
  font-size: 40rpx;
}

.env-value {
  font-size: 32rpx;
  font-weight: 600;
  color: #fff;
}

.env-label {
  font-size: 22rpx;
}

.action-grid {
  display: flex;
  flex-direction: row;
  justify-content: space-around;
  gap: 20rpx;
}

.action-btn {
  display: flex;
  flex-direction: column;
  align-items: center;
  gap: 12rpx;
  padding: 24rpx 36rpx;
  background: rgba(255, 255, 255, 0.05);
  border-radius: 16rpx;
  border: 1rpx solid rgba(255, 255, 255, 0.08);
  transition: background 0.2s;
}

.action-btn:active {
  background: rgba(230, 57, 70, 0.15);
}

.action-icon {
  font-size: 44rpx;
}

.action-label {
  font-size: 24rpx;
  color: #aaa;
}

.kill-feed {
  max-height: 400rpx;
  overflow: hidden;
}

.feed-item {
  display: flex;
  flex-direction: row;
  align-items: center;
  gap: 16rpx;
  padding: 16rpx 0;
  border-bottom: 1rpx solid rgba(255, 255, 255, 0.05);
}

.feed-item:last-child {
  border-bottom: none;
}

.feed-dot {
  font-size: 24rpx;
}

.feed-count {
  font-size: 28rpx;
  color: #e0e0e0;
  flex: 1;
}

.feed-time {
  font-size: 22rpx;
  color: #555;
}

.feed-empty {
  text-align: center;
  padding: 40rpx;
}
</style>
