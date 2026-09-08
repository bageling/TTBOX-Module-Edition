const statusEl = document.getElementById("activationStatus");
const keyInput = document.getElementById("activationKeyInput");
const activateButton = document.getElementById("activateButton");
const refreshButton = document.getElementById("refreshButton");
const recoveryButton = document.getElementById("recoveryButton");
const toastEl = document.getElementById("activationToast");
const activationParams = new URLSearchParams(window.location.search);
const manualActivation = activationParams.get("activation") === "1" && activationParams.get("automatic") !== "1";
let recoveryAvailable = false;
let recoveryLocked = false;

function setStatus(message, isError = false) {
  if (!statusEl) return;
  statusEl.textContent = message || "";
  statusEl.classList.toggle("error", !!isError);
}

function showToast(message, isError = false) {
  if (!toastEl) return;
  toastEl.textContent = message || "";
  toastEl.className = `activation-toast${isError ? " error" : ""}`;
  toastEl.hidden = false;
  window.clearTimeout(showToast.timer);
  showToast.timer = window.setTimeout(() => {
    toastEl.hidden = true;
  }, 2600);
}

function setBusy(busy) {
  [keyInput, activateButton, refreshButton].forEach((el) => {
    if (el) el.disabled = !!busy;
  });
  if (recoveryButton) {
    recoveryButton.disabled = !!busy || recoveryLocked || !recoveryAvailable;
  }
}

async function api(path, options = {}) {
  const response = await fetch(path, {
    credentials: "same-origin",
    ...options,
    headers: {
      Accept: "application/json",
      ...(options.headers || {}),
    },
  });
  const payload = await response.json().catch(() => ({}));
  if (!response.ok || !payload.ok) {
    const error = new Error(payload.error || `HTTP ${response.status}`);
    error.status = response.status;
    error.data = payload.data;
    throw error;
  }
  return payload.data;
}

function licenseValid(payload) {
  const license = payload && payload.license;
  return !!(license && license.valid);
}

function licensePendingMessage(payload) {
  const license = payload && payload.license;
  if (license && license.status === "device_identity_pending") {
    return license.message || "正在准备设备身份，请稍后刷新或重启设备。";
  }
  return "";
}

function activationNetworkMessage(payload) {
  const status = payload && payload.status;
  if (payload && payload.changed) {
    return "已切回 Wi-Fi 模式。若页面断开，请连接到同一局域网后重新打开。";
  }
  if (status && status.ethernet_connected) {
    return "已处于可联网模式，正在检查授权。";
  }
  const connected = status && status.connected;
  if (connected && connected.ssid) {
    return `已连接 Wi-Fi：${connected.ssid}，正在检查授权。`;
  }
  return "已禁止 AP 模式，请连接可联网 Wi-Fi 后完成激活。";
}

function formatBytes(bytes) {
  const value = Number(bytes || 0);
  if (!Number.isFinite(value) || value <= 0) return "";
  return `${(value / 1024 / 1024).toFixed(1)} MB`;
}

async function loadFullRecoveryStatus() {
  if (!recoveryButton || recoveryLocked) return;
  try {
    const payload = await api("/api/activation/full-recovery");
    recoveryAvailable = !!(payload && payload.available && payload.allowed);
    const version = String((payload && payload.version) || "");
    const size = formatBytes(payload && payload.size);
    if (recoveryAvailable) {
      recoveryButton.title = `使用本地 ${version || "full OTA"}${size ? ` (${size})` : ""} 覆盖修复并重新激活`;
    } else if (payload && payload.available && !payload.allowed) {
      recoveryButton.title = payload.reason || "当前设备状态不允许执行本地全量修复";
    } else {
      recoveryButton.title = "设备上暂无通过校验的本地 full OTA";
    }
  } catch (error) {
    recoveryAvailable = false;
    recoveryButton.title = error.message || "无法检查本地恢复包";
  }
  recoveryButton.disabled = !recoveryAvailable;
}

async function waitForFullRecovery() {
  const deadline = Date.now() + 180000;
  while (Date.now() < deadline) {
    await new Promise((resolve) => window.setTimeout(resolve, 2000));
    try {
      const payload = await api("/api/update/status");
      if (payload && payload.status === "failed" && payload.recovery) {
        recoveryLocked = false;
        setStatus(payload.error || payload.message || "本地全量恢复失败", true);
        showToast("本地全量恢复失败", true);
        await loadFullRecoveryStatus();
        setBusy(false);
        return;
      }
      if (payload && payload.status === "success" && payload.stage === "recovery_complete") {
        setStatus("本地全量恢复完成，正在刷新激活页面。");
        window.setTimeout(() => window.location.reload(), 800);
        return;
      }
      if (payload && payload.recovery && payload.message) {
        setStatus(payload.message);
      }
    } catch (_error) {
      // The web service is expected to disconnect briefly while files are replaced.
    }
  }
  recoveryLocked = false;
  setStatus("本地全量恢复仍未完成，请刷新页面检查状态。", true);
  await loadFullRecoveryStatus();
  setBusy(false);
}

