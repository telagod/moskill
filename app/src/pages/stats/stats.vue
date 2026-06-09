<template>
  <view class="container">
    <!-- Session Stats -->
    <view class="card">
      <text class="card-title">{{ t('stats.sessionStats') }}</text>
      <view class="stat-grid" v-if="session">
        <view class="stat-item">
          <text class="stat-value text-red">{{ session.killsTotal }}</text>
          <text class="stat-label">{{ t('stats.totalKills') }}</text>
        </view>
        <view class="stat-item">
          <text class="stat-value text-yellow">{{ session.maxStreak }}</text>
          <text class="stat-label">{{ t('stats.bestStreak') }}</text>
        </view>
        <view class="stat-item">
          <text class="stat-value text-green">{{ (session.efficiency * 100).toFixed(0) }}%</text>
          <text class="stat-label">{{ t('stats.efficiency') }}</text>
        </view>
        <view class="stat-item">
          <text class="stat-value text-blue">{{ formatDuration(session.duration) }}</text>
          <text class="stat-label">{{ t('dashboard.duration') }}</text>
        </view>
      </view>
      <view v-else class="stat-empty">
        <text class="text-small">{{ t('stats.noData') }}</text>
      </view>
    </view>

    <!-- Lifetime Stats -->
    <view class="card">
      <text class="card-title">{{ t('stats.lifetimeStats') }}</text>
      <view class="stat-grid" v-if="lifetime">
        <view class="stat-item">
          <text class="stat-value text-red">{{ lifetime.totalKills }}</text>
          <text class="stat-label">{{ t('stats.totalKills') }}</text>
        </view>
        <view class="stat-item">
          <text class="stat-value text-yellow">{{ lifetime.bestStreak }}</text>
          <text class="stat-label">{{ t('stats.bestStreak') }}</text>
        </view>
        <view class="stat-item">
          <text class="stat-value text-green">{{ lifetime.bestSession }}</text>
          <text class="stat-label">{{ t('stats.bestSession') }}</text>
        </view>
        <view class="stat-item">
          <text class="stat-value text-blue">{{ lifetime.totalSessions }}</text>
          <text class="stat-label">{{ t('stats.totalSessions') }}</text>
        </view>
      </view>
      <view v-else class="stat-empty">
        <button class="btn-secondary" @tap="loadStats">{{ t('stats.loadStats') }}</button>
      </view>
    </view>

    <!-- Kill Rate -->
    <view class="card" v-if="lifetime">
      <text class="card-title">{{ t('stats.killRateTitle') }}</text>
      <view class="rate-display">
        <text class="rate-number">{{ lifetime.killRate.toFixed(1) }}</text>
        <text class="rate-unit">{{ t('stats.perHour') }}</text>
      </view>
    </view>

    <!-- Classification Pie Chart -->
    <view class="card" v-if="lifetime">
      <text class="card-title">{{ t('stats.distribution') }}</text>
      <view class="pie-container">
        <view class="pie-chart" :style="{ background: pieGradient }"></view>
        <view class="pie-legend">
          <view class="legend-item" v-for="item in pieData" :key="item.label">
            <view class="legend-dot" :style="{ background: item.color }"></view>
            <view class="legend-label">
              <mk-icon :name="item.icon" :size="24" :color="item.color" :inline="true"/>
              <text>{{ item.label }}</text>
            </view>
            <text class="legend-value">{{ item.percent }}%</text>
          </view>
        </view>
      </view>
    </view>

    <!-- Hourly Histogram -->
    <view class="card" v-if="lifetime">
      <text class="card-title">{{ t('stats.hourly') }}</text>
      <view class="histogram">
        <view class="hist-bar-wrap" v-for="(val, idx) in lifetime.hourly" :key="'h'+idx">
          <view class="hist-bar" :style="{ height: barHeight(val, hourlyMax) + 'rpx' }"></view>
          <text class="hist-label" v-if="idx % 4 === 0">{{ idx }}h</text>
        </view>
      </view>
    </view>

    <!-- Daily Histogram -->
    <view class="card" v-if="lifetime">
      <text class="card-title">{{ t('stats.weekly') }}</text>
      <view class="histogram daily-hist">
        <view class="hist-bar-wrap daily-bar-wrap" v-for="(val, idx) in lifetime.daily" :key="'d'+idx">
          <view class="hist-bar daily-bar" :style="{ height: barHeight(val, dailyMax) + 'rpx' }"></view>
          <text class="hist-label">{{ dayLabels[idx] }}</text>
        </view>
      </view>
    </view>

    <!-- Records -->
    <view class="card" v-if="lifetime">
      <text class="card-title">{{ t('stats.records') }}</text>
      <view class="records-list">
        <view class="record-item flex-between">
          <text class="record-label"><mk-icon name="trophy" :size="28" color="#ffd700" :inline="true"/> {{ t('stats.bestSession') }}</text>
          <text class="record-value">{{ lifetime.bestSession }} kills</text>
        </view>
        <view class="record-item flex-between">
          <text class="record-label"><mk-icon name="fire" :size="28" color="#f39c12" :inline="true"/> {{ t('stats.bestStreak') }}</text>
          <text class="record-value">{{ lifetime.bestStreak }}</text>
        </view>
        <view class="record-item flex-between">
          <text class="record-label"><mk-icon name="lightning" :size="28" color="#e63946" :inline="true"/> {{ t('stats.killRate') }}</text>
          <text class="record-value">{{ lifetime.killRate.toFixed(1) }}/hr</text>
        </view>
        <view class="record-item flex-between">
          <text class="record-label"><mk-icon name="hourglass" :size="28" color="#3498db" :inline="true"/> {{ t('stats.totalActive') }}</text>
          <text class="record-value">{{ formatDuration(lifetime.totalActive) }}</text>
        </view>
      </view>
    </view>
  </view>
