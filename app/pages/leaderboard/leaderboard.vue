<template>
  <view class="container">
    <!-- Sort Tabs -->
    <view class="sort-tabs">
      <view
        class="sort-tab"
        v-for="tab in sortOptions"
        :key="tab.key"
        :class="{ active: sortBy === tab.key }"
        @tap="sortBy = tab.key"
      >
        <text class="sort-tab-text">{{ tab.label }}</text>
      </view>
    </view>

    <!-- Top 3 Podium -->
    <view class="podium" v-if="sortedBoard.length >= 3">
      <view class="podium-item silver">
        <text class="podium-medal">🥈</text>
        <text class="podium-name">{{ sortedBoard[1].name }}</text>
        <text class="podium-score">{{ getScore(sortedBoard[1]) }}</text>
        <view class="podium-bar bar-2"></view>
      </view>
      <view class="podium-item gold">
        <text class="podium-medal">🥇</text>
        <text class="podium-name">{{ sortedBoard[0].name }}</text>
        <text class="podium-score">{{ getScore(sortedBoard[0]) }}</text>
        <view class="podium-bar bar-1"></view>
      </view>
      <view class="podium-item bronze">
        <text class="podium-medal">🥉</text>
        <text class="podium-name">{{ sortedBoard[2].name }}</text>
        <text class="podium-score">{{ getScore(sortedBoard[2]) }}</text>
        <view class="podium-bar bar-3"></view>
      </view>
    </view>

    <!-- Leaderboard List -->
    <view class="board-list">
      <view
        class="card board-item"
        v-for="(entry, idx) in sortedBoard"
        :key="entry.id"
        @touchstart="touchStart(entry.id, $event)"
        @touchmove="touchMove(entry.id, $event)"
        @touchend="touchEnd(entry.id)"
      >
        <view class="board-item-content" :style="{ transform: 'translateX(' + (swipeOffsets[entry.id] || 0) + 'rpx)' }">
          <view class="rank-badge" :class="rankClass(idx)">
            <text class="rank-num">{{ idx + 1 }}</text>
          </view>
          <view class="board-info">
            <text class="board-name">{{ entry.name }}</text>
            <text class="board-meta text-small">{{ entry.date }} · {{ formatDuration(entry.duration) }}</text>
          </view>
          <view class="board-stats">
            <text class="board-kills">{{ entry.totalKills }}</text>
            <text class="board-streak text-small">🔥{{ entry.streak }}</text>
          </view>
        </view>
        <!-- Delete button revealed on swipe -->
        <view class="delete-btn" v-if="(swipeOffsets[entry.id] || 0) < -80" @tap="deleteEntry(entry.id)">
          <text class="delete-text">Delete</text>
        </view>
      </view>

      <view v-if="sortedBoard.length === 0" class="empty-state">
        <text class="empty-icon">🏆</text>
        <text class="empty-title">No Records Yet</text>
        <text class="empty-sub text-small">Complete a session to appear here</text>
      </view>
    </view>

    <!-- Save Session Button -->
    <view class="bottom-action">
      <button class="btn-primary" @tap="saveSession">
        💾 Save Current Session
      </button>
    </view>
  </view>
</template>

<script>
import { ref, computed, onMounted, reactive } from 'vue'
import { useStore } from 'vuex'

export default {
  setup() {
    const store = useStore()
    const sortBy = ref('totalKills')
    const swipeOffsets = reactive({})
    let touchStartX = 0
    let touchId = null

    const sortOptions = [
      { key: 'totalKills', label: 'Kills' },
      { key: 'streak', label: 'Streak' },
      { key: 'efficiency', label: 'Efficiency' },
      { key: 'killsM', label: 'Mosquitoes' },
    ]

    const leaderboard = computed(() => store.state.leaderboard)

    const sortedBoard = computed(() => {
      const list = [...leaderboard.value]
      list.sort((a, b) => {
        const key = sortBy.value
        return (b[key] || 0) - (a[key] || 0)
      })
      return list
    })

    function getScore(entry) {
      const key = sortBy.value
      if (key === 'efficiency') return (entry.efficiency * 100).toFixed(0) + '%'
      return entry[key] || 0
    }

    function rankClass(idx) {
      if (idx === 0) return 'rank-gold'
      if (idx === 1) return 'rank-silver'
      if (idx === 2) return 'rank-bronze'
      return ''
    }

    function formatDuration(seconds) {
      if (!seconds) return '0m'
      const h = Math.floor(seconds / 3600)
      const m = Math.floor((seconds % 3600) / 60)
      if (h > 0) return `${h}h ${m}m`
      return `${m}m`
    }

    function touchStart(id, e) {
      touchStartX = e.touches[0].clientX
      touchId = id
    }

    function touchMove(id, e) {
      if (touchId !== id) return
      const diff = (e.touches[0].clientX - touchStartX) * 2
      if (diff < 0) {
        swipeOffsets[id] = Math.max(diff, -200)
      } else {
        swipeOffsets[id] = 0
      }
    }

    function touchEnd(id) {
      if (swipeOffsets[id] > -80) {
        swipeOffsets[id] = 0
      }
      touchId = null
    }

    function deleteEntry(id) {
      const updated = leaderboard.value.filter(e => e.id !== id)
      store.commit('SET_LEADERBOARD', updated)
      uni.setStorageSync('moskill_leaderboard', updated)
      delete swipeOffsets[id]
      uni.showToast({ title: 'Deleted', icon: 'none' })
    }

    function saveSession() {
      if (!store.state.session) {
        uni.showToast({ title: 'No active session', icon: 'none' })
        return
      }
      store.dispatch('saveScore')
      uni.showToast({ title: 'Session saved!', icon: 'success' })
    }

    onMounted(() => {
      store.dispatch('loadLeaderboard')
    })

    return {
      sortBy, sortOptions, sortedBoard, swipeOffsets,
      getScore, rankClass, formatDuration,
      touchStart, touchMove, touchEnd,
      deleteEntry, saveSession
    }
  }
}
</script>

