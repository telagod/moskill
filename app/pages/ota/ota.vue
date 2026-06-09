<template>
  <view class="container">
    <!-- File Picker -->
    <view class="card" v-if="!file">
      <text class="card-title">Select Firmware</text>
      <view class="file-picker" @tap="pickFile">
        <text class="picker-icon">📦</text>
        <text class="picker-text">Tap to select .bin firmware file</text>
        <text class="picker-hint text-small">Supports .bin firmware images</text>
      </view>
    </view>

    <!-- File Info -->
    <view class="card" v-if="file">
      <text class="card-title">Firmware File</text>
      <view class="file-info">
        <view class="file-detail flex-between">
          <text class="file-label">Name</text>
          <text class="file-value">{{ file.name }}</text>
        </view>
        <view class="file-detail flex-between">
          <text class="file-label">Size</text>
          <text class="file-value">{{ formatSize(file.size) }}</text>
        </view>
        <view class="file-detail flex-between" v-if="fileCRC !== null">
          <text class="file-label">CRC32</text>
          <text class="file-value file-crc">0x{{ fileCRC.toString(16).toUpperCase().padStart(8, '0') }}</text>
        </view>
      </view>
      <button class="btn-secondary mt-20" @tap="pickFile" v-if="status === 'idle'">
        Change File
      </button>
    </view>

    <!-- Progress -->
    <view class="card" v-if="status !== 'idle' || progress > 0">
      <text class="card-title">Update Progress</text>
      <view class="progress-section">
        <view class="progress-bar-bg">
          <view
            class="progress-bar-fill"
            :class="progressClass"
            :style="{ width: progress + '%' }"
          ></view>
        </view>
        <text class="progress-text">{{ progress.toFixed(1) }}%</text>
      </view>

      <!-- Status Messages -->
      <view class="status-feed">
        <view class="status-item" v-for="(msg, idx) in statusLog" :key="idx">
          <text class="status-dot" :class="msg.type">●</text>
          <text class="status-msg">{{ msg.text }}</text>
        </view>
      </view>
    </view>

    <!-- Success State -->
    <view class="card success-card" v-if="status === 'complete'">
      <text class="success-icon">✅</text>
      <text class="success-title">Update Complete!</text>
      <text class="success-sub">Device will reboot automatically.</text>
      <text class="success-hint text-small mt-20">Please wait for the device to restart, then reconnect.</text>
    </view>

    <!-- Error State -->
    <view class="card error-card" v-if="status === 'error'">
      <text class="error-icon">❌</text>
      <text class="error-title">Update Failed</text>
      <text class="error-msg">{{ errorMsg }}</text>
      <button class="btn-primary mt-20" @tap="resetOTA">Try Again</button>
    </view>

    <!-- Action Buttons -->
    <view class="ota-actions">
      <button
        class="btn-primary"
        v-if="file && status === 'idle'"
        @tap="startUpdate"
      >
        ⚡ Start Firmware Update
      </button>

      <button
        class="btn-danger"
        v-if="status === 'uploading'"
        @tap="cancelUpdate"
      >
        ✖ Cancel Update
      </button>

      <button
        class="btn-secondary"
        v-if="status === 'complete'"
        @tap="goBack"
      >
        ← Back to Control
      </button>
    </view>
  </view>
</template>

<script>
import { ref, reactive, computed } from 'vue'
import { useStore } from 'vuex'
import { otaStart, otaWriteChunk, otaVerify, otaAbort, crc32, isConnected } from '@/utils/ble.js'