</template>

<script>
import { ref, computed, onMounted } from 'vue'
import { useStore } from 'vuex'
import { readSession, readLifetime, isConnected } from '@/utils/ble.js'
import { useLang } from '@/utils/i18n.js'

export default {
  setup() {
    const store = useStore()
    const { t } = useLang()
    const dayLabels = computed(() => t('stats.days'))

    const session = computed(() => store.state.session)
    const lifetime = computed(() => store.state.lifetime)

    const hourlyMax = computed(() => {
      if (!lifetime.value) return 1
      return Math.max(...lifetime.value.hourly, 1)
    })

    const dailyMax = computed(() => {
      if (!lifetime.value) return 1
      return Math.max(...lifetime.value.daily, 1)
    })

    const pieData = computed(() => {
      if (!lifetime.value) return []
      const total = lifetime.value.totalKills || 1
      const items = [
        { label: 'S', icon: 'fruit-fly', count: lifetime.value.killsS, color: '#3498db' },
        { label: 'M', icon: 'mosquito', count: lifetime.value.killsM, color: '#e63946' },
        { label: 'L', icon: 'fly', count: lifetime.value.killsL, color: '#2ecc71' },
        { label: 'XL', icon: 'moth', count: lifetime.value.killsXL, color: '#f39c12' },
      ]
      return items.map(it => ({
        ...it,
        percent: Math.round((it.count / total) * 100)
      }))
    })

    const pieGradient = computed(() => {
      if (!pieData.value.length) return '#333'
      let acc = 0
      const stops = []
      pieData.value.forEach(item => {
        const start = acc
        acc += item.percent
        stops.push(`${item.color} ${start}% ${acc}%`)
      })
      return `conic-gradient(${stops.join(', ')})`
    })

    function barHeight(val, max) {
      if (max === 0) return 4
      return Math.max(4, Math.round((val / max) * 160))
    }

    function formatDuration(seconds) {
      if (!seconds) return '0m'
      const h = Math.floor(seconds / 3600)
      const m = Math.floor((seconds % 3600) / 60)
      if (h > 0) return `${h}h ${m}m`
      return `${m}m`
    }

    async function loadStats() {
      if (!isConnected()) {
        uni.showToast({ title: t('stats.notConnected'), icon: 'none' })
        return
      }
      try {
        const sess = await readSession()
        store.commit('SET_SESSION', sess)
        const lt = await readLifetime()
        store.commit('SET_LIFETIME', lt)
      } catch (e) {
        uni.showToast({ title: t('stats.loadFailed'), icon: 'none' })
      }
    }

    onMounted(() => {
      if (isConnected() && !lifetime.value) {
        loadStats()
      }
    })

    return {
      t,
      session, lifetime, dayLabels,
      hourlyMax, dailyMax, pieData, pieGradient,
      barHeight, formatDuration, loadStats
    }
  }
}
</script>

