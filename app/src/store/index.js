import { createStore } from 'vuex'
import { mockSession, mockLifetime, mockConfig, mockBattery } from '../utils/mock'

export default createStore({
  state: {
    connected: false,
    devMode: uni.getStorageSync('moskill_dev_mode') || false,
    deviceId: null,
    deviceName: '',
    killCount: 0,
    session: null,
    lifetime: null,
    config: { sensitivity: 1, ledBrightness: 128, buzzerVolume: 2, buzzerOnKill: true, ledOnKill: true, streakEffects: true },
    env: { temp: '--', humi: '--' },
    battery: 0,
    leaderboard: [],
    killFeed: [],
  },
  mutations: {
    SET_CONNECTED(state, { connected, deviceId, deviceName }) {
      state.connected = connected
      state.deviceId = deviceId
      state.deviceName = deviceName || ''
    },
    SET_KILL_COUNT(state, count) {
      state.killCount = count
      state.killFeed.unshift({ count, time: Date.now() })
      if (state.killFeed.length > 50) state.killFeed.pop()
    },
    SET_SESSION(state, s) { state.session = s },
    SET_LIFETIME(state, l) { state.lifetime = l },
    SET_CONFIG(state, c) { state.config = c },
    SET_ENV(state, e) { state.env = e },
    SET_BATTERY(state, b) { state.battery = b },
    SET_DEV_MODE(state, v) {
      state.devMode = v
      state.connected = v
      if (v) {
        state.deviceName = 'MosKill-DEV'
        state.session = mockSession()
        state.lifetime = mockLifetime()
        state.config = mockConfig()
        state.battery = mockBattery()
        state.env = { temp: 27.5, humi: 68.3 }
        state.killCount = state.session.killsTotal
      }
    },
    SET_LEADERBOARD(state, lb) { state.leaderboard = lb },
    ADD_LEADERBOARD_ENTRY(state, entry) {
      state.leaderboard.push(entry)
      state.leaderboard.sort((a, b) => b.totalKills - a.totalKills)
      state.leaderboard = state.leaderboard.slice(0, 100)
      uni.setStorageSync('moskill_leaderboard', state.leaderboard)
    },
  },
  actions: {
    loadLeaderboard({ commit }) {
      const lb = uni.getStorageSync('moskill_leaderboard') || []
      commit('SET_LEADERBOARD', lb)
    },
    saveScore({ commit, state }) {
      if (!state.session) return
      const entry = {
        id: Date.now().toString(36),
        name: state.deviceName || 'MosKill',
        totalKills: state.session.killsTotal,
        streak: state.session.maxStreak,
        duration: state.session.duration,
        efficiency: state.session.efficiency,
        date: new Date().toISOString().slice(0, 10),
        killsM: state.session.killsM,
      }
      commit('ADD_LEADERBOARD_ENTRY', entry)
    },
  },
})
