/**
 * TTBOX API Client — 统一 API 请求层
 *
 * 所有页面禁止直接 fetch/axios/XMLHttpRequest。
 * 统一经过此模块。
 *
 * API 路径：/api/v1/{domain}/{action}
 * 响应格式：{ ok: bool, data: object, error?: string }
 */

(function () {
  'use strict';

  const API_BASE = '/api';

  /**
   * 核心请求函数
   */
  async function request(method, path, body, opts = {}) {
    const url = path.startsWith('http')
      ? path
      : (path.startsWith('/api/') ? path : `${API_BASE}${path}`);
    const init = {
      method,
      headers: { 'Content-Type': 'application/json', ...opts.headers },
      signal: opts.signal || null,
    };
    if (body && method !== 'GET') {
      init.body = JSON.stringify(body);
    }

    try {
      const resp = await fetch(url, init);
      const text = await resp.text();
      let json;
      try { json = JSON.parse(text); } catch (e) { json = { ok: false, error: '响应格式错误' }; }

      if (!resp.ok) {
        return { ok: false, status: resp.status, error: json.error || `请求失败 (${resp.status})`, data: null };
      }

      return { ok: true, status: resp.status, data: json.data || json, error: null };
    } catch (err) {
      if (err.name === 'AbortError') {
        return { ok: false, status: 0, error: '请求超时', data: null };
      }
      return { ok: false, status: 0, error: '网络错误: ' + err.message, data: null };
    }
  }

  /**
   * 对外接口
   */
  const apiClient = {
    request: (method, path, body, opts) => request(method, path, body, opts),
    get: (path, opts) => request('GET', path, null, opts),
    post: (path, body, opts) => request('POST', path, body, opts),
    put: (path, body, opts) => request('PUT', path, body, opts),
    del: (path, opts) => request('DELETE', path, null, opts),
    rawDownload: async (path, opts = {}) => {
      const controller = new AbortController();
      const timer = setTimeout(() => controller.abort(), opts.timeout || 30000);
      try { return await fetch(path, { ...opts, signal: controller.signal }); } finally { clearTimeout(timer); }
    },

    // ---- 系统 ----
    system: {
      version: () => request('GET', '/system/version'),
      status: () => request('GET', '/system'),
      storage: () => request('GET', '/system/storage'),
      reboot: () => request('POST', '/system/reboot'),
      poweroff: () => request('POST', '/system/poweroff'),
      hostname: (name) => request('PUT', '/system/hostname', { hostname: name }),
    },

    // ---- 运行时 ----
    runtime: {
      state: () => request('GET', '/state'),
      start: () => request('POST', '/runtime/start'),
      stop: () => request('POST', '/runtime/stop'),
      config: () => request('GET', '/config'),
      setConfig: (profile) => request('PUT', '/config', { profile }),
    },

    // ---- 模型 ----
    models: {
      list: () => request('GET', '/models'),
      import: () => { /* TODO: 文件上传 */ return Promise.resolve({ ok: false, error: '即将支持' }); },
      delete: (id) => request('POST', '/models/delete', { model_id: id }),
      select: (id) => request('POST', '/models/select', { model_id: id }),
      activate: (id) => request('POST', '/models/activate', { model_id: id }),
      classNames: (names) => request('POST', '/models/class-names', { class_names: names }),
      concurrency: (n) => request('POST', '/models/rknn-concurrency', { count: n }),
    },

    // ---- 预设 ----
    presets: {
      list: () => request('GET', '/presets'),
      save: (name, profile) => request('POST', '/presets', { name, profile }),
      load: (name) => request('POST', '/presets/load', { name }),
      delete: (name) => request('POST', '/presets/delete', { name }),
    },

    // ---- 预览 ----
    preview: {
      image: () => '/api/v1/preview.jpg',
      stream: () => '/api/v1/preview.mjpg',
    },

    // ---- 硬件 ----
    hardware: {
      display: () => request('GET', '/hardware/display'),
      setDisplay: (cfg) => request('PUT', '/hardware/display', cfg),
      mouse: () => request('GET', '/hardware/mouse'),
      setMouse: (cfg) => request('PUT', '/hardware/mouse', cfg),
    },

    // ---- 网络 ----
    network: {
      wifi: () => request('GET', '/network/wifi'),
      scan: () => request('POST', '/network/wifi/scan'),
      connect: (ssid, psk) => request('POST', '/network/wifi/connect', { ssid, psk }),
      ap: () => request('POST', '/network/wifi/ap/apply'),
      client: () => request('POST', '/network/wifi/client/activate'),
    },

    // ---- 更新 ----
    update: {
      status: () => request('GET', '/update/status'),
      check: () => request('POST', '/update/check'),
      usbScan: () => request('POST', '/update/usb/scan'),
      start: (version) => request('POST', '/update/start', { version }),
      rollback: () => request('POST', '/update/rollback'),
      cancel: () => request('POST', '/update/cancel'),
      log: () => request('GET', '/update/log'),
    },

    // ---- 授权 ----
    license: {
      info: () => request('GET', '/license'),
    },

    // ---- 诊断 ----
    diagnostics: {
      aimTrace: () => request('POST', '/diagnostics/aim-trace'),
    },

    // ---- 设备枚举 ----
    devices: {
      makcu: () => request('GET', '/makcu/devices'),
      ferrum: () => request('GET', '/ferrum/devices'),
      kmbox: () => request('GET', '/kmboxb/devices'),
    },
  };

  // 暴露到全局
  window.ttbox = window.ttbox || {};
  window.ttbox.api = apiClient;

  console.log('[TTBOX API Client] 已加载 (v1.0.0)');
})();