async function startFullRecovery() {
  if (!recoveryAvailable || recoveryLocked) return;
  const version = recoveryButton ? recoveryButton.title : "本地 full OTA";
  if (!window.confirm(`${version}\n\n该操作会覆盖安装程序文件并清除当前激活状态。是否继续？`)) {
    return;
  }
  recoveryLocked = true;
  setBusy(true);
  setStatus("正在启动本地全量恢复，请勿断电。");
  try {
    const payload = await api("/api/activation/full-recovery", {
      method: "POST",
      headers: { "Content-Type": "application/json" },
      body: JSON.stringify({ confirm: "restore-local-full-ota" }),
    });
    setStatus((payload && payload.message) || "本地全量恢复已启动，请勿断电。");
    waitForFullRecovery();
  } catch (error) {
    recoveryLocked = false;
    setStatus(error.message || String(error), true);
    showToast(error.message || String(error), true);
    await loadFullRecoveryStatus();
    setBusy(false);
  }
}

function daemonTimeoutError(error) {
  const message = String((error && error.message) || error || "").toLowerCase();
  return message.includes("daemon socket unavailable") && message.includes("timed out");
}

function activationResetPayload(error) {
  const data = error && error.data;
  return data && data.activation_identity_reset;
}

async function recoverLocalActivationIdentity(error) {
  const existingReset = activationResetPayload(error);
  if (existingReset && existingReset.restart_scheduled) {
    const message = existingReset.message || "授权服务正在重启；请稍后刷新页面并重新输入激活码。";
    setStatus(message, false);
    showToast("服务正在重启，请稍后刷新页面。");
    return true;
  }
  if (!daemonTimeoutError(error)) {
    return false;
  }
  setStatus("授权服务无响应，正在重启授权服务。");
  try {
    const payload = await api("/api/activation/reset-local-identity", { method: "POST" });
    const message = (payload && payload.message) || "授权服务正在重启；请稍后刷新页面并重新输入激活码。";
    setStatus(message, false);
    showToast("服务正在重启，请稍后刷新页面。");
  } catch (resetError) {
    if (resetError instanceof TypeError) {
      const message = "重启请求已发送，服务可能正在重启；请稍后刷新页面并重新输入激活码。";
      setStatus(message, false);
      showToast("服务正在重启，请稍后刷新页面。");
      return true;
    }
    const message = (resetError && resetError.message) || String(resetError);
    setStatus(message, true);
    showToast(message, true);
  }
  return true;
}

async function prepareActivationNetwork() {
  setBusy(true);
  setStatus("正在切回 Wi-Fi 模式，激活需要联网。");
  try {
    const payload = await api("/api/activation/network/prepare", { method: "POST" });
    setStatus(activationNetworkMessage(payload));
    return true;
  } catch (error) {
    if (error instanceof TypeError) {
      const message = "正在切回 Wi-Fi 模式。如果页面断开，请连接到同一局域网后重新打开激活页。";
      setStatus(message);
      showToast(message);
      return false;
    }
    const message = error.message || String(error);
    setStatus(`Wi-Fi 模式准备失败：${message}`, true);
    showToast(message, true);
    return true;
  } finally {
    setBusy(false);
  }
}

async function refreshStatus({ quiet = false } = {}) {
  setBusy(true);
  try {
    const payload = await api("/api/license");
    if (licenseValid(payload)) {
      if (manualActivation) {
        setStatus("设备本地授权有效，可重新输入激活码同步服务器授权。");
        if (!quiet) showToast("本地授权有效");
        return;
      }
      setStatus("设备已激活，正在进入系统。");
      window.location.replace("/");
      return;
    }
    setStatus(licensePendingMessage(payload) || "设备未激活，请输入激活码完成授权。");
    if (!quiet) showToast("授权状态已刷新");
  } catch (error) {
    if (await recoverLocalActivationIdentity(error)) {
      return;
    }
    setStatus(error.message || String(error), true);
    if (!quiet) showToast(error.message || String(error), true);
  } finally {
    setBusy(false);
  }
}

async function activate() {
  const licenseKey = keyInput ? keyInput.value.trim() : "";
  if (!licenseKey) {
    setStatus("请输入激活码。", true);
    if (keyInput) keyInput.focus();
    return;
  }
  setBusy(true);
  setStatus("正在验证激活码，请稍候。");
  try {
    await api("/api/license/activate", {
      method: "POST",
      headers: { "Content-Type": "application/json" },
      body: JSON.stringify({ license_key: licenseKey }),
    });
    setStatus("激活成功，正在进入系统。");
    window.setTimeout(() => window.location.replace("/"), 600);
  } catch (error) {
    if (await recoverLocalActivationIdentity(error)) {
      return;
    }
    setStatus(error.message || String(error), true);
    showToast(error.message || String(error), true);
  } finally {
    setBusy(false);
  }
}

if (activateButton) activateButton.addEventListener("click", activate);
if (refreshButton) refreshButton.addEventListener("click", () => refreshStatus());
if (recoveryButton) recoveryButton.addEventListener("click", startFullRecovery);
if (keyInput) {
  keyInput.addEventListener("keydown", (event) => {
    if (event.key === "Enter") {
      event.preventDefault();
      activate();
    }
  });
  keyInput.focus();
}

async function initActivationPage() {
  await loadFullRecoveryStatus();
  const shouldRefresh = await prepareActivationNetwork();
  if (shouldRefresh) {
    await refreshStatus({ quiet: true });
  }
}

initActivationPage();
