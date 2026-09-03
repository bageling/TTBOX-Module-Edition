/**
 * TTBOX Update Page — 真实 OTA/OTG 更新页面
 * 依赖：window.ttbox.api (apiClient.js)
 */
(function () {
  'use strict';

  const $ = (id) => document.getElementById(id);
  const BTN = {
    check: $('updateCheckButton'),
    install: $('updateInstallButton'),
    rollback: $('updateRollbackButton'),
    scanOtg: $('updateScanOtgButton'),
  };
  const EL = {
    pill: $('updateStatusPill'),
    currentVersion: $('updateCurrentVersion'),
    currentVersionPill: $('updateCurrentVersionPill'),
    build: $('updateBuild'),
    channel: $('updateChannel'),
    latestPill: $('updateLatestPill'),
    progressLabel: $('updateProgressLabel'),
    progressPercent: $('updateProgressPercent'),
    progressBar: $('updateProgressBar'),
    progressMessage: $('updateProgressMessage'),
    releaseNotes: $('updateReleaseNotes'),
    engineState: $('updateEngineState'),
    engineError: $('updateEngineError'),
  };

  let currentState = 'IDLE';
  let pollTimer = null;

  // ── 进度映射 ──
  const STATE_LABELS = {
    'IDLE': '待机', 'CHECKING': '检查中', 'DOWNLOADING': '下载中',
    'VERIFYING': '验证中', 'STAGING': '准备中', 'READY': '准备就绪',
    'APPLYING': '安装中', 'HEALTH_CHECK': '健康检查', 'COMMITTED': '更新完成',
    'FAILED': '失败', 'ROLLING_BACK': '回滚中', 'ROLLED_BACK': '已回滚',
  };

  const STATE_PROGRESS = {
    'IDLE': 0, 'CHECKING': 10, 'DOWNLOADING': 30, 'VERIFYING': 50,
    'STAGING': 60, 'READY': 70, 'APPLYING': 80, 'HEALTH_CHECK': 90,
    'COMMITTED': 100, 'FAILED': 0, 'ROLLING_BACK': 50, 'ROLLED_BACK': 0,
  };

  function setProgress(state, msg) {
    const pct = STATE_PROGRESS[state] || 0;
    EL.progressLabel.textContent = STATE_LABELS[state] || state;
    EL.progressPercent.textContent = pct + '%';
    EL.progressBar.style.width = pct + '%';
    if (msg) EL.progressMessage.textContent = msg;
    EL.engineState.textContent = STATE_LABELS[state] || state;
  }

  // ── 加载版本信息 ──
  async function loadVersion() {
    try {
      const res = await ttbox.api.system.version();
      if (res.ok && res.data) {
        EL.currentVersion.textContent = res.data.version || '--';
        EL.currentVersionPill.textContent = 'v' + (res.data.version || '--');
        EL.build.textContent = res.data.build || '--';
        EL.channel.textContent = res.data.channel || '--';
        EL.pill.textContent = 'v' + (res.data.version || '--');
      }
    } catch (e) {
      console.warn('[Update] Failed to load version:', e);
    }
  }

  // ── 检查更新 ──
  async function checkUpdate() {
    setProgress('CHECKING', '正在检查更新...');
    BTN.check.disabled = true;
    try {
      const res = await ttbox.api.update.check();
      if (res.ok && res.data) {
        if (res.data.update_available) {
          EL.latestPill.textContent = 'v' + res.data.latest_version + ' 可用';
          EL.latestPill.className = 'pill update-available';
          EL.releaseNotes.textContent = '版本 ' + res.data.latest_version + ' 可用\n' + (res.data.release_date || '');
          BTN.install.disabled = false;
          setProgress('IDLE', '新版本可用: v' + res.data.latest_version);
        } else {
          EL.latestPill.textContent = '已是最新';
          EL.latestPill.className = 'pill';
          EL.releaseNotes.textContent = '当前已是最新版本。';
          BTN.install.disabled = true;
          setProgress('IDLE', '已是最新版本');
        }
      }
    } catch (e) {
      EL.latestPill.textContent = '检查失败';
      EL.releaseNotes.textContent = '检查更新失败: ' + (e.message || '网络错误');
      setProgress('FAILED', '检查失败: ' + (e.message || '网络错误'));
    } finally {
      BTN.check.disabled = false;
    }
  }

  // ── 安装更新 ──
  async function installUpdate() {
    BTN.install.disabled = true;
    BTN.check.disabled = true;

    // 开始轮询状态
    pollTimer = setInterval(pollUpdateStatus, 2000);

    try {
      const res = await ttbox.api.update.start();
      if (res.ok) {
        setProgress('COMMITTED', '更新完成');
        EL.releaseNotes.textContent = '更新已成功安装。';
        loadVersion();
        BTN.rollback.disabled = false;
      } else {
        EL.releaseNotes.textContent = '更新失败: ' + (res.error || '未知错误');
        setProgress('FAILED', '更新失败');
        BTN.rollback.disabled = false;
      }
    } catch (e) {
      EL.releaseNotes.textContent = '更新失败: ' + (e.message || '网络错误');
      setProgress('FAILED', '更新失败');
    } finally {
      clearInterval(pollTimer);
      pollTimer = null;
      BTN.install.disabled = false;
      BTN.check.disabled = false;
    }

    // 最终状态刷新
    setTimeout(loadVersion, 1000);
  }

  // ── 轮询更新状态 ──
  async function pollUpdateStatus() {
    try {
      const res = await ttbox.api.update.status();
      if (res.ok && res.data) {
        const state = res.data.state || 'IDLE';
        currentState = state;
        setProgress(state, res.data.last_error || '');
        EL.engineState.textContent = STATE_LABELS[state] || state;
        EL.engineError.textContent = res.data.last_error || '--';

        if (state === 'COMMITTED' || state === 'FAILED' || state === 'ROLLED_BACK') {
          clearInterval(pollTimer);
          pollTimer = null;
          loadVersion();
          if (state === 'COMMITTED') {
            BTN.rollback.disabled = false;
          }
        }
      }
    } catch (e) {
      // 静默处理
    }
  }

  // ── 回滚 ──
  async function rollback() {
    BTN.rollback.disabled = true;
    setProgress('ROLLING_BACK', '正在回滚...');
    try {
      const res = await ttbox.api.update.rollback();
      if (res.ok) {
        setProgress('ROLLED_BACK', '已回滚到上一版本');
        EL.releaseNotes.textContent = '已回滚到上一版本。';
        loadVersion();
      } else {
        EL.releaseNotes.textContent = '回滚失败: ' + (res.error || '未知错误');
      }
    } catch (e) {
      EL.releaseNotes.textContent = '回滚失败: ' + (e.message || '网络错误');
      setProgress('FAILED', '回滚失败');
    } finally {
      BTN.rollback.disabled = false;
    }
  }

  // ── 扫描 OTG ──
  async function scanOtg() {
    setProgress('CHECKING', '扫描 USB 更新...');
    BTN.scanOtg.disabled = true;
    try {
      const res = await ttbox.api.update.usbScan();
      if (res.ok && res.data) {
        if (res.data.update_available) {
          EL.latestPill.textContent = 'USB: v' + res.data.latest_version;
          EL.releaseNotes.textContent = 'USB 更新可用: v' + res.data.latest_version;
          BTN.install.disabled = false;
          setProgress('IDLE', 'USB 更新可用');
        } else {
          EL.releaseNotes.textContent = '未发现 USB 更新。请确认 USB 设备已插入且包含 TTBOX/ 目录。';
          setProgress('IDLE', '未发现 USB 更新');
        }
      }
    } catch (e) {
      EL.releaseNotes.textContent = '扫描失败: ' + (e.message || '网络错误');
      setProgress('FAILED', '扫描失败');
    } finally {
      BTN.scanOtg.disabled = false;
    }
  }

  // ── 初始化 ──
  function init() {
    loadVersion();

    BTN.check.addEventListener('click', checkUpdate);
    BTN.install.addEventListener('click', installUpdate);
    BTN.rollback.addEventListener('click', rollback);
    BTN.scanOtg.addEventListener('click', scanOtg);

    // 初始状态
    setProgress('IDLE', '就绪');
    console.log('[TTBOX Update Page] initialized');
  }

  if (document.readyState === 'loading') {
    document.addEventListener('DOMContentLoaded', init);
  } else {
    init();
  }
})();