export default {
  setup() {
    const store = useStore()
    const CHUNK_SIZE = 236

    const file = ref(null)
    const fileData = ref(null)
    const fileCRC = ref(null)
    const status = ref('idle') // idle | uploading | verifying | complete | error
    const progress = ref(0)
    const errorMsg = ref('')
    const statusLog = reactive([])
    let cancelled = false

    const progressClass = computed(() => {
      if (status.value === 'error') return 'progress-error'
      if (status.value === 'complete') return 'progress-success'
      return 'progress-active'
    })

    function addLog(text, type = 'info') {
      statusLog.push({ text, type, time: Date.now() })
    }

    function formatSize(bytes) {
      if (!bytes) return '0 B'
      if (bytes < 1024) return bytes + ' B'
      if (bytes < 1024 * 1024) return (bytes / 1024).toFixed(1) + ' KB'
      return (bytes / (1024 * 1024)).toFixed(2) + ' MB'
    }

    function pickFile() {
      uni.chooseMessageFile({
        count: 1,
        type: 'file',
        extension: ['.bin'],
        success(res) {
          const f = res.tempFiles[0]
          file.value = {
            name: f.name,
            size: f.size,
            path: f.path,
          }

          // Read file data
          const fs = uni.getFileSystemManager()
          fs.readFile({
            filePath: f.path,
            success(readRes) {
              fileData.value = readRes.data
              fileCRC.value = crc32(readRes.data)
              addLog(`File loaded: ${f.name} (${formatSize(f.size)})`, 'info')
            },
            fail(e) {
              errorMsg.value = 'Failed to read file'
              status.value = 'error'
            }
          })
        },
        fail() {
          // User cancelled
        }
      })
    }

    async function startUpdate() {
      if (!isConnected()) {
        uni.showToast({ title: 'Device not connected', icon: 'none' })
        return
      }
      if (!fileData.value) {
        uni.showToast({ title: 'No file selected', icon: 'none' })
        return
      }

      cancelled = false
      status.value = 'uploading'
      progress.value = 0
      statusLog.length = 0
      addLog('Starting OTA update...', 'info')

      try {
        // Send OTA start command
        addLog(`Initiating transfer (${formatSize(file.value.size)})`, 'info')
        await otaStart(file.value.size)
        addLog('Device ready for firmware', 'success')

        // Send chunks
        const data = new Uint8Array(fileData.value)
        const totalChunks = Math.ceil(data.length / CHUNK_SIZE)
        let offset = 0

        for (let i = 0; i < totalChunks; i++) {
          if (cancelled) {
            addLog('Update cancelled by user', 'error')
            status.value = 'idle'
            progress.value = 0
            return
          }

          const end = Math.min(offset + CHUNK_SIZE, data.length)
          const chunk = data.slice(offset, end).buffer

          await otaWriteChunk(offset, chunk)

          offset = end
          progress.value = (offset / data.length) * 100

          // Log progress milestones
          if (i % Math.max(1, Math.floor(totalChunks / 10)) === 0) {
            addLog(`Uploading... ${progress.value.toFixed(0)}%`, 'info')
          }
        }

        addLog('Upload complete, verifying CRC32...', 'info')
        status.value = 'verifying'

        // Verify CRC
        await otaVerify(fileCRC.value)
        addLog(`CRC32 verified: 0x${fileCRC.value.toString(16).toUpperCase().padStart(8, '0')}`, 'success')

        progress.value = 100
        status.value = 'complete'
        addLog('Firmware update complete! Device will reboot.', 'success')

      } catch (e) {
        errorMsg.value = e.errMsg || e.message || 'OTA update failed'
        status.value = 'error'
        addLog(`Error: ${errorMsg.value}`, 'error')

        try {
          await otaAbort()
        } catch (_) {
          // Ignore abort errors
        }
      }
    }

    async function cancelUpdate() {
      cancelled = true
      try {
        await otaAbort()
        addLog('OTA aborted', 'error')
      } catch (e) {
        // ignore
      }
      status.value = 'idle'
      progress.value = 0
    }

    function resetOTA() {
      status.value = 'idle'
      progress.value = 0
      errorMsg.value = ''
      statusLog.length = 0
    }

    function goBack() {
      uni.navigateBack()
    }

    return {
      file, fileCRC, status, progress, errorMsg,
      statusLog, progressClass,
      formatSize, pickFile, startUpdate, cancelUpdate,
      resetOTA, goBack
    }
  }
}
</script>

<style scoped>
.file-picker {
  display: flex;
  flex-direction: column;
  align-items: center;
  justify-content: center;
  padding: 60rpx 30rpx;
  border: 3rpx dashed rgba(230, 57, 70, 0.4);
  border-radius: 20rpx;
  background: rgba(230, 57, 70, 0.05);
  gap: 16rpx;
}

.file-picker:active {
  background: rgba(230, 57, 70, 0.1);
}

.picker-icon {
  font-size: 64rpx;
}

.picker-text {
  font-size: 30rpx;
  color: #ccc;
}

.picker-hint {
  font-size: 24rpx;
}

.file-info {
  display: flex;
  flex-direction: column;
  gap: 16rpx;
}

.file-detail {
  padding: 12rpx 0;
  border-bottom: 1rpx solid rgba(255, 255, 255, 0.05);
}

.file-detail:last-child {
  border-bottom: none;
}

.file-label {
  font-size: 28rpx;
  color: #888;
}

.file-value {
  font-size: 28rpx;
  color: #fff;
  font-weight: 500;
}

.file-crc {
  font-family: monospace;
  color: #3498db;
}

.progress-section {
  margin-bottom: 24rpx;
}

.progress-bar-bg {
  height: 16rpx;
  background: #222;
  border-radius: 8rpx;
  overflow: hidden;
  margin-bottom: 12rpx;
}

.progress-bar-fill {
  height: 100%;
  border-radius: 8rpx;
  transition: width 0.3s;
}

.progress-active {
  background: linear-gradient(90deg, #e63946 0%, #ff6b6b 100%);
}

.progress-success {
  background: linear-gradient(90deg, #2ecc71 0%, #27ae60 100%);
}

.progress-error {
  background: linear-gradient(90deg, #e63946 0%, #c0392b 100%);
}

.progress-text {
  font-size: 28rpx;
  color: #fff;
  font-weight: 600;
  text-align: center;
}

.status-feed {
  max-height: 400rpx;
  overflow-y: auto;
}

.status-item {
  display: flex;
  flex-direction: row;
  align-items: flex-start;
  gap: 12rpx;
  padding: 10rpx 0;
}

.status-dot {
  font-size: 16rpx;
  margin-top: 4rpx;
}

.status-dot.info { color: #3498db; }
.status-dot.success { color: #2ecc71; }
.status-dot.error { color: #e63946; }

.status-msg {
  font-size: 26rpx;
  color: #bbb;
  line-height: 1.4;
}

.success-card {
  text-align: center;
  border: 1rpx solid rgba(46, 204, 113, 0.3);
  padding: 50rpx 30rpx;
}

.success-icon {
  font-size: 80rpx;
  display: block;
  margin-bottom: 20rpx;
}

.success-title {
  font-size: 36rpx;
  font-weight: 700;
  color: #2ecc71;
  display: block;
  margin-bottom: 12rpx;
}

.success-sub {
  font-size: 28rpx;
  color: #aaa;
  display: block;
}

.success-hint {
  font-size: 24rpx;
  color: #666;
  display: block;
}

.error-card {
  text-align: center;
  border: 1rpx solid rgba(230, 57, 70, 0.3);
  padding: 40rpx 30rpx;
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
}

.ota-actions {
  margin-top: 30rpx;
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
