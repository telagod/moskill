<template>
  <view :style="styles" :class="['mk-icon', inline ? 'mk-icon-inline' : '']"></view>
</template>

<script>
import { computed } from 'vue'
import { iconUri } from '@/utils/icons.js'

export default {
  name: 'MkIcon',
  props: {
    name: { type: String, required: true },
    size: { type: [Number, String], default: 48 },
    color: { type: String, default: '#e0e0e0' },
    inline: { type: Boolean, default: false },
  },
  setup(props) {
    const styles = computed(() => {
      const sz = typeof props.size === 'number' ? props.size + 'rpx' : props.size
      const uri = iconUri(props.name, props.color)
      return {
        width: sz,
        height: sz,
        backgroundImage: uri ? `url("${uri}")` : 'none',
        backgroundSize: 'contain',
        backgroundRepeat: 'no-repeat',
        backgroundPosition: 'center',
        flexShrink: '0',
      }
    })
    return { styles }
  },
}
</script>

<style>
.mk-icon {
  display: inline-flex;
}
.mk-icon-inline {
  vertical-align: middle;
}
</style>
