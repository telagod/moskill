<script>
import { t } from '@/utils/i18n.js'

function updateTabBar() {
  const tabs = [
    { index: 0, text: t('tab.home') },
    { index: 1, text: t('tab.stats') },
    { index: 2, text: t('tab.rank') },
    { index: 3, text: t('tab.control') },
  ]
  tabs.forEach(item => {
    try { uni.setTabBarItem(item) } catch (_) {}
  })
}

export default {
  onLaunch() {
    console.log('MosKill App launched')
  },
  onShow() {
    updateTabBar()
  },
}
</script>

<style>
page {
  background-color: #0f0f23;
  color: #e0e0e0;
  font-family: -apple-system, BlinkMacSystemFont, 'Segoe UI', Roboto, sans-serif;
}
.container {
  padding: 30rpx;
}
.card {
  background: linear-gradient(135deg, #1a1a2e 0%, #16213e 100%);
  border-radius: 24rpx;
  padding: 30rpx;
  margin-bottom: 24rpx;
  box-shadow: 0 8rpx 32rpx rgba(0,0,0,0.3);
  border: 1rpx solid rgba(255,255,255,0.05);
}
.card-title {
  font-size: 28rpx;
  color: #888;
  margin-bottom: 16rpx;
  text-transform: uppercase;
  letter-spacing: 2rpx;
}
.btn-primary {
  background: linear-gradient(135deg, #e63946 0%, #ff6b6b 100%);
  color: #fff;
  border: none;
  border-radius: 16rpx;
  padding: 24rpx 0;
  font-size: 32rpx;
  font-weight: 600;
  text-align: center;
}
.btn-secondary {
  background: rgba(255,255,255,0.08);
  color: #e0e0e0;
  border: 1rpx solid rgba(255,255,255,0.15);
  border-radius: 16rpx;
  padding: 24rpx 0;
  font-size: 30rpx;
  text-align: center;
}
.text-red { color: #e63946; }
.text-green { color: #2ecc71; }
.text-blue { color: #3498db; }
.text-yellow { color: #f39c12; }
.text-big { font-size: 72rpx; font-weight: 700; }
.text-medium { font-size: 48rpx; font-weight: 600; }
.text-small { font-size: 24rpx; color: #666; }
.flex-row { display: flex; flex-direction: row; align-items: center; }
.flex-between { display: flex; justify-content: space-between; align-items: center; }
.flex-center { display: flex; justify-content: center; align-items: center; }
.gap-20 { gap: 20rpx; }
.mt-20 { margin-top: 20rpx; }
.mb-20 { margin-bottom: 20rpx; }
</style>