<style scoped>
.stat-grid {
  display: flex;
  flex-direction: row;
  flex-wrap: wrap;
  gap: 20rpx;
}

.stat-item {
  flex: 1;
  min-width: 40%;
  display: flex;
  flex-direction: column;
  align-items: center;
  padding: 20rpx;
  background: rgba(255, 255, 255, 0.03);
  border-radius: 16rpx;
}

.stat-value {
  font-size: 44rpx;
  font-weight: 700;
}

.stat-label {
  font-size: 22rpx;
  color: #888;
  margin-top: 8rpx;
}

.stat-empty {
  text-align: center;
  padding: 40rpx;
}

.rate-display {
  display: flex;
  flex-direction: row;
  align-items: baseline;
  justify-content: center;
  gap: 12rpx;
}

.rate-number {
  font-size: 72rpx;
  font-weight: 800;
  color: #e63946;
}

.rate-unit {
  font-size: 26rpx;
  color: #888;
}

.pie-container {
  display: flex;
  flex-direction: row;
  align-items: center;
  gap: 40rpx;
}

.pie-chart {
  width: 200rpx;
  height: 200rpx;
  border-radius: 50%;
  flex-shrink: 0;
}

.pie-legend {
  display: flex;
  flex-direction: column;
  gap: 16rpx;
  flex: 1;
}

.legend-item {
  display: flex;
  flex-direction: row;
  align-items: center;
  gap: 12rpx;
}

.legend-dot {
  width: 20rpx;
  height: 20rpx;
  border-radius: 50%;
  flex-shrink: 0;
}

.legend-label {
  display: flex;
  flex-direction: row;
  align-items: center;
  gap: 8rpx;
  font-size: 26rpx;
  color: #ccc;
  flex: 1;
}

.legend-value {
  font-size: 26rpx;
  color: #888;
  font-weight: 600;
}

.histogram {
  display: flex;
  flex-direction: row;
  align-items: flex-end;
  height: 200rpx;
  gap: 4rpx;
  padding-top: 20rpx;
}

.hist-bar-wrap {
  flex: 1;
  display: flex;
  flex-direction: column;
  align-items: center;
  justify-content: flex-end;
  height: 100%;
}

.hist-bar {
  width: 100%;
  background: linear-gradient(180deg, #e63946 0%, #ff6b6b 100%);
  border-radius: 4rpx 4rpx 0 0;
  min-height: 4rpx;
  transition: height 0.3s;
}

.hist-label {
  font-size: 18rpx;
  color: #666;
  margin-top: 8rpx;
}

.daily-hist {
  gap: 16rpx;
}

.daily-bar-wrap {
  flex: 1;
}

.daily-bar {
  width: 80%;
}

.records-list {
  display: flex;
  flex-direction: column;
  gap: 20rpx;
}

.record-item {
  padding: 16rpx 0;
  border-bottom: 1rpx solid rgba(255, 255, 255, 0.05);
}

.record-item:last-child {
  border-bottom: none;
}

.record-label {
  font-size: 28rpx;
  color: #ccc;
}

.record-value {
  font-size: 28rpx;
  color: #fff;
  font-weight: 600;
}
</style>
