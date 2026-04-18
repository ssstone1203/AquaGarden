import { defineConfig } from 'vite'
import vue from '@vitejs/plugin-vue'
import { fileURLToPath, URL } from 'node:url'

export default defineConfig({//导出配置
  plugins: [vue()],//启用vue3插件，让vite能编译和处理vue单文件组件.vue文件
  resolve: {
    alias: {
      '@': fileURLToPath(new URL('./src', import.meta.url)),
    },
  },
  server: {
    port: 5173,
    proxy: {
      '/api': { target: 'http://127.0.0.1:8080', changeOrigin: true, ws: true },
      '/docs': { target: 'http://127.0.0.1:8080', changeOrigin: true },
      '/openapi.json': { target: 'http://127.0.0.1:8080', changeOrigin: true },
    },
  },
})