<style scoped>
.sort-tabs {
  display: flex;
  flex-direction: row;
  gap: 12rpx;
  margin-bottom: 30rpx;
  overflow-x: auto;
}

.sort-tab {
  padding: 14rpx 28rpx;
  border-radius: 30rpx;
  background: rgba(255, 255, 255, 0.06);
  border: 1rpx solid rgba(255, 255, 255, 0.1);
  white-space: nowrap;
}

.sort-tab.active {
  background: rgba(230, 57, 70, 0.2);
  border-color: #e63946;
}

.sort-tab-text {
  font-size: 26rpx;
  color: #aaa;
}

.sort-tab.active .sort-tab-text {
  color: #e63946;
  font-weight: 600;
}

.podium {
  display: flex;
  flex-direction: row;
  align-items: flex-end;
  justify-content: center;
  margin-bottom: 40rpx;
  padding: 20rpx;
  gap: 16rpx;
}

.podium-item {
  display: flex;
  flex-direction: column;
  align-items: center;
  flex: 1;
}

.podium-medal {
  font-size: 48rpx;
  margin-bottom: 8rpx;
}

.podium-name {
  font-size: 24rpx;
  color: #ccc;
  margin-bottom: 4rpx;
  text-align: center;
  overflow: hidden;
  text-overflow: ellipsis;
  white-space: nowrap;
  max-width: 180rpx;
}

.podium-score {
  font-size: 32rpx;
  font-weight: 700;
  color: #fff;
  margin-bottom: 12rpx;
}

.podium-bar {
  width: 100%;
  border-radius: 12rpx 12rpx 0 0;
}

.bar-1 {
  height: 160rpx;
  background: linear-gradient(180deg, #ffd700 0%, #b8860b 100%);
}

.bar-2 {
  height: 120rpx;
  background: linear-gradient(180deg, #c0c0c0 0%, #808080 100%);
}

.bar-3 {
  height: 90rpx;
  background: linear-gradient(180deg, #cd7f32 0%, #8b4513 100%);
}

.board-list {
  margin-bottom: 120rpx;
}

.board-item {
  position: relative;
  overflow: hidden;
  padding: 0;
}

.board-item-content {
  display: flex;
  flex-direction: row;
  align-items: center;
  gap: 20rpx;
  padding: 24rpx 30rpx;
  transition: transform 0.15s;
}

.rank-badge {
  width: 56rpx;
  height: 56rpx;
  border-radius: 50%;
  display: flex;
  align-items: center;
  justify-content: center;
  background: rgba(255, 255, 255, 0.08);
  flex-shrink: 0;
}

.rank-badge.rank-gold {
  background: linear-gradient(135deg, #ffd700, #b8860b);
}

.rank-badge.rank-silver {
  background: linear-gradient(135deg, #c0c0c0, #808080);
}

.rank-badge.rank-bronze {
  background: linear-gradient(135deg, #cd7f32, #8b4513);
}

.rank-num {
  font-size: 26rpx;
  font-weight: 700;
  color: #fff;
}

.board-info {
  flex: 1;
  display: flex;
  flex-direction: column;
  gap: 6rpx;
  overflow: hidden;
}

.board-name {
  font-size: 30rpx;
  font-weight: 600;
  color: #fff;
  overflow: hidden;
  text-overflow: ellipsis;
  white-space: nowrap;
}

.board-meta {
  font-size: 22rpx;
}

.board-stats {
  display: flex;
  flex-direction: column;
  align-items: flex-end;
  gap: 4rpx;
}

.board-kills {
  font-size: 34rpx;
  font-weight: 700;
  color: #e63946;
}

.board-streak {
  font-size: 22rpx;
  color: #f39c12;
}

.delete-btn {
  position: absolute;
  right: 0;
  top: 0;
  bottom: 0;
  width: 160rpx;
  display: flex;
  align-items: center;
  justify-content: center;
  background: #e63946;
  border-radius: 0 24rpx 24rpx 0;
}

.delete-text {
  color: #fff;
  font-size: 28rpx;
  font-weight: 600;
}

.empty-state {
  display: flex;
  flex-direction: column;
  align-items: center;
  padding: 100rpx 0;
  gap: 16rpx;
}

.empty-icon {
  font-size: 80rpx;
}

.empty-title {
  font-size: 32rpx;
  color: #666;
}

.empty-sub {
  font-size: 24rpx;
}

.bottom-action {
  position: fixed;
  bottom: 140rpx;
  left: 30rpx;
  right: 30rpx;
}
</style>
