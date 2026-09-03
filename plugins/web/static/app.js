const HOTKEYS = ["left", "right", "middle", "back", "forward", "auto"];
const POINTER_HOTKEYS = ["left", "right", "middle", "back", "forward"];
const BLOCKABLE_PHYSICAL_BUTTONS = [...POINTER_HOTKEYS];
const OPTIONAL_POINTER_HOTKEYS = ["", ...POINTER_HOTKEYS];
const AUTO_TRIGGER_HOTKEYS = ["auto", "right", "middle", "back", "forward"];
const AUTO_TRIGGER_FIRE_MODES = ["tap", "spray"];
const RAPID_FIRE_HOTKEYS = ["left", "right", "middle", "back", "forward"];
const HOTKEY_LABELS = {
  left: "左键",
  right: "右键",
  middle: "中键",
  back: "后侧键",
  forward: "前侧键",
  auto: "自动",
  "": "不使用",
};
const CROSSHAIR_MAIN_HOTKEY_LABELS = {
  ...HOTKEY_LABELS,
  "": "自动",
};
const STATUS_LABELS = {
  running: "运行中",
  stopped: "已停止",
  starting: "启动中",
  stopping: "停止中",
  reconnecting: "采集重连中",
  error: "异常",
  idle: "空闲",
  locked: "未激活",
};
const AIM_CLASS_MAX_COUNT = 30;
const AIM_CLASS_ALL_MASK = (1 << AIM_CLASS_MAX_COUNT) - 1;
const AIM_PROFILE_AXIS_OFFSET_MIN = 0;
const AIM_PROFILE_AXIS_OFFSET_MAX = 1;
const AIM_PROFILE_FOV_SCALE_MIN = 0.1;
const AIM_PROFILE_FOV_SCALE_MAX = 1;
const CROP_SIZE_OPTIONS = [192, 256, 320, 416, 640];
const CROP_SIZE_MIN = 1;
const CROP_SIZE_MAX = 1080;
const CAPTURE_CROP_OFFSET_MIN = -150;
const CAPTURE_CROP_OFFSET_MAX = 150;
const AIM_REFERENCE_OFFSET_MAX = 320;
const CROSSHAIR_MAX_COLORS = 3;
const CROSSHAIR_PRESET_COLORS = ["red", "green", "blue", "cyan", "yellow", "white", "black"];
const CROSSHAIR_SLOT_DEFAULT_COLORS = ["red", "green", "blue"];
const AIM_OVERLAY_CONFIG_IDS = new Set([
  "capture_crop_size",
  "range_factor",
  "controller_aim_reference_offset_x",
  "controller_aim_reference_offset_y",
]);
const DYNAMIC_OFFSET_INPUT_IDS = [
  "capture_crop_offset_x",
  "capture_crop_offset_y",
  "controller_aim_reference_offset_x",
  "controller_aim_reference_offset_y",
];
const RANGE_BINDINGS = [
  ["video_detection_confidence", "video_detection_confidence_range"],
  ["video_detection_iou", "video_detection_iou_range"],
  ["sens", "sens_range"],
  ["range_factor", "range_factor_range"],
];
const NUMERIC_RANGE_LIMITS = {
  capture_crop_size: [CROP_SIZE_MIN, CROP_SIZE_MAX],
  sens: [0, 5],
  aim_profile_sensitivity: [0.1, 3],
  aim_profile_fov_scale: [AIM_PROFILE_FOV_SCALE_MIN, AIM_PROFILE_FOV_SCALE_MAX],
  aim_profile_offset_x: [AIM_PROFILE_AXIS_OFFSET_MIN, AIM_PROFILE_AXIS_OFFSET_MAX],
  aim_profile_offset_y: [AIM_PROFILE_AXIS_OFFSET_MIN, AIM_PROFILE_AXIS_OFFSET_MAX],
  range_factor: [0, 1],
  capture_crop_offset_x: [CAPTURE_CROP_OFFSET_MIN, CAPTURE_CROP_OFFSET_MAX],
  capture_crop_offset_y: [CAPTURE_CROP_OFFSET_MIN, CAPTURE_CROP_OFFSET_MAX],
  controller_kp_x: [0, 100],
  controller_kp_y: [0, 100],
  controller_ki_x: [0, 50],
  controller_ki_y: [0, 50],
  controller_kd_x: [0, 30],
  controller_kd_y: [0, 30],
  controller_predict_x: [0, 3],
  controller_predict_y: [0, 3],
  controller_rate_x: [0, 1],
  controller_rate_y: [0, 1],
  controller_output_deadzone: [0, 20],
  controller_pull_curve_strength: [0, 2],
  controller_pull_curve_jitter_px: [0, 12],
  controller_pull_curve_min_distance: [0, 1280],
  controller_continuous_lead_enter_distance: [0, 1280],
  controller_continuous_lead_scale: [0, 1],
  controller_continuous_lead_fade_in_ms: [0, 1000],
  controller_continuous_lead_fade_out_ms: [0, 1000],
  controller_continuous_lead_near_disable_ratio: [0, 1],
  controller_aim_reference_offset_x: [-AIM_REFERENCE_OFFSET_MAX, AIM_REFERENCE_OFFSET_MAX],
  controller_aim_reference_offset_y: [-AIM_REFERENCE_OFFSET_MAX, AIM_REFERENCE_OFFSET_MAX],
  controller_selector_lost_grace_ms: [0, 1000],
  controller_y_axis_fire_release_delay_sec: [0, 2],
  autoCalibrationGainX: [0.03, 8],
  autoCalibrationGainY: [0.03, 8],
  autoCalibrationDelay: [0, 50],
  recoil_strength: [0, 300],
  recoil_speed: [0.1, 3],
  recoil_target_lost_release_ms: [0, 3000],
  recoil_trigger_delay_ms: [0, 3000],
  recoil_humanize_curve_strength: [0, 1],
  recoil_humanize_jitter_px: [0, 2],
  recoil_humanize_jitter_frequency: [0, 30],
  rapid_fire_press_base_ms: [30, 300],
  rapid_fire_interval_base_ms: [8, 120],
  crosshair_roi_w: [2, 150],
  crosshair_roi_h: [2, 150],
  fan_control_start_celsius: [20, 90],
  fan_control_full_celsius: [20, 90],
  fan_control_min_pwm_percent: [0, 100],
  fan_control_max_pwm_percent: [0, 100],
  hailo_pipeline_depth: [1, 4],
};
const OVERVIEW_DEFAULTS = {
  capture_crop_size: 320,
  range_factor: 1,
  capture_crop_offset_x: 0,
  capture_crop_offset_y: 0,
  controller_aim_reference_offset_x: 0,
  controller_aim_reference_offset_y: 0,
  video_detection_confidence: 0.25,
  video_detection_iou: 0.45,
};

function overviewDefaults() {
  return {
    ...OVERVIEW_DEFAULTS,
    capture_crop_size: modelInputCropSize(currentModel()) || OVERVIEW_DEFAULTS.capture_crop_size,
  };
}

const CONTROLLER_DEFAULTS = {
  kp_x: 15,
  kp_y: 15,
  ki_x: 0,
  ki_y: 0,
  kd_x: 0,
  kd_y: 0,
  predict_x: 0.5,
  predict_y: 0.4,
  rate_x: 0.4,
  rate_y: 0.3,
  smooth_x: 9900,
  smooth_y: 9900,
  output_deadzone: 1,
  pull_curve_enabled: true,
  pull_curve_strength: 0.8,
  pull_curve_jitter_px: 3,
  pull_curve_min_distance: 80,
  continuous_lead_enabled: false,
  continuous_lead_enter_distance: 150,
  continuous_lead_scale: 0.5,
  continuous_lead_fade_in_ms: 300,
  continuous_lead_fade_out_ms: 300,
  continuous_lead_near_disable_ratio: 0.66,
  block_physical_mouse_x_while_aiming: false,
  block_physical_mouse_y_while_aiming: false,
  aim_fire_lock_y: false,
  aim_reference_offset_x: 0,
  aim_reference_offset_y: 0,
  y_axis_fire_hotkey: "left",
  y_axis_fire_release_delay_sec: 0.3,
  selector_lost_grace_ms: 30,
};
const MOVEMENT_CONTROL_DEFAULTS = {
  sens: 1,
  mouse_output_mode: "passthrough",
  controller: CONTROLLER_DEFAULTS,
};
const KMBOXNET_DEFAULTS = {
  enabled: false,
  ip: "",
  port: 0,
  uuid: "",
  monitor_port: 5001,
  encrypted: false,
  timeout_ms: 300,
};
const CATNET_DEFAULTS = {
  enabled: false,
  ip: "192.168.7.1",
  port: 8888,
  uuid: "",
  monitor_port: 1234,
  timeout_ms: 300,
};
const MAKCU_DEFAULTS = {
  enabled: false,
  port: "auto",
  high_speed: true,
};
const FERRUM_DEFAULTS = {
  enabled: false,
  port: "auto",
};
const KMBOXB_DEFAULTS = {
  enabled: false,
  port: "auto",
};
const RECOIL_DEFAULTS = {
  enabled: false,
  only_when_target_visible: true,
  target_lost_release_ms: 200,
  hotkey: "left",
  hotkey2: "",
  hotkey_mode: "any",
  trigger_delay_enabled: false,
  trigger_delay_ms: 120,
  strength: 0,
  speed: 1,
  humanize_enabled: true,
  humanize_curve_strength: 0.45,
  humanize_jitter_px: 0.25,
  humanize_jitter_frequency: 8,
};
const AUTO_TRIGGER_DEFAULTS = {
  enabled: false,
  activation_hotkey: "auto",
  profiles: [],
  enter_delay_min_ms: 0,
  enter_delay_max_ms: 0,
  cooldown_min_ms: 120,
  cooldown_max_ms: 220,
  spray_release_delay_ms: 500,
  fire_mode: "tap",
  continuous_mode: false,
  spray_recoil_assist: false,
};
const RAPID_FIRE_DEFAULTS = {
  enabled: false,
  hotkey: "forward",
  press_base_ms: 170,
  interval_base_ms: 13,
};
const CROSSHAIR_DEFAULTS = {
  detection_enabled: false,
  only_when_fire_held: true,
  fire_hotkey: "left",
  hotkey: "left",
  hotkey2: "",
  hotkey_mode: "any",
  roi_w: 50,
  roi_h: 50,
  preset_colors: ["red"],
};
const FAN_CONTROL_DEFAULTS = {
  enabled: false,
  temperature_source: "auto",
  start_celsius: 45,
  full_celsius: 70,
  min_pwm_percent: 100,
  max_pwm_percent: 100,
  stop_hysteresis_celsius: 3,
};
const AUTO_BACK_FLICK_DEFAULTS = {
  enabled: false,
  random_direction: false,
  dodge_away_from_target: false,
  class_id: 0,
  class_filter_mask: 1,
  turn_pixels: 3000,
  wait_ms: 150,
  turn_random: 50,
  return_random: 20,
  steps: 100,
  cooldown_ms: 3000,
  trigger_delay_ms: 0,
  confidence: 0.25,
  class_configs: [],
};
const HOTKEY_GUARD_DEFAULTS = {
  enabled: false,
  toggle_hotkey: "middle",
};
const DISPLAY_NATIVE_MODES = [
  "",
  "1080p60",
  "1080p90",
  "1080p120",
  "1080p144",
  "1080p240",
  "1440p60",
  "1440p120",
  "1440p144",
  "2160p60",
];
const DISPLAY_NATIVE_MODE_LABELS = {
  "": "自动使用 EDID 模式",
  "1080p60": "1920x1080 @ 60 Hz",
  "1080p90": "1920x1080 @ 90 Hz",
  "1080p120": "1920x1080 @ 120 Hz",
  "1080p144": "1920x1080 @ 144 Hz",
  "1080p240": "1920x1080 @ 240 Hz",
  "1440p60": "2560x1440 @ 60 Hz",
  "1440p120": "2560x1440 @ 120 Hz",
  "1440p144": "2560x1440 @ 144 Hz",
  "2160p60": "3840x2160 @ 60 Hz",
};
const DISPLAY_IDENTITY_FIELD_IDS = ["display_name", "display_vendor", "display_product_id", "display_serial"];
const DISPLAY_EDID_MODE_FIELD_IDS = ["displayEdidModeWidth", "displayEdidModeHeight", "displayEdidModeRefresh"];
const DISPLAY_DYNAMIC_MODE_RE = /^(\d{3,4})x(\d{3,4})@(\d{2,3})$/;
const DISPLAY_DYNAMIC_MODE_MIN_WIDTH = 640;
const DISPLAY_DYNAMIC_MODE_MAX_WIDTH = 4095;
const DISPLAY_DYNAMIC_MODE_MIN_HEIGHT = 400;
const DISPLAY_DYNAMIC_MODE_MAX_HEIGHT = 4095;
const DISPLAY_DYNAMIC_MODE_MIN_REFRESH = 24;
const DISPLAY_DYNAMIC_MODE_MAX_REFRESH = 360;
const DISPLAY_DYNAMIC_MODE_MIN_PIXEL_CLOCK_KHZ = 25000;
const DISPLAY_DYNAMIC_MODE_MAX_PIXEL_CLOCK_KHZ = 600000;
const DISPLAY_DYNAMIC_MODE_H_BLANK = 48 + 32 + 80;
const DISPLAY_DYNAMIC_MODE_V_BLANK = 3 + 5 + 77;
const DISCLAIMER_STORAGE_KEY = "ttbox_disclaimer_ack_v1";
const THEME_STORAGE_KEY = "ttbox_theme";
const UI_BRAND_TTBOX = "ttbox";
const MOUSE_MODE_SWITCH_SUPPRESS_MS = 180000;

const state = {
  data: null,
  config: null,
  configReady: false,
  isPopulating: false,
  isApplying: false,
  applyQueued: false,
  applyTimer: null,
  modelListSignature: "",
  modelCardsRenderSignature: "",
  modelGameFilter: "all",
  modelBackendFilter: "all",
  modelGameOptions: [],
  modelGameSuggestionOpen: false,
  modelImportState: "idle",
  modelImportCloseTimer: null,
  remoteHost: "",
  remoteConnecting: false,
  remoteRefreshing: false,
  remoteError: "",
  makcuDevices: [],
  ferrumDevices: [],
  kmboxbDevices: [],
  presetNames: [],
  presetSelectedName: "",
  presetListSignature: "",
  presetSuggestionOpen: false,
  presetAutoSaveInFlight: false,
  presetAutoSaveQueued: false,
  presetAutoSaveName: "",
  presetAutoSaveConfig: null,
  modelPresetBindingOpenId: "",
  modelGameBindingOpenId: "",
  modelRemoteFrameBindingOpenId: "",
  modelRknnConcurrencyBindingOpenId: "",
  modelHailoPipelineBindingOpenId: "",
  modelClassNamesEditModelId: "",
  aimClassRenderSignature: "",
  autoTriggerClassRenderSignature: "",
  updatePlan: null,
  updateVersions: [],
  updateVersionPayload: null,
  updateStatus: null,
  updateStatusTimer: null,
  updateStatusInFlight: false,
  updateRefreshScheduled: false,
  themeStore: null,
  hailo: null,
  hailoStatusTimer: null,
  hailoStatusInFlight: false,
  licenseRecoveryInProgress: false,
  lastLicenseRecoveryMessage: "",
  wifi: null,
  wifiSelectedSsid: "",
  wifiModePanel: "client",
  wifiApCredentialsInitialized: false,
  lanBlockDevices: [],
  displayRealMonitor: null,
  displayHardwarePayload: null,
  storageExpandBusy: false,
  systemHostname: "",
  webPort: 8080,
  currentVersion: "",
  activePageId: "home-page",
  activeControlSectionId: "control-section-auto-calibration",
  activeAssistSectionId: "assist-section-recoil",
  navigationLockedToLicense: false,
  licenseStatusLoaded: false,
  mouseModeSwitchSuppressUntil: 0,
  uiBrand: UI_BRAND_TTBOX,
  theme: "dark",
  livePollTimer: null,
  livePollInFlight: false,
  livePollErrorNotified: false,
  autoCalibration: null,
  autoCalibrationPollTimer: null,
  autoCalibrationPollInFlight: false,
  autoCalibrationDraftDirty: false,
  autoCalibrationManualSaving: false,
  activationRedirecting: false,
  autoStartSaving: false,
  blockedPhysicalButtons: [],
  blockedPhysicalButtonsDraft: [],
  autoBackFlickClassConfigs: [],
  autoBackFlickEditingClassId: -1,
};

const LICENSE_GATE_PAGE_ID = "license-page";

function $(id) {
  return document.getElementById(id);
}

function setText(id, value) {
  const el = $(id);
  if (el) {
    el.textContent = value === undefined || value === null ? "" : String(value);
  }
}

function on(id, eventName, handler) {
  const el = $(id);
  if (el) {
    el.addEventListener(eventName, handler);
  }
}

function autoStartToggles() {
  return Array.from(document.querySelectorAll("[data-auto-start-toggle]"));
}

function syncAutoStartControls(setting) {
  if (!setting || state.autoStartSaving) return;
  const enabled = setting.enabled === true;
  autoStartToggles().forEach((toggle) => {
    toggle.checked = enabled;
    toggle.disabled = false;
    const control = toggle.closest(".auto-start-control");
    if (control) {
      control.title = setting.message || (enabled ? "下次开机将自动启动采集和推理" : "开机后保持停止状态");
    }
  });
}

function bindAutoStartControls() {
  autoStartToggles().forEach((toggle) => {
    toggle.addEventListener("change", async () => {
      const enabled = toggle.checked;
      const previous = !!(state.data && state.data.auto_start && state.data.auto_start.enabled);
      state.autoStartSaving = true;
      autoStartToggles().forEach((item) => {
        item.checked = enabled;
        item.disabled = true;
      });
      try {
        const setting = await api("/api/settings/auto-start", {
          method: "PUT",
          headers: { "Content-Type": "application/json" },
          body: JSON.stringify({ enabled }),
        });
        if (state.data) state.data.auto_start = setting;
        state.autoStartSaving = false;
        syncAutoStartControls(setting);
        showToast(enabled ? "开机自启动已开启，下次开机生效" : "开机自启动已关闭");
      } catch (error) {
        state.autoStartSaving = false;
        syncAutoStartControls({ enabled: previous });
        showToast(error.message || String(error), true);
      }
    });
  });
}

function integrateFanSettings() {
  // Universal - always apply
  const fanPage = $("fan-page");
  const assistStack = document.querySelector("#assist-page .assist-section-stack");
  if (!fanPage || !assistStack || fanPage.parentElement === assistStack) return;

  fanPage.removeAttribute("data-page");
  fanPage.classList.remove("page-card", "is-active");
  fanPage.classList.add("assist-section", "xh-fan-settings-section");
  fanPage.setAttribute("data-assist-section", "");
  fanPage.hidden = true;
  assistStack.appendChild(fanPage);
}

function normalizeUiBrand(value) {
  return UI_BRAND_TTBOX;
}

function brandFromPayload(payload) {
  return UI_BRAND_TTBOX;
}

function brandConfig(brand) {
  return {
    uiBrand: UI_BRAND_TTBOX,
    title: "TTBOX 控制台",
    eyebrow: "TTBOX SYSTEM",
    mark: "TT",
    allowThemeSwitch: true,
    defaultLocalName: "ttbox",
    defaultHotspotSsid: "TTBOX",
    fallbackResetText: "重置默认 Wi-Fi",
  };
}

function currentBrandConfig() {
  return brandConfig(state.uiBrand);
}

function storedTheme() {
  try {
    return localStorage.getItem(THEME_STORAGE_KEY) === "light" ? "light" : "dark";
  } catch {
    return "dark";
  }
}

function saveStoredTheme(theme) {
  try {
    localStorage.setItem(THEME_STORAGE_KEY, theme);
  } catch {
    // localStorage can be disabled in private or locked-down browser modes.
  }
}

function updateThemeToggleLabel() {
  const button = $("themeToggleButton");
  if (!button) return;
  const isLight = state.theme === "light";
  button.textContent = isLight ? "深色" : "浅色";
  button.dataset.icon = isLight ? "moon" : "sun";
  button.setAttribute("data-symbol", isLight ? "\u263e" : "\u2600");
  button.setAttribute("aria-label", isLight ? "切换到深色主题" : "切换到浅色主题");
  const customVisualTheme = (document.documentElement.dataset.visualTheme || "default") !== "default";
  button.hidden = customVisualTheme;
}

function applyTheme(theme, { persist = false } = {}) {
  const nextTheme = theme === "light" ? "light" : "dark";
  state.theme = nextTheme;
  document.documentElement.dataset.theme = nextTheme;
  if (persist) {
    saveStoredTheme(nextTheme);
  }
  updateThemeToggleLabel();
}

function applyBrand(payload) {
  const config = brandConfig(brandFromPayload(payload));
  state.uiBrand = config.uiBrand;
  document.documentElement.dataset.uiBrand = config.uiBrand;
  document.body.classList.add("ui-brand-ttbox");
  // TTBOX brand - no special class needed
    const mark = $("brandMark");
  const eyebrow = $("brandEyebrow");
  const title = $("brandTitle");
  if (mark) mark.textContent = config.mark;
  if (eyebrow) eyebrow.textContent = config.eyebrow;
  if (title) title.textContent = config.title;
  document.title = config.title;

  const hostnameInput = $("lanHostnameInput");
  if (hostnameInput) hostnameInput.placeholder = config.defaultLocalName;
  const apSsidInput = $("wifiApSsid");
  if (apSsidInput) apSsidInput.placeholder = config.defaultHotspotSsid;
  const fallbackButton = $("wifiFallbackButton");
  if (fallbackButton) fallbackButton.textContent = config.fallbackResetText;

  const themeButton = $("themeToggleButton");
  if (themeButton) {
    const customVisualTheme = (document.documentElement.dataset.visualTheme || "default") !== "default";
    themeButton.hidden = !config.allowThemeSwitch || customVisualTheme;
  }
  const visualThemeColor = document.documentElement.dataset.visualThemeColor || "system";
  applyTheme(["light", "dark"].includes(visualThemeColor) ? visualThemeColor : storedTheme());
}

function initThemeControls() {
  const button = $("themeToggleButton");
  if (!button) return;
  button.addEventListener("click", () => {
    if ((document.documentElement.dataset.visualTheme || "default") !== "default") return;
    applyTheme(state.theme === "light" ? "dark" : "light", { persist: true });
  });
  updateThemeToggleLabel();
}

function setThemePreviewOpen(open, theme = null) {
  const dialog = $("themePreviewDialog");
  if (!dialog) return;
  dialog.hidden = !open;
  if (!open || !theme) return;
  const title = $("themePreviewTitle");
  const list = $("themePreviewList");
  if (title) title.textContent = theme.title || "主题预览";
  if (list) {
    list.innerHTML = (theme.previews || []).map((preview) => `
      <figure class="theme-preview-item">
        <img src="${escapeAttr(preview.url)}" alt="${escapeAttr(`${theme.title || "主题"} ${preview.label || "预览"}`)}" loading="lazy">
        <figcaption>${escapeHtml(preview.label || "预览")}</figcaption>
      </figure>
    `).join("");
  }
}

function themeCardAction(theme) {
  if (theme.id === "default") {
    return theme.active
      ? '<button class="ghost-button" type="button" disabled>使用中</button>'
      : '<button class="primary-button" type="button" data-theme-use="default">使用主题</button>';
  }
  if (!theme.owned) {
    return `<div class="theme-redeem-row">
      <input type="text" autocomplete="off" maxlength="32" placeholder="输入 YT 主题卡密" data-theme-key-input="${escapeAttr(theme.id)}">
      <button class="primary-button" type="button" data-theme-redeem="${escapeAttr(theme.id)}">解锁并使用</button>
    </div>`;
  }
  const buttons = [];
  if (!theme.installed) {
    buttons.push(`<button class="primary-button" type="button" data-theme-install="${escapeAttr(theme.id)}">安装主题</button>`);
  } else if (theme.update_available) {
    buttons.push(`<button class="primary-button" type="button" data-theme-install="${escapeAttr(theme.id)}">更新主题</button>`);
  }
  if (theme.installed) {
    buttons.push(theme.active
      ? '<button class="ghost-button" type="button" disabled>使用中</button>'
      : `<button class="primary-button" type="button" data-theme-use="${escapeAttr(theme.id)}">使用主题</button>`);
  }
  return `<div class="theme-card-actions">${buttons.join("")}</div>`;
}

function renderThemeStore(payload) {
  state.themeStore = payload;
  const list = $("themeCardList");
  const status = $("themeStoreStatus");
  const purchaseButton = $("purchaseThemesButton");
  if (purchaseButton) {
    let purchaseUrl = "";
    try {
      const parsed = new URL(String(payload && payload.purchase_url || ""));
      if (["http:", "https:"].includes(parsed.protocol)) purchaseUrl = parsed.href;
    } catch {
      purchaseUrl = "";
    }
    purchaseButton.hidden = !purchaseUrl;
    if (purchaseUrl) purchaseButton.href = purchaseUrl;
    else purchaseButton.removeAttribute("href");
  }
  if (!list || !status) return;
  const themes = Array.isArray(payload && payload.themes) ? payload.themes : [];
  status.classList.toggle("is-error", !!payload.sync_error);
  status.textContent = payload.sync_error
    ? `服务器暂时不可用，正在显示本地主题：${payload.sync_error}`
    : `共 ${themes.length} 款主题 · ${payload.offline ? "离线目录" : "权益已同步"}`;
  list.innerHTML = themes.map((theme) => {
    const preview = (theme.previews || [])[0];
    const badge = theme.active ? "使用中" : theme.owned ? (theme.installed ? "已安装" : "已拥有") : "需主题卡密";
    const version = theme.id === "default" ? "系统内置" : `版本 ${theme.installed_version || theme.latest_version || "--"}`;
    return `<article class="theme-card" data-theme-card="${escapeAttr(theme.id)}">
      <div class="theme-card-preview">
        ${preview
          ? `<img src="${escapeAttr(preview.url)}" alt="${escapeAttr(`${theme.title || "主题"}预览`)}" loading="lazy">`
          : `<div class="theme-card-preview-empty">${escapeHtml(theme.id === "default" ? "TTBOX" : theme.title || "主题")}</div>`}
        <span class="theme-card-badge">${escapeHtml(badge)}</span>
      </div>
      <div class="theme-card-body">
        <div class="theme-card-heading"><div><h3>${escapeHtml(theme.title || theme.id)}</h3><p>${escapeHtml(version)}</p></div>${theme.update_available ? '<span class="pill">可更新</span>' : ""}</div>
        <p class="theme-card-description">${escapeHtml(theme.description || "")}</p>
        ${(theme.previews || []).length ? `<button class="ghost-button" type="button" data-theme-preview="${escapeAttr(theme.id)}">查看预览</button>` : ""}
        ${theme.compatible === false ? '<p class="field-error">当前系统版本与该主题不兼容</p>' : themeCardAction(theme)}
      </div>
    </article>`;
  }).join("");
}

async function refreshThemeStore({ silent = false } = {}) {
  if (!$(`themeCardList`)) return null;
  const status = $("themeStoreStatus");
  if (status && !silent) {
    status.classList.remove("is-error");
    status.textContent = "正在同步主题目录";
  }
  const payload = await api("/api/themes");
  renderThemeStore(payload);
  const currentVisualTheme = document.documentElement.dataset.visualTheme || "default";
  if (currentVisualTheme !== "default" && payload.active_theme_id === "default") {
    window.location.reload();
  }
  return payload;
}

async function redeemStoreTheme(themeId) {
  const input = document.querySelector(`[data-theme-key-input="${CSS.escape(themeId)}"]`);
  const themeKey = String(input && input.value || "").trim().toUpperCase();
  if (!themeKey) throw new Error("请输入主题卡密");
  const result = await api("/api/themes/redeem", {
    method: "POST",
    headers: { "Content-Type": "application/json" },
    body: JSON.stringify({ theme_id: themeId, theme_key: themeKey }),
  });
  showToast("主题已解锁并安装");
  window.location.reload();
  return result;
}

async function installStoreTheme(themeId) {
  const result = await api(`/api/themes/${encodeURIComponent(themeId)}/install`, { method: "POST" });
  await refreshThemeStore({ silent: true });
  showToast("主题已安装");
  if ((document.documentElement.dataset.visualTheme || "default") === themeId) {
    window.location.reload();
  }
  return result;
}

async function useStoreTheme(themeId) {
  await api("/api/themes/current", {
    method: "PUT",
    headers: { "Content-Type": "application/json" },
    body: JSON.stringify({ theme_id: themeId }),
  });
  window.location.reload();
}

function initThemeStore() {
  const list = $("themeCardList");
  if (!list) return;
  list.addEventListener("click", (event) => {
    const previewButton = event.target.closest("[data-theme-preview]");
    const redeemButton = event.target.closest("[data-theme-redeem]");
    const installButton = event.target.closest("[data-theme-install]");
    const useButton = event.target.closest("[data-theme-use]");
    if (previewButton) {
      const theme = (state.themeStore && state.themeStore.themes || []).find((item) => item.id === previewButton.dataset.themePreview);
      if (theme) setThemePreviewOpen(true, theme);
    } else if (redeemButton) {
      runUiAction(() => redeemStoreTheme(redeemButton.dataset.themeRedeem));
    } else if (installButton) {
      runUiAction(() => installStoreTheme(installButton.dataset.themeInstall));
    } else if (useButton) {
      runUiAction(() => useStoreTheme(useButton.dataset.themeUse));
    }
  });
  on("refreshThemesButton", "click", () => runUiAction(() => refreshThemeStore()));
  on("closeThemePreviewButton", "click", () => setThemePreviewOpen(false));
  on("themePreviewDialog", "click", (event) => {
    if (event.target === event.currentTarget) setThemePreviewOpen(false);
  });
  refreshThemeStore({ silent: true }).catch((error) => {
    const status = $("themeStoreStatus");
    if (status) {
      status.classList.add("is-error");
      status.textContent = error.message || String(error);
    }
  });
}

function escapeHtml(value) {
  return String(value === undefined || value === null ? "" : value)
    .replace(/&/g, "&amp;")
    .replace(/</g, "&lt;")
    .replace(/>/g, "&gt;")
    .replace(/"/g, "&quot;")
    .replace(/'/g, "&#039;");
}

function escapeAttr(value) {
  return escapeHtml(value);
}

async function copyTextToClipboard(text) {
  const value = String(text || "");
  if (!value) {
    throw new Error("没有可复制的内容");
  }
  if (navigator.clipboard && window.isSecureContext) {
    await navigator.clipboard.writeText(value);
    return;
  }
  const textarea = document.createElement("textarea");
  textarea.value = value;
  textarea.setAttribute("readonly", "");
  textarea.style.position = "fixed";
  textarea.style.left = "-9999px";
  textarea.style.top = "0";
  document.body.appendChild(textarea);
  textarea.select();
  const copied = document.execCommand("copy");
  textarea.remove();
  if (!copied) {
    throw new Error("浏览器拒绝复制，请手动复制设备码");
  }
}

function formatDateTime(value) {
  const raw = String(value || "").trim();
  if (!raw) return "--";
  const date = new Date(raw);
  if (Number.isNaN(date.getTime())) return raw;
  return date.toLocaleString("zh-CN", { hour12: false });
}

function compareVersionText(left, right) {
  const leftParts = String(left || "").match(/\d+|[A-Za-z]+|[^A-Za-z\d]+/g) || [];
  const rightParts = String(right || "").match(/\d+|[A-Za-z]+|[^A-Za-z\d]+/g) || [];
  const length = Math.max(leftParts.length, rightParts.length);
  for (let index = 0; index < length; index += 1) {
    const leftPart = leftParts[index] || "";
    const rightPart = rightParts[index] || "";
    if (leftPart === rightPart) {
      continue;
    }
    const leftNumber = /^\d+$/.test(leftPart) ? Number(leftPart) : NaN;
    const rightNumber = /^\d+$/.test(rightPart) ? Number(rightPart) : NaN;
    if (Number.isFinite(leftNumber) && Number.isFinite(rightNumber)) {
      return leftNumber - rightNumber;
    }
    return leftPart.localeCompare(rightPart);
  }
  return 0;
}

function stableStringify(value) {
  if (Array.isArray(value)) {
    return `[${value.map(stableStringify).join(",")}]`;
  }
  if (value && typeof value === "object") {
    return `{${Object.keys(value).sort().map((key) => {
      return `${JSON.stringify(key)}:${stableStringify(value[key])}`;
    }).join(",")}}`;
  }
  return JSON.stringify(value);
}

function cloneJson(value) {
  return value == null ? value : JSON.parse(JSON.stringify(value));
}

function showToast(message, isError = false) {
  const el = $("toast");
  if (!el) {
    return;
  }
  el.textContent = message;
  el.className = `toast ${isError ? "error" : ""}`;
  clearTimeout(showToast._timer);
  showToast._timer = setTimeout(() => {
    el.className = "toast hidden";
  }, 2600);
}

function markMouseModeSwitching(durationMs = MOUSE_MODE_SWITCH_SUPPRESS_MS) {
  state.mouseModeSwitchSuppressUntil = Math.max(
    state.mouseModeSwitchSuppressUntil || 0,
    Date.now() + durationMs,
  );
}

function isMouseModeSwitching() {
  return Date.now() < (state.mouseModeSwitchSuppressUntil || 0);
}

function isTransientDaemonBusy(message) {
  const text = String(message || "").toLowerCase();
  return text.includes("daemon socket unavailable")
    && (text.includes("timed out") || text.includes("resource temporarily unavailable") || text.includes("errno 11"));
}

function suppressMouseSwitchDaemonTimeout(message) {
  if (!isMouseModeSwitching() || !isTransientDaemonBusy(message)) {
    return false;
  }
  setHardwareStatus("mouseHardwareStatus", "切换中");
  return true;
}

function setAnyModalOpen() {
  const hasOpenModal = Array.from(document.querySelectorAll(".modal-backdrop"))
    .some((modal) => !modal.hidden);
  document.body.classList.toggle("modal-open", hasOpenModal);
}

function setDisclaimerDialogOpen(open) {
  const dialog = $("disclaimerDialog");
  if (!dialog) {
    return;
  }
  updateDisclaimerActions();
  dialog.hidden = !open;
  setAnyModalOpen();
  if (open) {
    const acceptButton = $("acceptDisclaimerButton");
    if (acceptButton) {
      acceptButton.focus();
    }
  }
}

function maybeShowDisclaimer() {
  try {
    if (window.localStorage && window.localStorage.getItem(DISCLAIMER_STORAGE_KEY) === "hidden") {
      window.setTimeout(() => maybeShowAnnouncement(), 120);
      return;
    }
  } catch {
    // Some embedded browsers can disable localStorage; showing the dialog is still fine.
  }
  window.setTimeout(() => setDisclaimerDialogOpen(true), 120);
}

function isLicenseValid() {
  return !!(state.data && state.data.state && state.data.state.license && state.data.state.license.valid);
}

function updateDisclaimerActions() {
  const hideButton = $("hideDisclaimerButton");
  if (!hideButton) {
    return;
  }
  const valid = isLicenseValid();
  hideButton.disabled = !valid;
  hideButton.title = valid ? "" : "设备激活后才可选择不再显示";
}

function licenseStateFromPayload(payload) {
  return (payload && payload.state && payload.state.license) || {};
}

function friendlyLicenseMessage(message, fallback = "设备未激活，请输入激活码后继续使用。") {
  const text = String(message || "").trim();
  if (!text) {
    return fallback;
  }
  let reason = text;
  if (text.startsWith("{")) {
    try {
      const payload = JSON.parse(text);
      reason = String(payload.error || (payload.data && payload.data.reason) || payload.message || text).trim();
    } catch {
      reason = text;
    }
  }
  if (reason === "valid license is required" || reason === "license is not active on this server") {
    return "授权校验失败，请点击授权修复或重新输入激活码。";
  }
  if (reason === "license key is already bound to another device") {
    return "激活码已绑定其他设备。";
  }
  return reason || fallback;
}

function activeLicenseMessage() {
  const license = licenseStateFromPayload(state.data);
  return friendlyLicenseMessage(license.message);
}

function syncLicenseKeyInputs(sourceInput) {
  const value = sourceInput ? sourceInput.value : "";
  ["licenseKeyInput", "licenseGateKeyInput"].forEach((id) => {
    const input = $(id);
    if (input && input !== sourceInput) {
      input.value = value;
    }
  });
}

function updateLicenseGateStatus(message) {
  const status = $("licenseGateStatus");
  if (status) {
    status.textContent = message || activeLicenseMessage();
  }
}

function focusLicenseGateInput() {
  const input = $("licenseGateKeyInput") || $("licenseKeyInput");
  if (input) {
    window.setTimeout(() => input.focus(), 40);
  }
}

function hideDisclaimer(remember) {
  if (remember) {
    if (!isLicenseValid()) {
      showToast("设备激活后才可选择不再显示", true);
      updateDisclaimerActions();
      return;
    }
    try {
      window.localStorage.setItem(DISCLAIMER_STORAGE_KEY, "hidden");
    } catch {
      showToast("当前浏览器无法保存不再显示设置", true);
    }
  }
  setDisclaimerDialogOpen(false);
  maybeShowAnnouncement();
}

function sleep(ms) {
  return new Promise((resolve) => window.setTimeout(resolve, ms));
}

function nextPaint() {
  return new Promise((resolve) => {
    window.requestAnimationFrame(() => window.requestAnimationFrame(resolve));
  });
}

function setActivationBusy(busy) {
  ["activateLicenseButton", "licenseGateActivateButton", "licenseGateRefreshButton"].forEach((id) => {
    const button = $(id);
    if (button) {
      button.disabled = !!busy;
    }
  });
  ["licenseKeyInput", "licenseGateKeyInput"].forEach((id) => {
    const input = $(id);
    if (input) {
      input.disabled = !!busy;
    }
  });
}

function setActivationSetupProgress(progress, activeStep, message) {
  const overlay = $("activationSetupOverlay");
  if (overlay) {
    overlay.hidden = false;
  }
  document.body.classList.add("modal-open");
  const progressEl = $("activationSetupProgress");
  if (progressEl) {
    progressEl.style.width = `${Math.max(0, Math.min(100, progress))}%`;
  }
  ["License", "Display", "Wait", "Mouse"].forEach((name, index) => {
    const stepEl = $(`activationSetupStep${name}`);
    if (!stepEl) {
      return;
    }
    stepEl.classList.toggle("is-done", index < activeStep);
    stepEl.classList.toggle("is-active", index === activeStep);
  });
  const messageEl = $("activationSetupMessage");
  if (messageEl) {
    messageEl.textContent = message || "请保持设备供电，不要关闭页面。";
  }
}

function clearActivationSetupProgress() {
  const overlay = $("activationSetupOverlay");
  if (overlay) {
    overlay.hidden = true;
  }
  document.body.classList.remove("modal-open");
  setAnyModalOpen();
}

async function runFirstActivationSetup() {
  setActivationSetupProgress(12, 0, "授权已通过，正在准备设备硬件身份。");
  await loadHardware().catch(() => {});

  setActivationSetupProgress(32, 1, "正在随机并应用显示器模式。");
  randomizeDisplayHardware();
  const displayConfig = validateDisplayHardware();
  const displayResult = await api("/api/hardware/display", {
    method: "PUT",
    headers: { "Content-Type": "application/json" },
    body: JSON.stringify({ config: displayConfig, apply: true, patch_boot_image: false, reboot_after_apply: false }),
  });
  populateDisplayHardware({
    available: true,
    config: displayResult.config,
    status: displayResult.result,
  });

  setActivationSetupProgress(58, 2, "显示器模式已应用，等待显示链路稳定 10 秒。");
  await sleep(10000);

  setActivationSetupProgress(82, 3, "正在启用完整透传，保留真实鼠标硬件信息。");
  const mouseResult = await api("/api/hardware/mouse/mode", {
    method: "PUT",
    headers: { "Content-Type": "application/json" },
    body: JSON.stringify({ mode: "full_passthrough", apply_now: true }),
  });
  await sleep(2500);
  populateMouseHardware(mouseResult);
  setActivationSetupProgress(100, 4, "初始化完成，Windows 会重新枚举完整鼠标设备，页面保持可用。");
}

function setAnnouncementDialogOpen(open) {
  const dialog = $("announcementDialog");
  if (!dialog) {
    return;
  }
  dialog.hidden = !open;
  setAnyModalOpen();
  if (open) {
    const ackButton = $("ackAnnouncementButton");
    if (ackButton) {
      ackButton.focus();
    }
  }
}

function renderAnnouncement(announcement) {
  const title = $("announcementTitle");
  const body = $("announcementBody");
  if (title) {
    title.textContent = announcement.title || "公告";
  }
  if (body) {
    body.textContent = announcement.content || "";
  }
  const dialog = $("announcementDialog");
  if (dialog) {
    dialog.dataset.version = announcement.version || "";
  }
}

async function maybeShowAnnouncement() {
  const disclaimer = $("disclaimerDialog");
  const announcementDialog = $("announcementDialog");
  if ((disclaimer && !disclaimer.hidden) || (announcementDialog && !announcementDialog.hidden)) {
    return;
  }
  let announcement = null;
  try {
    announcement = await api("/api/announcement");
  } catch {
    return;
  }
  if (!announcement || !announcement.enabled || !announcement.content) {
    return;
  }
  renderAnnouncement(announcement);
  setAnnouncementDialogOpen(true);
}

function acknowledgeAnnouncement() {
  setAnnouncementDialogOpen(false);
}

function payloadContainsRevocation(payload) {
  if (!payload || typeof payload !== "object") {
    return false;
  }
  if (payload.revoked === true) {
    return true;
  }
  if (payload.revoked && typeof payload.revoked === "object" && payload.revoked.revoked) {
    return true;
  }
  if (payload.data && typeof payload.data === "object") {
    return payloadContainsRevocation(payload.data);
  }
  return false;
}

function isAuthorizationFailure(error) {
  if (!error) {
    return false;
  }
  if (payloadContainsRevocation(error.payload)) {
    return true;
  }
  const message = String(error.message || "").trim().toLowerCase();
  if (!message) {
    return error.status === 401;
  }
  const authorizationTerms = [
    "valid license is required",
    "license is not active on this server",
    "license key is disabled",
    "license revoked",
    "trial license expired",
    "device is frozen",
    "未激活",
    "已禁用",
    "已撤销",
    "已过期",
    "被冻结",
  ];
  return error.status === 401 || authorizationTerms.some((term) => message.includes(term));
}

function activationPageUrl() {
  const url = new URL(window.location.href);
  url.hash = "";
  url.searchParams.set("activation", "1");
  url.searchParams.set("automatic", "1");
  url.searchParams.set("_", String(Date.now()));
  return url.toString();
}

function redirectToActivationPage() {
  if (state.activationRedirecting) {
    return;
  }
  state.activationRedirecting = true;
  document.title = "设备激活";
  document.body.classList.remove("license-loading");
  window.setTimeout(() => {
    window.location.replace(activationPageUrl());
  }, 0);
}

async function api(path, options = {}) {
  const client = window.ttbox && window.ttbox.api;
  if (!client || typeof client.request !== "function") throw new Error("TTBOX API Client 未加载");
  let body = options.body;
  if (typeof body === "string") {
    try { body = JSON.parse(body); } catch { body = undefined; }
  }
  const result = await client.request((options.method || "GET").toUpperCase(), path, body, { ...options, body: undefined });
  if (!result.ok) {
    const error = new Error(result.error || "请求失败");
    error.status = result.status;
    error.payload = result.data;
    throw error;
  }
  return result.data;
}

function setApplyStatus(mode, text) {
  const el = $("applyIndicator");
  if (!el) {
    return;
  }
  el.className = `sync-badge ${mode}`;
  el.textContent = text;
}

function setAimTraceStatus(text) {
  const el = $("recordAimTraceStatus");
  if (el) {
    el.textContent = text || "";
  }
}

function setUsbDiagnosticsStatus(text) {
  const el = $("usbDiagnosticsStatus");
  if (el) {
    el.textContent = text || "";
  }
}

function filenameFromContentDisposition(header, fallback) {
  const value = String(header || "");
  const utf8Match = value.match(/filename\*=UTF-8''([^;]+)/i);
  if (utf8Match) {
    try {
      return decodeURIComponent(utf8Match[1].trim().replace(/^"|"$/g, ""));
    } catch {
      return utf8Match[1].trim().replace(/^"|"$/g, "") || fallback;
    }
  }
  const plainMatch = value.match(/filename="?([^";]+)"?/i);
  return plainMatch ? plainMatch[1].trim() : fallback;
}

async function downloadUsbDiagnostics() {
  const button = $("downloadUsbDiagnosticsButton");
  try {
    if (button) {
      button.disabled = true;
    }
    setUsbDiagnosticsStatus("生成中...");
    const response = await ttbox.api.rawDownload("/api/diagnostics/usb-proxy.zip");
    if (!response.ok) {
      const text = await response.text().catch(() => "");
      throw new Error(text.slice(0, 200) || `HTTP ${response.status}`);
    }
    const blob = await response.blob();
    const filename = filenameFromContentDisposition(
      response.headers.get("Content-Disposition"),
      "usb-proxy-diagnostics.zip",
    );
    const url = URL.createObjectURL(blob);
    const link = document.createElement("a");
    link.href = url;
    link.download = filename;
    document.body.appendChild(link);
    link.click();
    link.remove();
    window.setTimeout(() => URL.revokeObjectURL(url), 1000);
    setUsbDiagnosticsStatus("已下载");
    showToast("USB诊断日志已生成");
  } catch (error) {
    setUsbDiagnosticsStatus("下载失败");
    throw error;
  } finally {
    if (button) {
      button.disabled = false;
    }
  }
}

function fillOptions(select, values, labels = HOTKEY_LABELS) {
  if (!select) {
    return;
  }
  const previous = select.value;
  select.innerHTML = "";
  values.forEach((value) => {
    const option = document.createElement("option");
    option.value = value;
    option.textContent = labels[value] || value;
    select.appendChild(option);
  });
  if (values.includes(previous)) {
    select.value = previous;
  }
}

function setCheckbox(id, value) {
  const el = $(id);
  if (el) {
    el.checked = !!value;
  }
}

function setRadioValue(name, value) {
  const targetValue = String(value || "");
  let matched = false;
  document.querySelectorAll(`input[type="radio"][name="${name}"]`).forEach((input) => {
    const checked = input.value === targetValue;
    input.checked = checked;
    matched = matched || checked;
  });
  if (!matched) {
    const fallback = document.querySelector(`input[type="radio"][name="${name}"]`);
    if (fallback) {
      fallback.checked = true;
    }
  }
}

function decimalPlacesFromStep(step) {
  if (!step || step === "any") {
    return null;
  }
  const numericStep = Number(step);
  if (!Number.isFinite(numericStep) || numericStep <= 0) {
    return null;
  }
  const normalized = numericStep.toFixed(10).replace(/0+$/, "");
  const dotIndex = normalized.indexOf(".");
  return dotIndex === -1 ? 0 : normalized.length - dotIndex - 1;
}

function trimFixedNumber(value) {
  return value.replace(/(\.\d*?)0+$/, "$1").replace(/\.$/, "");
}

function formatControlValue(el, value) {
  if (value === undefined || value === null || value === "") {
    return "";
  }
  if (!el || (el.type !== "number" && el.type !== "range")) {
    return value;
  }
  const numericValue = Number(value);
  if (!Number.isFinite(numericValue)) {
    return value;
  }
  const decimals = decimalPlacesFromStep(el.step);
  if (decimals === null) {
    return String(value);
  }
  if (decimals === 0) {
    return String(Math.round(numericValue));
  }
  return trimFixedNumber(numericValue.toFixed(decimals));
}

function setValue(id, value) {
  const el = $(id);
  if (el) {
    if (state.isPopulating && document.activeElement === el && el.matches("input, textarea, select")) {
      return;
    }
    el.value = formatControlValue(el, value);
  }
}

function getNumber(id, fallback = 0) {
  const el = $(id);
  const raw = el ? el.value : "";
  if (raw === "") {
    return fallback;
  }
  const value = Number(raw);
  return Number.isFinite(value) ? value : fallback;
}

function getNumberInRange(id, fallback = 0) {
  const limits = dynamicNumericRangeLimitsForId(id) || NUMERIC_RANGE_LIMITS[id];
  const value = getNumber(id, fallback);
  return limits ? clamp(value, limits[0], limits[1]) : value;
}

function clampNumberInputToLimits(input) {
  if (!input || !input.id || input.value === "") {
    return;
  }
  const limits = dynamicNumericRangeLimitsForId(input.id) || NUMERIC_RANGE_LIMITS[input.id];
  const value = Number(input.value);
  if (!limits || !Number.isFinite(value)) {
    return;
  }
  const clampedValue = clamp(value, limits[0], limits[1]);
  input.value = formatControlValue(input, clampedValue);
  if (input.id === "capture_crop_size") {
    syncCropSizePresetRange(clampedValue);
    return;
  }
  const rangeInput = $(`${input.id}_range`);
  if (rangeInput) {
    rangeInput.value = formatControlValue(rangeInput, clampedValue);
  }
}

function getString(id) {
  const el = $(id);
  return el ? el.value.trim() : "";
}

function printableAscii(value, maxLength) {
  return String(value || "")
    .replace(/[^\x20-\x7E]/g, "")
    .slice(0, maxLength);
}

function vendorCode(value) {
  return printableAscii(value, 8).toUpperCase().replace(/[^A-Z]/g, "").slice(0, 3);
}

function hexText(value, maxDigits) {
  const text = String(value || "").trim();
  const prefix = text.toLowerCase().startsWith("0x") ? "0x" : "";
  const digits = text.replace(/^0x/i, "").replace(/[^0-9a-fA-F]/g, "").slice(0, maxDigits);
  return `${prefix}${digits}`;
}

function normalizeHexValue(value, maxDigits) {
  const digits = String(value || "").trim().replace(/^0x/i, "");
  if (!new RegExp(`^[0-9a-fA-F]{1,${maxDigits}}$`).test(digits)) {
    return "";
  }
  const numeric = Number.parseInt(digits, 16);
  if (!Number.isFinite(numeric) || numeric <= 0) {
    return "";
  }
  return `0x${digits.toLowerCase().padStart(maxDigits, "0")}`;
}

function setValidation(containerId, fieldIds, messages) {
  fieldIds.forEach((id) => {
    const el = $(id);
    if (el) {
      el.classList.remove("is-invalid");
    }
  });
  const container = $(containerId);
  if (!container) {
    return;
  }
  if (messages.length === 0) {
    container.hidden = true;
    container.textContent = "";
    return;
  }
  messages.forEach(({ id }) => {
    const el = $(id);
    if (el) {
      el.classList.add("is-invalid");
    }
  });
  container.hidden = false;
  container.textContent = messages.map(({ text }) => text).join("\n");
}

function getCheckbox(id) {
  const el = $(id);
  return !!(el && el.checked);
}

function getRadioValue(name, fallback = "") {
  const checked = document.querySelector(`input[type="radio"][name="${name}"]:checked`);
  return checked ? checked.value : fallback;
}

function normalizeMouseOutputMode(mode) {
  if (mode === "kmboxnet") {
    return "kmboxnet";
  }
  if (mode === "catnet") {
    return "catnet";
  }
  if (mode === "makcu") {
    return "makcu";
  }
  if (mode === "ferrum") {
    return "ferrum";
  }
  if (mode === "kmboxb") {
    return "kmboxb";
  }
  return "passthrough";
}

function normalizeKmboxUuid(value) {
  return String(value || "").replace(/[^0-9a-fA-F]/g, "").slice(0, 8).toUpperCase();
}

function normalizeKmboxConfig(config = {}, mode = "passthrough") {
  const port = Number(config.port);
  const monitorPort = Number(config.monitor_port);
  const timeoutMs = Number(config.timeout_ms);
  const enabled = Boolean(config.enabled) || normalizeMouseOutputMode(mode) === "kmboxnet";
  return {
    enabled,
    ip: String(config.ip || "").trim(),
    port: Number.isFinite(port) ? Math.round(port) : KMBOXNET_DEFAULTS.port,
    uuid: normalizeKmboxUuid(config.uuid),
    monitor_port: Number.isFinite(monitorPort) ? Math.round(monitorPort) : KMBOXNET_DEFAULTS.monitor_port,
    encrypted: Boolean(config.encrypted),
    timeout_ms: Number.isFinite(timeoutMs) ? Math.round(timeoutMs) : KMBOXNET_DEFAULTS.timeout_ms,
  };
}

function normalizeCatnetConfig(config = {}, mode = "passthrough") {
  const port = Number(config.port);
  const monitorPort = Number(config.monitor_port);
  const timeoutMs = Number(config.timeout_ms);
  const enabled = Boolean(config.enabled) || normalizeMouseOutputMode(mode) === "catnet";
  return {
    enabled,
    ip: String(config.ip || CATNET_DEFAULTS.ip).trim(),
    port: Number.isFinite(port) ? Math.round(port) : CATNET_DEFAULTS.port,
    uuid: normalizeKmboxUuid(config.uuid),
    monitor_port: Number.isFinite(monitorPort) ? Math.round(monitorPort) : CATNET_DEFAULTS.monitor_port,
    timeout_ms: Number.isFinite(timeoutMs) ? Math.round(timeoutMs) : CATNET_DEFAULTS.timeout_ms,
  };
}

function normalizeMakcuPort(value) {
  let port = String(value || "").trim();
  if (!port || port.toLowerCase() === "auto") {
    return "auto";
  }
  if (port.startsWith("/dev/")) {
    port = port.slice(5);
  }
  return /^tty(?:USB|ACM)\d+$/.test(port) ? port : "auto";
}

function normalizeMakcuConfig(config = {}, mode = "passthrough") {
  const enabled = Boolean(config.enabled) || normalizeMouseOutputMode(mode) === "makcu";
  return {
    enabled,
    port: normalizeMakcuPort(config.port || MAKCU_DEFAULTS.port),
    high_speed: config.high_speed === undefined ? MAKCU_DEFAULTS.high_speed : Boolean(config.high_speed),
  };
}

function normalizeFerrumPort(value) {
  return normalizeMakcuPort(value);
}

function normalizeFerrumConfig(config = {}, mode = "passthrough") {
  const enabled = Boolean(config.enabled) || normalizeMouseOutputMode(mode) === "ferrum";
  return {
    enabled,
    port: normalizeFerrumPort(config.port || FERRUM_DEFAULTS.port),
  };
}

function makcuDeviceLabel(device) {
  if (!device || !device.port) {
    return "";
  }
  const parts = [device.path || `/dev/${device.port}`];
  const vidPid = [device.vid, device.pid].filter(Boolean).join(":");
  if (vidPid) {
    parts.push(vidPid);
  }
  if (device.product) {
    parts.push(device.product);
  }
  return parts.join(" · ");
}

function renderMakcuDeviceOptions(selectedPort = "auto") {
  const select = $("makcu_port");
  if (!select) {
    return;
  }
  const normalized = normalizeMakcuPort(selectedPort);
  select.innerHTML = "";
  const autoOption = document.createElement("option");
  autoOption.value = "auto";
  autoOption.textContent = "自动选择";
  select.appendChild(autoOption);
  (state.makcuDevices || []).forEach((device) => {
    if (!device || !device.port) {
      return;
    }
    const option = document.createElement("option");
    option.value = normalizeMakcuPort(device.port);
    option.textContent = makcuDeviceLabel(device);
    if (!device.is_makcu) {
      option.textContent += " · 非自动候选";
    }
    select.appendChild(option);
  });
  const hasSelected = Array.from(select.options).some((option) => option.value === normalized);
  if (!hasSelected && normalized !== "auto") {
    const option = document.createElement("option");
    option.value = normalized;
    option.textContent = `/dev/${normalized} · 已保存`;
    select.appendChild(option);
  }
  select.value = hasSelected ? normalized : (normalized === "auto" ? "auto" : normalized);
}

function populateMakcuConfig(config = {}, mode = "passthrough") {
  const makcu = normalizeMakcuConfig(config, mode);
  setCheckbox("makcu_enabled", makcu.enabled);
  setCheckbox("makcu_high_speed", makcu.high_speed);
  renderMakcuDeviceOptions(makcu.port);
  updateMakcuFormUi(makcu);
}

function collectMakcuConfig() {
  return normalizeMakcuConfig({
    enabled: getCheckbox("makcu_enabled"),
    port: getString("makcu_port") || "auto",
    high_speed: getCheckbox("makcu_high_speed"),
  });
}

function setExternalMouseCardExpanded(cardSelector, bodySelector, enabled) {
  const card = document.querySelector(cardSelector);
  const body = document.querySelector(bodySelector);
  if (card) {
    card.classList.toggle("is-collapsed", !enabled);
  }
  if (body) {
    body.hidden = !enabled;
  }
}

function updateMakcuFormUi(makcu = collectMakcuConfig()) {
  const saveButton = $("saveMakcuButton");
  if (saveButton) {
    saveButton.textContent = makcu.enabled ? "保存并启用" : "保存配置";
  }
  setExternalMouseCardExpanded("[data-makcu-card]", "[data-makcu-body]", makcu.enabled);
}

function validateMakcuConfig() {
  const fieldIds = ["makcu_port"];
  const config = collectMakcuConfig();
  const messages = [];
  if (config.enabled && config.port !== "auto" && !/^tty(?:USB|ACM)\d+$/.test(config.port)) {
    messages.push({ id: "makcu_port", text: "MAKCU 串口必须是自动选择、ttyUSB* 或 ttyACM*。" });
  }
  setValidation("makcuValidation", fieldIds, messages);
  if (messages.length > 0) {
    throw new Error("MAKCU 配置不符合规范");
  }
  renderMakcuDeviceOptions(config.port);
  updateMakcuFormUi(config);
  return config;
}

async function refreshMakcuDevices({ silent = false } = {}) {
  try {
    const result = await api("/api/makcu/devices");
    state.makcuDevices = Array.isArray(result.devices) ? result.devices : [];
    renderMakcuDeviceOptions(collectMakcuConfig().port);
    if (!silent) {
      showToast("MAKCU 串口列表已刷新");
    }
  } catch (error) {
    if (!silent) {
      throw error;
    }
  }
}

function ferrumDeviceLabel(device) {
  if (!device || !device.port) {
    return "";
  }
  const parts = [device.path || `/dev/${device.port}`];
  const vidPid = [device.vid, device.pid].filter(Boolean).join(":");
  if (vidPid) {
    parts.push(vidPid);
  }
  if (device.product) {
    parts.push(device.product);
  }
  if (device.manufacturer) {
    parts.push(device.manufacturer);
  }
  return parts.join(" · ");
}

function renderFerrumDeviceOptions(selectedPort = "auto") {
  const select = $("ferrum_port");
  if (!select) {
    return;
  }
  const normalized = normalizeFerrumPort(selectedPort);
  select.innerHTML = "";
  const autoOption = document.createElement("option");
  autoOption.value = "auto";
  autoOption.textContent = "自动选择";
  select.appendChild(autoOption);
  (state.ferrumDevices || []).forEach((device) => {
    if (!device || !device.port) {
      return;
    }
    const option = document.createElement("option");
    option.value = normalizeFerrumPort(device.port);
    option.textContent = ferrumDeviceLabel(device);
    if (!device.is_ferrum) {
      option.textContent += " · 非 CP210x 候选";
    }
    select.appendChild(option);
  });
  const hasSelected = Array.from(select.options).some((option) => option.value === normalized);
  if (!hasSelected && normalized !== "auto") {
    const option = document.createElement("option");
    option.value = normalized;
    option.textContent = `/dev/${normalized} · 已保存`;
    select.appendChild(option);
  }
  select.value = hasSelected ? normalized : (normalized === "auto" ? "auto" : normalized);
}

function populateFerrumConfig(config = {}, mode = "passthrough") {
  const ferrum = normalizeFerrumConfig(config, mode);
  setCheckbox("ferrum_enabled", ferrum.enabled);
  renderFerrumDeviceOptions(ferrum.port);
  updateFerrumFormUi(ferrum);
}

function collectFerrumConfig() {
  return normalizeFerrumConfig({
    enabled: getCheckbox("ferrum_enabled"),
    port: getString("ferrum_port") || "auto",
  });
}

function updateFerrumFormUi(ferrum = collectFerrumConfig()) {
  const saveButton = $("saveFerrumButton");
  if (saveButton) {
    saveButton.textContent = ferrum.enabled ? "保存并启用" : "保存配置";
  }
  setExternalMouseCardExpanded("[data-ferrum-card]", "[data-ferrum-body]", ferrum.enabled);
}

function validateFerrumConfig() {
  const fieldIds = ["ferrum_port"];
  const config = collectFerrumConfig();
  const messages = [];
  if (config.enabled && config.port !== "auto" && !/^tty(?:USB|ACM)\d+$/.test(config.port)) {
    messages.push({ id: "ferrum_port", text: "Ferrum 串口必须是自动选择、ttyUSB* 或 ttyACM*。" });
  }
  setValidation("ferrumValidation", fieldIds, messages);
  if (messages.length > 0) {
    throw new Error("Ferrum 配置不符合规范");
  }
  renderFerrumDeviceOptions(config.port);
  updateFerrumFormUi(config);
  return config;
}

async function refreshFerrumDevices({ silent = false } = {}) {
  try {
    const result = await api("/api/ferrum/devices");
    state.ferrumDevices = Array.isArray(result.devices) ? result.devices : [];
    renderFerrumDeviceOptions(collectFerrumConfig().port);
    if (!silent) {
      showToast("Ferrum 串口列表已刷新");
    }
  } catch (error) {
    if (!silent) {
      throw error;
    }
  }
}

function normalizeKmboxbPort(value) {
  return normalizeMakcuPort(value);
}

function normalizeKmboxbConfig(config = {}, mode = "passthrough") {
  const enabled = Boolean(config.enabled) || normalizeMouseOutputMode(mode) === "kmboxb";
  return {
    enabled,
    port: normalizeKmboxbPort(config.port || KMBOXB_DEFAULTS.port),
  };
}

function kmboxbDeviceLabel(device) {
  if (!device || !device.port) {
    return "";
  }
  const parts = [device.path || `/dev/${device.port}`];
  const vidPid = [device.vid, device.pid].filter(Boolean).join(":");
  if (vidPid) {
    parts.push(vidPid);
  }
  if (device.product) {
    parts.push(device.product);
  }
  if (device.manufacturer) {
    parts.push(device.manufacturer);
  }
  return parts.join(" · ");
}

function renderKmboxbDeviceOptions(selectedPort = "auto") {
  const select = $("kmboxb_port");
  if (!select) {
    return;
  }
  const normalized = normalizeKmboxbPort(selectedPort);
  select.innerHTML = "";
  const autoOption = document.createElement("option");
  autoOption.value = "auto";
  autoOption.textContent = "自动识别 B+";
  select.appendChild(autoOption);
  (state.kmboxbDevices || []).forEach((device) => {
    if (!device || !device.port) {
      return;
    }
    const option = document.createElement("option");
    option.value = normalizeKmboxbPort(device.port);
    option.textContent = kmboxbDeviceLabel(device);
    if (!device.is_kmboxb_candidate) {
      option.textContent += " · 非 CH34x 自动候选";
    }
    select.appendChild(option);
  });
  const hasSelected = Array.from(select.options).some((option) => option.value === normalized);
  if (!hasSelected && normalized !== "auto") {
    const option = document.createElement("option");
    option.value = normalized;
    option.textContent = `/dev/${normalized} · 已保存`;
    select.appendChild(option);
  }
  select.value = normalized;
}

function populateKmboxbConfig(config = {}, mode = "passthrough") {
  const kmboxb = normalizeKmboxbConfig(config, mode);
  setCheckbox("kmboxb_enabled", kmboxb.enabled);
  renderKmboxbDeviceOptions(kmboxb.port);
  updateKmboxbFormUi(kmboxb);
}

function collectKmboxbConfig() {
  return normalizeKmboxbConfig({
    enabled: getCheckbox("kmboxb_enabled"),
    port: getString("kmboxb_port") || "auto",
  });
}

function updateKmboxbFormUi(kmboxb = collectKmboxbConfig()) {
  const saveButton = $("saveKmboxbButton");
  if (saveButton) {
    saveButton.textContent = kmboxb.enabled ? "保存并启用" : "保存配置";
  }
  setExternalMouseCardExpanded("[data-kmboxb-card]", "[data-kmboxb-body]", kmboxb.enabled);
}

function validateKmboxbConfig() {
  const fieldIds = ["kmboxb_port"];
  const config = collectKmboxbConfig();
  const messages = [];
  if (config.enabled && config.port !== "auto" && !/^tty(?:USB|ACM)\d+$/.test(config.port)) {
    messages.push({ id: "kmboxb_port", text: "kmbox B+ 串口必须是自动选择、ttyUSB* 或 ttyACM*。" });
  }
  setValidation("kmboxbValidation", fieldIds, messages);
  if (messages.length > 0) {
    throw new Error("kmbox B+ 配置不符合规范");
  }
  renderKmboxbDeviceOptions(config.port);
  updateKmboxbFormUi(config);
  return config;
}

async function refreshKmboxbDevices({ silent = false } = {}) {
  try {
    const result = await api("/api/kmboxb/devices");
    state.kmboxbDevices = Array.isArray(result.devices) ? result.devices : [];
    renderKmboxbDeviceOptions(collectKmboxbConfig().port);
    if (!silent) {
      showToast("kmbox B+ 串口列表已刷新");
    }
  } catch (error) {
    if (!silent) {
      throw error;
    }
  }
}

function populateKmboxConfig(config = {}, mode = "passthrough") {
  const kmbox = normalizeKmboxConfig(config, mode);
  setCheckbox("kmbox_enabled", kmbox.enabled);
  setValue("kmbox_ip", kmbox.ip);
  setValue("kmbox_port", kmbox.port || "");
  setValue("kmbox_uuid", kmbox.uuid);
  setValue("kmbox_monitor_port", kmbox.monitor_port);
  setValue("kmbox_timeout_ms", kmbox.timeout_ms);
  setCheckbox("kmbox_encrypted", kmbox.encrypted);
  updateKmboxFormUi(kmbox);
}

function collectKmboxConfig() {
  return normalizeKmboxConfig({
    enabled: getCheckbox("kmbox_enabled"),
    ip: getString("kmbox_ip"),
    port: getNumber("kmbox_port", 0),
    uuid: normalizeKmboxUuid(getString("kmbox_uuid")),
    monitor_port: getNumber("kmbox_monitor_port", KMBOXNET_DEFAULTS.monitor_port),
    encrypted: getCheckbox("kmbox_encrypted"),
    timeout_ms: getNumber("kmbox_timeout_ms", KMBOXNET_DEFAULTS.timeout_ms),
  });
}

function updateKmboxFormUi(kmbox = collectKmboxConfig()) {
  const saveButton = $("saveKmboxButton");
  if (saveButton) {
    saveButton.textContent = kmbox.enabled ? "保存并启用" : "保存配置";
  }
  setExternalMouseCardExpanded("[data-kmbox-card]", "[data-kmbox-body]", kmbox.enabled);
}

function validateKmboxConfig() {
  const fieldIds = [
    "kmbox_ip",
    "kmbox_port",
    "kmbox_uuid",
    "kmbox_monitor_port",
    "kmbox_timeout_ms",
  ];
  const config = collectKmboxConfig();
  const messages = [];
  if (config.enabled) {
    if (!/^(\d{1,3}\.){3}\d{1,3}$/.test(config.ip) ||
        config.ip.split(".").some((part) => Number(part) > 255)) {
      messages.push({ id: "kmbox_ip", text: "盒子 IP 必须是 IPv4 地址，例如 192.168.2.188。" });
    }
    if (config.port < 1 || config.port > 65535) {
      messages.push({ id: "kmbox_port", text: "通信端口必须在 1-65535 之间。" });
    }
    if (!/^[0-9A-F]{8}$/.test(config.uuid)) {
      messages.push({ id: "kmbox_uuid", text: "UUID 必须是 8 位十六进制字符。" });
    }
    if (config.monitor_port !== 0 && (config.monitor_port < 1024 || config.monitor_port > 65535)) {
      messages.push({ id: "kmbox_monitor_port", text: "监听端口必须为 0，或 1024-65535 之间的本地 UDP 端口。" });
    }
    if (config.timeout_ms < 50 || config.timeout_ms > 3000) {
      messages.push({ id: "kmbox_timeout_ms", text: "响应超时必须在 50-3000 ms 之间。" });
    }
  }
  setValidation("kmboxValidation", fieldIds, messages);
  if (messages.length > 0) {
    throw new Error("键鼠盒子配置不符合规范");
  }
  setValue("kmbox_uuid", config.uuid);
  setValue("kmbox_port", config.port || "");
  setValue("kmbox_monitor_port", config.monitor_port);
  setValue("kmbox_timeout_ms", config.timeout_ms);
  updateKmboxFormUi(config);
  return config;
}

function populateCatnetConfig(config = {}, mode = "passthrough") {
  const catnet = normalizeCatnetConfig(config, mode);
  setCheckbox("catnet_enabled", catnet.enabled);
  setValue("catnet_ip", catnet.ip);
  setValue("catnet_port", catnet.port || "");
  setValue("catnet_uuid", catnet.uuid);
  setValue("catnet_monitor_port", catnet.monitor_port);
  setValue("catnet_timeout_ms", catnet.timeout_ms);
  updateCatnetFormUi(catnet);
}

function collectCatnetConfig() {
  return normalizeCatnetConfig({
    enabled: getCheckbox("catnet_enabled"),
    ip: getString("catnet_ip"),
    port: getNumber("catnet_port", CATNET_DEFAULTS.port),
    uuid: normalizeKmboxUuid(getString("catnet_uuid")),
    monitor_port: getNumber("catnet_monitor_port", CATNET_DEFAULTS.monitor_port),
    timeout_ms: getNumber("catnet_timeout_ms", CATNET_DEFAULTS.timeout_ms),
  });
}

function updateCatnetFormUi(catnet = collectCatnetConfig()) {
  const saveButton = $("saveCatnetButton");
  if (saveButton) {
    saveButton.textContent = catnet.enabled ? "保存并启用" : "保存配置";
  }
  setExternalMouseCardExpanded("[data-catnet-card]", "[data-catnet-body]", catnet.enabled);
}

function validateCatnetConfig() {
  const fieldIds = [
    "catnet_ip",
    "catnet_port",
    "catnet_uuid",
    "catnet_monitor_port",
    "catnet_timeout_ms",
  ];
  const config = collectCatnetConfig();
  const messages = [];
  if (config.enabled) {
    if (!/^(\d{1,3}\.){3}\d{1,3}$/.test(config.ip) ||
        config.ip.split(".").some((part) => Number(part) > 255)) {
      messages.push({ id: "catnet_ip", text: "CatNet 盒子 IP 必须是 IPv4 地址，例如 192.168.7.1。" });
    }
    if (config.port < 1 || config.port > 65535) {
      messages.push({ id: "catnet_port", text: "通信端口必须在 1-65535 之间。" });
    }
    if (!/^[0-9A-F]{8}$/.test(config.uuid)) {
      messages.push({ id: "catnet_uuid", text: "UUID 必须是 8 位十六进制字符。" });
    }
    if (config.monitor_port !== 0 && (config.monitor_port < 1024 || config.monitor_port > 65535)) {
      messages.push({ id: "catnet_monitor_port", text: "监听端口必须为 0，或 1024-65535 之间的本地 UDP 端口。" });
    }
    if (config.timeout_ms < 50 || config.timeout_ms > 3000) {
      messages.push({ id: "catnet_timeout_ms", text: "响应超时必须在 50-3000 ms 之间。" });
    }
  }
  setValidation("catnetValidation", fieldIds, messages);
  if (messages.length > 0) {
    throw new Error("CatNet 配置不符合规范");
  }
  setValue("catnet_uuid", config.uuid);
  setValue("catnet_port", config.port || "");
  setValue("catnet_monitor_port", config.monitor_port);
  setValue("catnet_timeout_ms", config.timeout_ms);
  updateCatnetFormUi(config);
  return config;
}

function renderKmboxStatus(mouseOutput = {}) {
  const config = state.config && state.config.mouse_output ? state.config.mouse_output : {};
  const kmboxConfig = normalizeKmboxConfig(config.kmboxnet || {}, config.mode);
  const runtimeMode = normalizeMouseOutputMode(mouseOutput.mode || config.mode);
  const enabled = kmboxConfig.enabled || runtimeMode === "kmboxnet";
  const connected = runtimeMode === "kmboxnet" && !!mouseOutput.connected;
  const lastError = runtimeMode === "kmboxnet" ? String(mouseOutput.last_error || "") : "";
  const status = $("kmboxStatus");
  if (status) {
    if (!enabled) {
      setHardwareStatus("kmboxStatus", "未启用", false);
    } else if (connected) {
      setHardwareStatus("kmboxStatus", "已连接", true);
    } else if (lastError) {
      setHardwareStatus("kmboxStatus", "连接失败", false);
    } else {
      setHardwareStatus("kmboxStatus", "等待连接", false);
    }
  }
  const hint = $("kmboxRuntimeHint");
  if (hint) {
    if (!enabled) {
      hint.textContent = "保存后会自动尝试连接。";
    } else if (connected) {
      hint.textContent = "kmboxNet 已接管鼠标监听与控制。";
    } else if (lastError) {
      hint.textContent = `连接错误：${lastError}`;
    } else {
      hint.textContent = "已启用 kmboxNet，等待系统运行后自动连接。";
    }
  }
}

function renderCatnetStatus(mouseOutput = {}) {
  const config = state.config && state.config.mouse_output ? state.config.mouse_output : {};
  const catnetConfig = normalizeCatnetConfig(config.catnet || {}, config.mode);
  const runtimeMode = normalizeMouseOutputMode(mouseOutput.mode || config.mode);
  const enabled = catnetConfig.enabled || runtimeMode === "catnet";
  const connected = runtimeMode === "catnet" && !!mouseOutput.connected;
  const lastError = runtimeMode === "catnet" ? String(mouseOutput.last_error || "") : "";
  if (!enabled) {
    setHardwareStatus("catnetStatus", "未启用", false);
  } else if (connected) {
    setHardwareStatus("catnetStatus", "已连接", true);
  } else if (lastError) {
    setHardwareStatus("catnetStatus", "连接失败", false);
  } else {
    setHardwareStatus("catnetStatus", "等待连接", false);
  }
  const hint = $("catnetRuntimeHint");
  if (hint) {
    if (!enabled) {
      hint.textContent = "保存后会自动尝试连接。";
    } else if (connected) {
      hint.textContent = "CatNet 已接管鼠标监听与控制。";
    } else if (lastError) {
      hint.textContent = `连接错误：${lastError}`;
    } else {
      hint.textContent = "已启用 CatNet，等待系统运行后自动连接。";
    }
  }
}

function renderMakcuStatus(mouseOutput = {}) {
  const config = state.config && state.config.mouse_output ? state.config.mouse_output : {};
  const makcuConfig = normalizeMakcuConfig(config.makcu || {}, config.mode);
  const runtimeMode = normalizeMouseOutputMode(mouseOutput.mode || config.mode);
  const enabled = makcuConfig.enabled || runtimeMode === "makcu";
  const connected = runtimeMode === "makcu" && !!mouseOutput.connected;
  const lastError = runtimeMode === "makcu" ? String(mouseOutput.last_error || "") : "";
  if (!enabled) {
    setHardwareStatus("makcuStatus", "未启用", false);
  } else if (connected) {
    setHardwareStatus("makcuStatus", "已连接", true);
  } else if (lastError) {
    setHardwareStatus("makcuStatus", "连接失败", false);
  } else {
    setHardwareStatus("makcuStatus", "等待连接", false);
  }
  const hint = $("makcuRuntimeHint");
  if (hint) {
    if (!enabled) {
      hint.textContent = "保存后会自动尝试连接。";
    } else if (connected) {
      hint.textContent = "MAKCU 已接管鼠标监听与控制。";
    } else if (lastError) {
      hint.textContent = `连接错误：${lastError}`;
    } else {
      hint.textContent = "已启用 MAKCU，等待系统运行后自动连接。";
    }
  }
}

function renderFerrumStatus(mouseOutput = {}) {
  const config = state.config && state.config.mouse_output ? state.config.mouse_output : {};
  const ferrumConfig = normalizeFerrumConfig(config.ferrum || {}, config.mode);
  const runtimeMode = normalizeMouseOutputMode(mouseOutput.mode || config.mode);
  const enabled = ferrumConfig.enabled || runtimeMode === "ferrum";
  const connected = runtimeMode === "ferrum" && !!mouseOutput.connected;
  const lastError = runtimeMode === "ferrum" ? String(mouseOutput.last_error || "") : "";
  if (!enabled) {
    setHardwareStatus("ferrumStatus", "未启用", false);
  } else if (connected) {
    setHardwareStatus("ferrumStatus", "已连接", true);
  } else if (lastError) {
    setHardwareStatus("ferrumStatus", "连接失败", false);
  } else {
    setHardwareStatus("ferrumStatus", "等待连接", false);
  }
  const hint = $("ferrumRuntimeHint");
  if (hint) {
    if (!enabled) {
      hint.textContent = "保存后会自动尝试连接。";
    } else if (connected) {
      hint.textContent = "Ferrum Legacy 已接管鼠标监听与控制。";
    } else if (lastError) {
      hint.textContent = `连接错误：${lastError}`;
    } else {
      hint.textContent = "已启用 Ferrum Legacy，等待系统运行后自动连接。";
    }
  }
}

function renderKmboxbStatus(mouseOutput = {}) {
  const config = state.config && state.config.mouse_output ? state.config.mouse_output : {};
  const kmboxbConfig = normalizeKmboxbConfig(config.kmboxb || {}, config.mode);
  const runtimeMode = normalizeMouseOutputMode(mouseOutput.mode || config.mode);
  const enabled = kmboxbConfig.enabled || runtimeMode === "kmboxb";
  const connected = runtimeMode === "kmboxb" && !!mouseOutput.connected;
  const lastError = runtimeMode === "kmboxb" ? String(mouseOutput.last_error || "") : "";
  if (!enabled) {
    setHardwareStatus("kmboxbStatus", "未启用", false);
  } else if (connected) {
    setHardwareStatus("kmboxbStatus", "已连接", true);
  } else if (lastError) {
    setHardwareStatus("kmboxbStatus", "连接失败", false);
  } else {
    setHardwareStatus("kmboxbStatus", "等待连接", false);
  }

  const hint = $("kmboxbRuntimeHint");
  if (!hint) {
    return;
  }
  if (!enabled) {
    hint.textContent = "固定使用官方默认 115200 波特率。";
  } else if (connected) {
    const inputModeLabels = {
      "callback+poll": "回调+轮询",
      polling: "轮询",
      unavailable: "按键读取不可用",
    };
    const port = String(mouseOutput.serial_port || "未知端口");
    const version = String(mouseOutput.device_version || "未知版本");
    const inputMode = inputModeLabels[String(mouseOutput.physical_input_mode || "")] || "按键读取状态未知";
    hint.textContent = `端口：${port} · 版本：${version} · 物理按键：${inputMode}`;
  } else if (lastError) {
    hint.textContent = `连接错误：${lastError}`;
  } else {
    hint.textContent = "已启用 kmbox B+，等待系统自动识别并连接。";
  }
}

async function testMouseOutputCircle() {
  const button = $("testMouseCircleButton");
  if (button) {
    button.disabled = true;
    button.textContent = "发送中";
  }
  try {
    const result = await api("/api/mouse-output/test-circle", { method: "POST" });
    if (result && result.state) {
      renderRuntime({ ...(state.data || {}), state: result.state });
    }
    showToast("测试画圈已发送");
  } finally {
    if (button) {
      button.disabled = false;
      button.textContent = "测试画圈";
    }
  }
}

function clamp(value, min, max) {
  return Math.min(max, Math.max(min, value));
}

const RANGE_THUMB_SIZE_PX = 20;
const RANGE_THUMB_HIT_SLOP_PX = 14;

function numericAttr(el, attrName, fallback) {
  const value = Number(el.getAttribute(attrName));
  return Number.isFinite(value) ? value : fallback;
}

function rangeEventPoint(event) {
  if (event.touches && event.touches.length > 0) {
    return { x: event.touches[0].clientX, y: event.touches[0].clientY };
  }
  if (event.changedTouches && event.changedTouches.length > 0) {
    return { x: event.changedTouches[0].clientX, y: event.changedTouches[0].clientY };
  }
  if (Number.isFinite(event.clientX) && Number.isFinite(event.clientY)) {
    return { x: event.clientX, y: event.clientY };
  }
  return null;
}

function rangeThumbCenterX(rangeInput) {
  const rect = rangeInput.getBoundingClientRect();
  const min = numericAttr(rangeInput, "min", 0);
  const max = numericAttr(rangeInput, "max", 100);
  const value = clamp(Number(rangeInput.value), min, max);
  const span = max - min;
  const rawRatio = span > 0 ? (value - min) / span : 0;
  const ratio = getComputedStyle(rangeInput).direction === "rtl" ? 1 - rawRatio : rawRatio;
  const thumbSize = Math.min(RANGE_THUMB_SIZE_PX, Math.max(0, rect.width));
  const travel = Math.max(0, rect.width - thumbSize);
  return rect.left + thumbSize / 2 + travel * ratio;
}

function isRangeThumbHit(rangeInput, event) {
  const point = rangeEventPoint(event);
  if (!point) {
    return true;
  }

  const rect = rangeInput.getBoundingClientRect();
  if (rect.width <= 0 || rect.height <= 0) {
    return true;
  }

  const centerX = rangeThumbCenterX(rangeInput);
  const centerY = rect.top + rect.height / 2;
  const hitRadius = RANGE_THUMB_SIZE_PX / 2 + RANGE_THUMB_HIT_SLOP_PX;
  return Math.abs(point.x - centerX) <= hitRadius &&
    Math.abs(point.y - centerY) <= rect.height / 2 + RANGE_THUMB_HIT_SLOP_PX;
}

function shouldUseThumbOnlyRange(event) {
  const isCoarsePointer = window.matchMedia && window.matchMedia("(pointer: coarse)").matches;
  if (event.type === "touchstart") {
    return true;
  }
  if (event.pointerType) {
    return event.pointerType !== "mouse" || isCoarsePointer;
  }
  return isCoarsePointer;
}

function stopRangeTrackEvent(event) {
  event.preventDefault();
  if (typeof event.stopImmediatePropagation === "function") {
    event.stopImmediatePropagation();
  } else {
    event.stopPropagation();
  }
}

function clearRangeTrackBlock(rangeInput) {
  delete rangeInput.dataset.blockedRangeValue;
  delete rangeInput.dataset.blockRangeUntil;
}

function hasActiveRangeTrackBlock(rangeInput) {
  const blockUntil = Number(rangeInput.dataset.blockRangeUntil || 0);
  return blockUntil > 0 && Date.now() <= blockUntil;
}

function blockRangeTrackInteraction(rangeInput, event) {
  const blockUntil = Date.now() + 700;
  rangeInput.dataset.blockedRangeValue = rangeInput.value;
  rangeInput.dataset.blockRangeUntil = String(blockUntil);
  stopRangeTrackEvent(event);
  window.setTimeout(() => {
    if (Number(rangeInput.dataset.blockRangeUntil || 0) === blockUntil) {
      clearRangeTrackBlock(rangeInput);
    }
  }, 760);
}

function restoreBlockedRangeValue(rangeInput, event) {
  if (!hasActiveRangeTrackBlock(rangeInput)) {
    return;
  }
  if (rangeInput.dataset.blockedRangeValue !== undefined) {
    rangeInput.value = rangeInput.dataset.blockedRangeValue;
  }
  stopRangeTrackEvent(event);
}

function enableThumbOnlyRangeInput(rangeInput) {
  if (!rangeInput || rangeInput.type !== "range" || rangeInput.dataset.thumbOnlyBound === "1") {
    return;
  }
  rangeInput.dataset.thumbOnlyBound = "1";

  const handleStart = (event) => {
    if (rangeInput.disabled) {
      return;
    }
    if (event.type === "mousedown" && event.button !== 0) {
      return;
    }
    if (!shouldUseThumbOnlyRange(event)) {
      return;
    }
    if (!isRangeThumbHit(rangeInput, event)) {
      blockRangeTrackInteraction(rangeInput, event);
      return;
    }
    clearRangeTrackBlock(rangeInput);
  };

  rangeInput.addEventListener("pointerdown", handleStart, { capture: true });
  rangeInput.addEventListener("mousedown", handleStart, { capture: true });
  rangeInput.addEventListener("touchstart", handleStart, { capture: true, passive: false });
  rangeInput.addEventListener("input", (event) => restoreBlockedRangeValue(rangeInput, event), true);
  rangeInput.addEventListener("change", (event) => restoreBlockedRangeValue(rangeInput, event), true);
  rangeInput.addEventListener("click", (event) => restoreBlockedRangeValue(rangeInput, event), true);
}

function enableThumbOnlyRangeInputs(root = document) {
  root.querySelectorAll("input[type='range']").forEach(enableThumbOnlyRangeInput);
}

function formatNumber(value, digits = 2) {
  const number = Number(value);
  return Number.isFinite(number) ? number.toFixed(digits) : (0).toFixed(digits);
}

function positiveNumber(value) {
  const number = Number(value);
  return Number.isFinite(number) && number > 0 ? number : null;
}

function formatMaybe(value, fallback = "-") {
  return value === undefined || value === null || value === "" ? fallback : String(value);
}

function formatPercent(value) {
  const number = Number(value);
  return Number.isFinite(number) ? `${number.toFixed(1)}%` : "--";
}

function formatBytes(value) {
  const number = Number(value);
  if (!Number.isFinite(number) || number < 0) {
    return "--";
  }
  const units = ["B", "KB", "MB", "GB", "TB"];
  let scaled = number;
  let unitIndex = 0;
  while (scaled >= 1024 && unitIndex < units.length - 1) {
    scaled /= 1024;
    unitIndex += 1;
  }
  const digits = unitIndex <= 1 ? 0 : 1;
  return `${scaled.toFixed(digits)} ${units[unitIndex]}`;
}

function formatDuration(seconds) {
  const totalSeconds = Number(seconds);
  if (!Number.isFinite(totalSeconds) || totalSeconds < 0) {
    return "--";
  }
  const days = Math.floor(totalSeconds / 86400);
  const hours = Math.floor((totalSeconds % 86400) / 3600);
  const minutes = Math.floor((totalSeconds % 3600) / 60);
  if (days > 0) {
    return `${days} 天 ${hours} 时`;
  }
  if (hours > 0) {
    return `${hours} 时 ${minutes} 分`;
  }
  return `${minutes} 分`;
}

function formatStatus(status, running) {
  if (status === "reconnecting") {
    return STATUS_LABELS.reconnecting;
  }
  if (running) {
    return "运行中";
  }
  return STATUS_LABELS[status] || "已停止";
}

function formatPreprocessBackend(latencyState) {
  const backend = latencyState && latencyState.raw_preprocess_backend;
  if (backend === "rga") {
    return `RGA ${formatNumber(latencyState.rga_ms)} ms`;
  }
  if (backend === "cpu_direct") {
    return `CPU直拷 ${formatNumber(latencyState.rga_ms)} ms`;
  }
  if (backend === "cpu_fallback") {
    return `CPU回退 ${formatNumber(latencyState.detect_total_ms)} ms`;
  }
  if (backend === "failed") {
    return "硬件预处理失败";
  }
  return "-";
}

function formatHailoTemperature(hailoTemperature) {
  const info = hailoTemperature || {};
  const maxTemp = Number(info.max_celsius);
  const ts0 = Number(info.ts0_celsius);
  const ts1 = Number(info.ts1_celsius);
  if (!info.available || !Number.isFinite(maxTemp) || maxTemp <= 0) {
    return { value: "--", detail: info.last_error || "未采样" };
  }
  const detail = Number.isFinite(ts0) && Number.isFinite(ts1)
    ? `TS0 ${ts0.toFixed(1)} °C / TS1 ${ts1.toFixed(1)} °C`
    : "Hailo-8";
  return { value: `${maxTemp.toFixed(1)} °C`, detail };
}

function modelProfileLabel(value) {
  if (!value || value === "generic") {
    return "通用";
  }
  return value.replace(/_/g, " ").toUpperCase();
}

function modelFileName(model) {
  const value = (model && (model.display_name || model.file_name || model.id)) || "";
  const parts = String(value).split(/[\\/]/);
  return parts[parts.length - 1] || value || "未命名模型";
}

function modelDimension(model) {
  const width = Number(model && model.input_width);
  const height = Number(model && model.input_height);
  if (Number.isFinite(width) && Number.isFinite(height) && width > 0 && height > 0) {
    return `${width}x${height}`;
  }

  const source = [
    model && model.id,
    model && model.display_name,
    model && model.file_name,
  ].filter(Boolean).join(" ");
  const pair = source.match(/(?:^|[_\-\s])(\d{3,4})\s*[xX×]\s*(\d{3,4})(?=$|[_\-\s.])/);
  if (pair) {
    return `${pair[1]}x${pair[2]}`;
  }

  const candidates = Array.from(source.matchAll(/(?:^|[_\-\s])(\d{3,4})(?=$|[_\-\s.])/g))
    .map((match) => Number(match[1]))
    .filter((value) => Number.isFinite(value) && value >= 128 && value <= 2048);
  if (candidates.length > 0) {
    return `${candidates[0]}x${candidates[0]}`;
  }

  return "尺寸待检测";
}

function modelInputCropSize(model) {
  const width = Number(model && model.input_width);
  const height = Number(model && model.input_height);
  if (Number.isFinite(width) && Number.isFinite(height) && width > 0 && height > 0) {
    return normalizeCropSize(Math.max(width, height));
  }

  const dimension = modelDimension(model);
  const square = String(dimension).match(/^(\d{3,4})x\1$/);
  return square ? normalizeCropSize(Number(square[1])) : null;
}

function modelOutputLabel(model) {
  const outputCount = Number(model && model.output_count);
  const classCount = Number(model && model.class_count);
  const parts = [];
  if (Number.isFinite(outputCount) && outputCount > 0) {
    parts.push(`${outputCount} 路输出`);
  }
  if (Number.isFinite(classCount) && classCount > 0) {
    parts.push(`${classCount} 类`);
  }
  return parts.length > 0 ? parts.join(" / ") : "输出待检测";
}

function modelBackend(model) {
  const backend = String((model && model.backend) || "").trim().toLowerCase();
  if (backend === "remote") {
    return "remote";
  }
  if (backend === "cloud_encrypted" || backend === "cloud-encrypted" || backend === "aimatrix") {
    return "cloud_encrypted";
  }
  if (backend === "hailo" || backend === "hef") {
    return "hailo";
  }
  const fileName = String((model && model.file_name) || "").toLowerCase();
  return fileName.endsWith(".hef") || fileName.endsWith(".hef.enc") ? "hailo" : "rknn";
}

function modelBackendLabel(model) {
  const backend = modelBackend(model);
  if (backend === "remote") {
    return "远端";
  }
  if (backend === "cloud_encrypted") {
    return "云加密";
  }
  return backend === "hailo" ? "Hailo" : "RKNN";
}

function modelHailoPipelineDepth(model) {
  const depth = Number(model && model.hailo_pipeline_depth);
  if (!Number.isFinite(depth) || depth <= 0) {
    return modelBackend(model) === "hailo" ? 3 : null;
  }
  return Math.round(clamp(depth, 1, 4));
}

function modelHailoPipelineDepthLabel(model) {
  if (typeof model === "number") {
    return `并发 ${Math.round(clamp(model, 1, 4))}`;
  }
  if (modelBackend(model) !== "hailo") {
    return "";
  }
  const depth = modelHailoPipelineDepth(model);
  return depth ? `并发 ${depth}` : "并发默认";
}

function modelUsesHailoPipelineDepth(model) {
  return modelBackend(model) === "hailo";
}

function modelBackendFilterValue(model) {
  const backend = modelBackend(model);
  if (backend === "remote") {
    return "remote";
  }
  if (backend === "cloud_encrypted") {
    return "cloud_encrypted";
  }
  return backend === "hailo" ? "hef" : "rknn";
}

function shouldShowCloudEncryptedModelsInLibrary() {
  return state.uiBrand === UI_BRAND_TTBOX;
}

function modelVisibleInLibrary(model) {
  return shouldShowCloudEncryptedModelsInLibrary() || modelBackend(model) !== "cloud_encrypted";
}

function visibleModelLibraryModels(models) {
  return (Array.isArray(models) ? models : []).filter(modelVisibleInLibrary);
}

function modelUsesRknnConcurrency(model) {
  const backend = modelBackend(model);
  return backend === "rknn" || backend === "cloud_encrypted";
}

function modelRknnConcurrency(model) {
  const value = Number(model && model.rknn_concurrency);
  if (!Number.isFinite(value) || value <= 0) {
    return 1;
  }
  return Math.round(clamp(value, 1, 3));
}

function modelRknnConcurrencyLabel(modelOrValue) {
  const value = typeof modelOrValue === "number" ? modelOrValue : modelRknnConcurrency(modelOrValue);
  return `并发 ${Math.round(clamp(value, 1, 3))}`;
}

function normalizeRemoteFrameFormat(value) {
  const normalized = String(value || "").toLowerCase();
  if (normalized === "nv12" || normalized === "h264") {
    return normalized;
  }
  return "jpeg";
}

function remoteFrameFormatLabel(value) {
  const normalized = normalizeRemoteFrameFormat(value);
  if (normalized === "nv12") {
    return "NV12 直发";
  }
  if (normalized === "h264") {
    return "H.264 低延迟";
  }
  return "JPEG 编码";
}

function modelClassLabel(model) {
  const classCount = Number(model && model.class_count);
  if (Number.isFinite(classCount) && classCount > 0) {
    return `${classCount} 类`;
  }
  const description = String((model && model.description) || "");
  const match = description.match(/(\d+)\s*(?:类|类别)/);
  if (match) {
    return `${match[1]} 类`;
  }
  return "类别待检测";
}

function modelListSignature(models) {
  return (models || []).map((model) => [
    model.id,
    model.backend || "",
    model.display_name,
    model.file_name,
    model.input_width || 0,
    model.input_height || 0,
    model.output_count || 0,
    model.class_count || 0,
    model.hailo_pipeline_depth || 0,
    model.rknn_concurrency || 1,
    Array.isArray(model.class_names) ? model.class_names.join(",") : "",
    model.game_profile || "",
    model.preset_name || "",
    model.description || "",
    model.remote_host || "",
    model.remote_model_id || "",
    model.remote_engine_name || "",
    model.cloud_model_name || "",
    model.remote_frame_format || "jpeg",
    model.remote_available === false ? "missing" : "available",
  ].join("|")).join(";");
}

function currentModelImportType(form) {
  const selected = form ? form.querySelector('input[name="model_type"]:checked') : null;
  if (!selected) {
    return "rknn";
  }
  return ["onnx", "hef", "remote_onnx"].includes(selected.value) ? selected.value : "rknn";
}

function modelImportSubmitLabel(importType) {
  if (importType === "remote_onnx") return "上传到远端";
  if (importType === "onnx") return "转换并导入";
  if (importType === "hef") return "导入 HEF";
  return "导入";
}

function modelImportProgressMessage(importType) {
  return importType === "onnx" || importType === "remote_onnx" ? "正在导入并转换" : "正在导入";
}

function setModelImportStatus(mode, message = "") {
  const normalized = ["importing", "success", "error"].includes(mode) ? mode : "idle";
  const form = $("modelImportForm");
  const status = $("modelImportStatus");
  const submit = $("submitModelImportButton");
  const cancel = $("cancelModelImportButton");
  const close = $("closeModelImportButton");
  const dialog = $("modelImportDialog");
  const importType = currentModelImportType(form);
  const active = normalized !== "idle";
  const importing = normalized === "importing";

  if (state.modelImportCloseTimer) {
    window.clearTimeout(state.modelImportCloseTimer);
    state.modelImportCloseTimer = null;
  }
  state.modelImportState = normalized;

  if (dialog) {
    dialog.dataset.importState = normalized;
  }
  if (form) {
    form.classList.toggle("is-status-active", active);
    form.classList.toggle("is-importing", importing);
    form.classList.toggle("is-success", normalized === "success");
    form.classList.toggle("is-error", normalized === "error");
    Array.from(form.elements).forEach((element) => {
      if (element === submit || element === cancel) {
        return;
      }
      element.disabled = importing;
    });
  }
  if (submit) {
    submit.hidden = active;
    submit.disabled = importing;
    submit.textContent = modelImportSubmitLabel(importType);
  }
  if (cancel) {
    cancel.hidden = normalized === "importing" || normalized === "success";
    cancel.disabled = importing;
    cancel.textContent = normalized === "error" ? "关闭" : "取消";
  }
  if (close) {
    close.disabled = importing;
    close.setAttribute("aria-disabled", importing ? "true" : "false");
  }
  if (status) {
    status.hidden = !active;
    status.className = `import-status span-all is-${normalized}`;
    status.setAttribute("role", normalized === "error" ? "alert" : "status");
    status.setAttribute("aria-live", normalized === "error" ? "assertive" : "polite");
    status.replaceChildren();
    if (active) {
      const icon = document.createElement("span");
      icon.className = "import-status-icon";
      icon.setAttribute("aria-hidden", "true");
      const text = document.createElement("span");
      text.className = "import-status-text";
      text.textContent = message || (normalized === "success" ? "导入成功" : normalized === "error" ? "导入失败" : modelImportProgressMessage(importType));
      status.append(icon, text);
    }
  }
}

function updateModelImportMode() {
  const form = $("modelImportForm");
  if (!form) {
    return;
  }
  const importType = currentModelImportType(form);
  const fileInput = $("modelImportFile") || form.querySelector('input[name="file"]');
  const zipField = $("modelCalibrationZipField");
  const zipInput = $("modelCalibrationZip");
  const hint = $("modelImportHint");
  const submit = $("submitModelImportButton");
  const isOnnx = importType === "onnx";
  const isHef = importType === "hef";
  const isRemoteOnnx = importType === "remote_onnx";

  if (fileInput) {
    fileInput.accept = isRemoteOnnx ? ".onnx,.onnx.enc,.enc" : isOnnx ? ".onnx" : isHef ? ".hef,.hef.enc,.enc" : ".rknn,.rknn.enc,.enc";
  }
  if (zipField) {
    zipField.hidden = !isOnnx;
  }
  if (zipInput) {
    zipInput.required = false;
    if (!isOnnx) {
      zipInput.value = "";
    }
  }
  if (hint) {
    hint.hidden = !(isOnnx || isRemoteOnnx);
    hint.textContent = isRemoteOnnx
      ? "支持 ONNX 或使用本机设备模型码加密的 ONNX；Windows 会在内存转换并保存加密的 TensorRT engine。"
      : "上传 ONNX 后会自动转换为 RKNN，自定义校准图建议 5-15 张游戏截图即可，若不上传则使用内置校准图进行校准。";
  }
  if (submit && !submit.disabled && state.modelImportState === "idle") {
    submit.textContent = modelImportSubmitLabel(importType);
  }
}

function setModelImportBusy(busy, message = "") {
  setModelImportStatus(busy ? "importing" : "idle", message);
}

function setExportPresetLink(name) {
  const link = $("exportPresetButton");
  if (!link) {
    return;
  }
  if (!name) {
    link.href = "#";
    link.classList.add("is-disabled");
    return;
  }
  link.href = presetExportUrl(name);
  link.classList.remove("is-disabled");
}

function presetExportUrl(name) {
  return `/api/presets/${encodeURIComponent(name)}/export`;
}

function normalizeCropSize(value, fallback = 320) {
  const hasValue = value !== undefined && value !== null &&
    (typeof value !== "string" || value.trim() !== "");
  const numericValue = hasValue ? Number(value) : NaN;
  const hasFallback = fallback !== undefined && fallback !== null &&
    (typeof fallback !== "string" || fallback.trim() !== "");
  const fallbackNumber = hasFallback ? Number(fallback) : 320;
  const fallbackValue = Number.isFinite(fallbackNumber) ? fallbackNumber : 320;
  const safeValue = Number.isFinite(numericValue) ? numericValue : fallbackValue;
  return Math.round(clamp(safeValue, CROP_SIZE_MIN, CROP_SIZE_MAX));
}

function nearestCropSizeOptionIndex(value, fallback = 320) {
  const target = normalizeCropSize(value, fallback);
  let closestIndex = 0;
  CROP_SIZE_OPTIONS.forEach((candidate, index) => {
    const closest = CROP_SIZE_OPTIONS[closestIndex];
    if (Math.abs(candidate - target) < Math.abs(closest - target)) {
      closestIndex = index;
    }
  });
  return closestIndex;
}

function syncCropSizePresetRange(value) {
  const range = $("capture_crop_size_range");
  if (!range) {
    return;
  }
  range.min = "0";
  range.max = String(Math.max(0, CROP_SIZE_OPTIONS.length - 1));
  range.step = "1";
  range.value = String(nearestCropSizeOptionIndex(value));
}

function setCropSizeValue(value) {
  const input = $("capture_crop_size");
  if (!input) {
    return;
  }
  const cropSize = normalizeCropSize(value);
  input.value = formatControlValue(input, cropSize);
  syncCropSizePresetRange(cropSize);
}

function getCropSize(fallback = 320) {
  const input = $("capture_crop_size");
  if (!input) {
    return normalizeCropSize(fallback);
  }
  return normalizeCropSize(input.value, fallback);
}

function getAimReferenceOffsetLimit(cropSize = getCropSize(state.config && state.config.capture ? state.config.capture.crop_size : 320)) {
  return Math.floor(normalizeCropSize(cropSize) / 2);
}

function dynamicNumericRangeLimitsForId(id) {
  if (id === "capture_crop_offset_x" || id === "capture_crop_offset_y") {
    return [CAPTURE_CROP_OFFSET_MIN, CAPTURE_CROP_OFFSET_MAX];
  }
  if (id === "controller_aim_reference_offset_x" || id === "controller_aim_reference_offset_y") {
    const limit = getAimReferenceOffsetLimit();
    return [-limit, limit];
  }
  return null;
}

function updateDynamicOffsetControlLimits({ clampValues = false } = {}) {
  DYNAMIC_OFFSET_INPUT_IDS.forEach((id) => {
    const limits = dynamicNumericRangeLimitsForId(id);
    const input = $(id);
    if (!limits || !input) {
      return;
    }
    const [min, max] = limits;
    input.min = String(min);
    input.max = String(max);
    const rangeInput = $(`${id}_range`);
    if (rangeInput) {
      rangeInput.min = String(min);
      rangeInput.max = String(max);
    }
    if (clampValues && input.value !== "") {
      const clampedValue = clamp(Number(input.value), min, max);
      if (Number.isFinite(clampedValue)) {
        input.value = formatControlValue(input, clampedValue);
        if (rangeInput) {
          rangeInput.value = formatControlValue(rangeInput, clampedValue);
        }
      }
    }
  });
  syncRangeFieldsForIds(DYNAMIC_OFFSET_INPUT_IDS);
  updateAimRangeOverlay();
}

function getPreviewImageLayout(stage, cropSize) {
  if (!stage) {
    return null;
  }
  const stageRect = stage.getBoundingClientRect();
  if (stageRect.width <= 0 || stageRect.height <= 0) {
    return null;
  }

  const preview = $("previewImage");
  const naturalWidth = preview && preview.naturalWidth > 0 ? preview.naturalWidth : cropSize;
  const naturalHeight = preview && preview.naturalHeight > 0 ? preview.naturalHeight : cropSize;
  const imageScale = Math.min(stageRect.width / naturalWidth, stageRect.height / naturalHeight);
  const imageWidth = naturalWidth * imageScale;
  const imageHeight = naturalHeight * imageScale;
  return {
    imageLeft: (stageRect.width - imageWidth) * 0.5,
    imageTop: (stageRect.height - imageHeight) * 0.5,
    imageWidth,
    imageHeight,
    xScale: imageWidth / cropSize,
    yScale: imageHeight / cropSize,
    cropScale: Math.min(imageWidth, imageHeight) / cropSize,
  };
}

function updateTargetBoxOverlay() {
  const box = $("targetBoxOverlay");
  if (!box) return;
  const label = $("targetBoxLabel");
  const stage = box.closest(".preview-stage");
  const detection = state.data?.state?.detection || {};
  const target = detection.target_box;
  const allBoxes = Array.isArray(detection.boxes) ? detection.boxes : [];
  const displayTarget = (() => {
    if (!target) return null;
    const related = allBoxes.filter((candidate) => {
      const cx = (Number(candidate.x1) + Number(candidate.x2)) * 0.5;
      const cy = (Number(candidate.y1) + Number(candidate.y2)) * 0.5;
      const tw = Math.max(1, Number(target.x2) - Number(target.x1));
      const th = Math.max(1, Number(target.y2) - Number(target.y1));
      const verticalOverlap = Number(candidate.y2) >= Number(target.y1) && Number(candidate.y1) <= Number(target.y2);
      return verticalOverlap && Math.abs(cx - ((Number(target.x1) + Number(target.x2)) * 0.5)) <= Math.max(tw * 2.5, 180) &&
        Math.abs(cy - ((Number(target.y1) + Number(target.y2)) * 0.5)) <= th;
    });
    const boxes = related.length ? related : [target];
    return boxes.reduce((merged, candidate) => ({
      x1: Math.min(merged.x1, Number(candidate.x1)),
      y1: Math.min(merged.y1, Number(candidate.y1)),
      x2: Math.max(merged.x2, Number(candidate.x2)),
      y2: Math.max(merged.y2, Number(candidate.y2)),
    }), { x1: Number(target.x1), y1: Number(target.y1), x2: Number(target.x2), y2: Number(target.y2) });
  })();
  const cropSize = getCropSize(state.config?.capture?.crop_size || 320);
  const inputWidth = Number(state.data?.state?.capture?.input_width || cropSize);
  const inputHeight = Number(state.data?.state?.capture?.input_height || cropSize);
  const configuredCrop = Number(state.config?.capture?.crop_size || 0);
  const hasCaptureRoi = configuredCrop > 0 && configuredCrop <= inputWidth && configuredCrop <= inputHeight;
  const sourceWidth = hasCaptureRoi ? configuredCrop : inputWidth;
  const sourceHeight = hasCaptureRoi ? configuredCrop : inputHeight;
  const cropOffsetX = Number(state.config?.capture?.crop_offset_x || 0);
  const cropOffsetY = Number(state.config?.capture?.crop_offset_y || 0);
  const sourceOriginX = hasCaptureRoi
    ? Math.max(0, (inputWidth - sourceWidth) * 0.5 + cropOffsetX)
    : 0;
  const sourceOriginY = hasCaptureRoi
    ? Math.max(0, (inputHeight - sourceHeight) * 0.5 + cropOffsetY)
    : 0;
  const layout = getPreviewImageLayout(stage, cropSize);
  if (!displayTarget || !layout || !(sourceWidth > 0 && sourceHeight > 0)) {
    box.style.display = "none";
    return;
  }
  const sourceXScale = layout.imageWidth / sourceWidth;
  const sourceYScale = layout.imageHeight / sourceHeight;
  const x1 = clamp(Number(displayTarget.x1) - sourceOriginX, 0, sourceWidth);
  const y1 = clamp(Number(displayTarget.y1) - sourceOriginY, 0, sourceHeight);
  const x2 = clamp(Number(displayTarget.x2) - sourceOriginX, 0, sourceWidth);
  const y2 = clamp(Number(displayTarget.y2) - sourceOriginY, 0, sourceHeight);
  if (!(x2 > x1 && y2 > y1)) {
    box.style.display = "none";
    return;
  }
  box.style.display = "block";
  box.style.left = `${layout.imageLeft + x1 * sourceXScale}px`;
  box.style.top = `${layout.imageTop + y1 * sourceYScale}px`;
  box.style.width = `${(x2 - x1) * sourceXScale}px`;
  box.style.height = `${(y2 - y1) * sourceYScale}px`;
  if (label) {
    label.textContent = `目标 ${target.target_id ?? "-"} · 类别 ${target.class_id ?? "-"}`;
  }
}

function updateAimRangeOverlay() {
  const overlay = $("aimRangeOverlay");
  const dot = $("aimReferenceDot");
  const stage = (overlay || dot) ? (overlay || dot).closest(".preview-stage") : null;
  if (!stage) {
    return;
  }

  const cropSize = getCropSize(state.config && state.config.capture ? state.config.capture.crop_size : 320);
  const layout = getPreviewImageLayout(stage, cropSize);
  if (!layout) {
    return;
  }

  if (overlay) {
    const rangeFactor = clamp(getNumber("range_factor", state.config ? state.config.range_factor : 1), 0, 1);
    const side = cropSize * rangeFactor * layout.cropScale;
    overlay.style.left = `${layout.imageLeft + layout.imageWidth * 0.5}px`;
    overlay.style.top = `${layout.imageTop + layout.imageHeight * 0.5}px`;
    overlay.style.width = `${side}px`;
  }

  if (dot) {
    const controller = (state.config && state.config.ai && state.config.ai.controller) || {};
    const maxReference = Math.max(0, cropSize - 1);
    const referenceX = clamp(
      cropSize * 0.5 + getNumber(
        "controller_aim_reference_offset_x",
        controller.aim_reference_offset_x ?? CONTROLLER_DEFAULTS.aim_reference_offset_x
      ),
      0,
      maxReference
    );
    const referenceY = clamp(
      cropSize * 0.5 + getNumber(
        "controller_aim_reference_offset_y",
        controller.aim_reference_offset_y ?? CONTROLLER_DEFAULTS.aim_reference_offset_y
      ),
      0,
      maxReference
    );
    dot.style.left = `${layout.imageLeft + referenceX * layout.xScale}px`;
    dot.style.top = `${layout.imageTop + referenceY * layout.yScale}px`;
  }
}

function addRangeBinding(numberId, rangeId) {
  if (!numberId || !rangeId || RANGE_BINDINGS.some(([existingNumberId, existingRangeId]) => {
    return existingNumberId === numberId || existingRangeId === rangeId;
  })) {
    return;
  }
  RANGE_BINDINGS.push([numberId, rangeId]);
}

function rangeLimitsForInput(input) {
  const configured = dynamicNumericRangeLimitsForId(input.id) || NUMERIC_RANGE_LIMITS[input.id];
  const min = configured ? String(configured[0]) : input.min !== "" ? input.min : "0";
  const max = configured ? String(configured[1]) : input.max !== "" ? input.max : "100";
  const step = input.step && input.step !== "any" ? input.step : "1";
  return { min, max, step };
}

function enhanceNumericRangeControls() {
  document.querySelectorAll("[data-config][type='number']:not([readonly])").forEach((input) => {
    if (!input.id) {
      return;
    }
    const { min, max, step } = rangeLimitsForInput(input);
    input.min = min;
    input.max = max;
    input.step = step;
    if (input.id === "capture_crop_size") {
      syncCropSizePresetRange(input.value || OVERVIEW_DEFAULTS.capture_crop_size);
      const presetRange = $("capture_crop_size_range");
      if (presetRange) {
        enableThumbOnlyRangeInput(presetRange);
      }
      return;
    }

    const rangeId = `${input.id}_range`;
    let rangeInput = $(rangeId);
    if (!rangeInput) {
      rangeInput = document.createElement("input");
      rangeInput.id = rangeId;
      rangeInput.type = "range";
      rangeInput.className = "value-range";
      rangeInput.setAttribute("aria-label", `${input.id} 滑条`);
      input.insertAdjacentElement("afterend", rangeInput);
    }

    rangeInput.min = min;
    rangeInput.max = max;
    rangeInput.step = step;
    const rangeField = input.closest(".field, .slider-field");
    if (rangeField) {
      rangeField.classList.add("range-control-field");
    }
    enableThumbOnlyRangeInput(rangeInput);
    addRangeBinding(input.id, rangeId);
  });
}

function syncAllRangeFields() {
  RANGE_BINDINGS.forEach(([numberId, rangeId]) => {
    const numberInput = $(numberId);
    const rangeInput = $(rangeId);
    if (numberInput && rangeInput && numberInput.value !== "") {
      if (document.activeElement === numberInput) {
        if (Number.isFinite(Number(numberInput.value))) {
          rangeInput.value = formatControlValue(rangeInput, numberInput.value);
        }
        return;
      }
      const formattedValue = formatControlValue(numberInput, numberInput.value);
      numberInput.value = formattedValue;
      rangeInput.value = formatControlValue(rangeInput, formattedValue);
    }
  });
  syncCropSizePresetRange(getCropSize());
}

function initRangeBindings() {
  const cropSizeInput = $("capture_crop_size");
  const cropSizeRange = $("capture_crop_size_range");
  if (cropSizeInput && cropSizeRange) {
    enableThumbOnlyRangeInput(cropSizeRange);
    cropSizeRange.addEventListener("input", () => {
      const index = Math.round(clamp(Number(cropSizeRange.value), 0, CROP_SIZE_OPTIONS.length - 1));
      const cropSize = CROP_SIZE_OPTIONS[index] || OVERVIEW_DEFAULTS.capture_crop_size;
      cropSizeInput.value = formatControlValue(cropSizeInput, cropSize);
      cropSizeRange.value = String(index);
      updateDynamicOffsetControlLimits();
      updateAimRangeOverlay();
    });
    cropSizeRange.addEventListener("change", () => {
      const index = Math.round(clamp(Number(cropSizeRange.value), 0, CROP_SIZE_OPTIONS.length - 1));
      const cropSize = CROP_SIZE_OPTIONS[index] || OVERVIEW_DEFAULTS.capture_crop_size;
      cropSizeInput.value = formatControlValue(cropSizeInput, cropSize);
      cropSizeRange.value = String(index);
      updateDynamicOffsetControlLimits({ clampValues: true });
      requestConfigApply(70);
    });
    cropSizeInput.addEventListener("input", () => {
      if (cropSizeInput.value !== "" && Number.isFinite(Number(cropSizeInput.value))) {
        syncCropSizePresetRange(cropSizeInput.value);
        updateDynamicOffsetControlLimits();
      }
      updateAimRangeOverlay();
    });
    cropSizeInput.addEventListener("change", () => {
      if (cropSizeInput.value !== "") {
        clampNumberInputToLimits(cropSizeInput);
      }
      updateDynamicOffsetControlLimits({ clampValues: true });
      updateAimRangeOverlay();
    });
  }
  RANGE_BINDINGS.forEach(([numberId, rangeId]) => {
    const numberInput = $(numberId);
    const rangeInput = $(rangeId);
    if (!numberInput || !rangeInput) {
      return;
    }
    enableThumbOnlyRangeInput(rangeInput);
    const syncRangeToNumber = () => {
      const formattedValue = formatControlValue(numberInput, rangeInput.value);
      numberInput.value = formattedValue;
      rangeInput.value = formatControlValue(rangeInput, formattedValue);
      if (AIM_OVERLAY_CONFIG_IDS.has(numberId)) {
        updateAimRangeOverlay();
      }
    };
    rangeInput.addEventListener("input", syncRangeToNumber);
    rangeInput.addEventListener("change", () => {
      syncRangeToNumber();
      requestConfigApply(70);
    });
    numberInput.addEventListener("input", () => {
      if (numberInput.value !== "") {
        rangeInput.value = formatControlValue(rangeInput, numberInput.value);
      }
      if (AIM_OVERLAY_CONFIG_IDS.has(numberId)) {
        updateAimRangeOverlay();
      }
    });
    numberInput.addEventListener("keydown", (event) => {
      if (event.key === "Enter") {
        event.preventDefault();
        numberInput.blur();
      }
    });
    numberInput.addEventListener("change", () => {
      if (numberInput.value !== "") {
        clampNumberInputToLimits(numberInput);
        const formattedValue = formatControlValue(numberInput, numberInput.value);
        numberInput.value = formattedValue;
        rangeInput.value = formatControlValue(rangeInput, formattedValue);
      }
      if (AIM_OVERLAY_CONFIG_IDS.has(numberId)) {
        updateAimRangeOverlay();
      }
    });
  });
}

function currentModel() {
  const models = state.data && Array.isArray(state.data.models) ? state.data.models : [];
  const selectedId = (state.config && state.config.model_id) ||
    (state.data && state.data.config && state.data.config.model_id) ||
    "";
  return models.find((model) => model.id === selectedId) || models[0] || null;
}

function findModelById(modelId) {
  const models = state.data && Array.isArray(state.data.models) ? state.data.models : [];
  return models.find((model) => model.id === modelId) || null;
}

function selectedModelDisplayName(payload, runtime, detection) {
  const selectedId = (runtime && runtime.selected_model_id) ||
    (payload && payload.config && payload.config.model_id) ||
    (state.config && state.config.model_id) ||
    (state.data && state.data.config && state.data.config.model_id) ||
    "";
  const models = (payload && Array.isArray(payload.models) ? payload.models : null) ||
    (state.data && Array.isArray(state.data.models) ? state.data.models : []);
  const model = models.find((item) => item.id === selectedId) || null;
  return (detection && detection.model_name) || (model ? modelFileName(model) : "") || selectedId;
}

function currentModelClassCount() {
  const model = currentModel();
  const count = Number(model && model.class_count);
  if (Number.isFinite(count) && count > 0) {
    return Math.min(AIM_CLASS_MAX_COUNT, Math.max(1, Math.floor(count)));
  }
  return 1;
}

function modelClassNames(model) {
  const names = model && Array.isArray(model.class_names) ? model.class_names : [];
  return names.slice(0, AIM_CLASS_MAX_COUNT).map((name) => String(name || "").trim());
}

function currentModelClassNames() {
  return modelClassNames(currentModel());
}

function normalizeLoopoutOverlayColor(value) {
  const color = String(value || "").trim();
  return /^#[0-9a-f]{6}$/i.test(color) ? color.toLowerCase() : "#ff5edb";
}

function loopoutOverlayClassMask(config = {}) {
  const raw = Number(config.class_mask);
  return Number.isFinite(raw) && raw >= 0 ? Math.floor(raw) & AIM_CLASS_ALL_MASK : AIM_CLASS_ALL_MASK;
}

function loopoutOverlayClassCount() {
  const model = currentModel();
  const count = Number(model && model.class_count);
  const namedCount = modelClassNames(model).length;
  const resolved = Number.isFinite(count) && count > 0 ? count : namedCount;
  return Math.max(1, Math.min(AIM_CLASS_MAX_COUNT, Math.floor(resolved || 1)));
}

function renderLoopoutOverlayClassOptions(mask = AIM_CLASS_ALL_MASK) {
  const container = $("displayLoopoutOverlayClasses");
  if (!container) {
    return;
  }
  const names = currentModelClassNames();
  const count = loopoutOverlayClassCount();
  container.innerHTML = "";
  container.dataset.overlayClassMask = String(mask & AIM_CLASS_ALL_MASK);
  for (let classId = 0; classId < count; classId += 1) {
    const label = document.createElement("label");
    label.className = "check-line";
    const input = document.createElement("input");
    input.type = "checkbox";
    input.dataset.overlayClassId = String(classId);
    input.checked = (mask & (2 ** classId)) !== 0;
    const text = document.createElement("span");
    text.textContent = classDisplayName(classId, names);
    label.append(input, text);
    container.appendChild(label);
  }
}

function populateLoopoutOverlaySettings(config = {}) {
  setValue("display_loopout_overlay_thickness", Math.max(1, Math.min(8, Number(config.thickness) || 2)));
  setCheckbox("display_loopout_overlay_use_class_colors", config.use_class_colors !== false);
  setValue("display_loopout_overlay_color", normalizeLoopoutOverlayColor(config.color));
  renderLoopoutOverlayClassOptions(loopoutOverlayClassMask(config));
  updateLoopoutOverlayColorUi();
}

function updateLoopoutOverlayColorUi() {
  const useClassColors = getCheckbox("display_loopout_overlay_use_class_colors");
  const color = $("display_loopout_overlay_color");
  if (color) {
    color.disabled = useClassColors;
  }
}

function collectLoopoutOverlaySettings() {
  const classInputs = document.querySelectorAll("#displayLoopoutOverlayClasses input[data-overlay-class-id]");
  const visibleMask = classInputs.length >= AIM_CLASS_MAX_COUNT
    ? AIM_CLASS_ALL_MASK
    : (2 ** classInputs.length) - 1;
  const originalMask = Number($("displayLoopoutOverlayClasses")?.dataset.overlayClassMask || 0) & AIM_CLASS_ALL_MASK;
  let mask = classInputs.length === 0
    ? loopoutOverlayClassMask((state.config && state.config.loopout_overlay) || {})
    : originalMask & (AIM_CLASS_ALL_MASK ^ visibleMask);
  classInputs.forEach((input) => {
    if (input.checked) {
      mask += 2 ** Number(input.dataset.overlayClassId || 0);
    }
  });
  const thickness = Math.max(1, Math.min(8, Math.round(Number(getString("display_loopout_overlay_thickness")) || 2)));
  setValue("display_loopout_overlay_thickness", thickness);
  return {
    class_mask: mask,
    thickness,
    use_class_colors: getCheckbox("display_loopout_overlay_use_class_colors"),
    color: normalizeLoopoutOverlayColor(getString("display_loopout_overlay_color")),
  };
}

function setLoopoutOverlaySettingsOpen(open) {
  const popover = $("displayLoopoutOverlaySettingsPopover");
  const button = $("displayLoopoutOverlaySettingsButton");
  if (!popover || !button) {
    return;
  }
  popover.hidden = !open;
  button.setAttribute("aria-expanded", open ? "true" : "false");
  if (open) {
    populateLoopoutOverlaySettings((state.config && state.config.loopout_overlay) || {});
  }
}

function currentModelClassRenderSignature(modelId = "") {
  const model = currentModel();
  const selectedId = modelId || (model && model.id) || "";
  return `${selectedId}:${currentModelClassCount()}:${currentModelClassNames().join("\u001f")}`;
}

function classDisplayName(classId, names = currentModelClassNames()) {
  const parsedName = names[classId];
  return parsedName ? parsedName : `类别 ${classId}`;
}

function modelClassEditCount(model) {
  const count = Number(model && model.class_count);
  if (Number.isFinite(count) && count > 0) {
    return Math.min(AIM_CLASS_MAX_COUNT, Math.max(1, Math.floor(count)));
  }
  return Math.min(AIM_CLASS_MAX_COUNT, Math.max(1, modelClassNames(model).length || 1));
}

function classMaskFromClassId(classId) {
  const classCount = currentModelClassCount();
  const safeClassId = Math.floor(clamp(
    Number.isFinite(Number(classId)) ? Number(classId) : AUTO_BACK_FLICK_DEFAULTS.class_id,
    0,
    Math.max(0, classCount - 1)
  ));
  return 1 << safeClassId;
}

function firstClassIdFromMask(mask, fallback = AUTO_BACK_FLICK_DEFAULTS.class_id) {
  const visibleMask = currentModelClassMask();
  const normalizedMask = Number(mask) & visibleMask;
  for (let classId = 0; classId < currentModelClassCount(); classId += 1) {
    if ((normalizedMask & (1 << classId)) !== 0) {
      return classId;
    }
  }
  return Math.floor(clamp(
    Number.isFinite(Number(fallback)) ? Number(fallback) : AUTO_BACK_FLICK_DEFAULTS.class_id,
    0,
    Math.max(0, currentModelClassCount() - 1)
  ));
}

function autoBackFlickClassMask(autoBackFlick = {}) {
  const visibleMask = currentModelClassMask();
  const rawMask = Number(autoBackFlick && autoBackFlick.class_filter_mask);
  if (Number.isFinite(rawMask) && rawMask >= 0) {
    return rawMask & visibleMask;
  }
  return classMaskFromClassId(
    autoBackFlick && autoBackFlick.class_id !== undefined
      ? autoBackFlick.class_id
      : AUTO_BACK_FLICK_DEFAULTS.class_id
  ) & visibleMask;
}

function collectAutoBackFlickClassMask(fallback = AUTO_BACK_FLICK_DEFAULTS) {
  const picker = $("autoBackFlickClassPicker");
  if (!picker) {
    return autoBackFlickClassMask(fallback);
  }
  const checkboxes = Array.from(picker.querySelectorAll(".auto-back-flick-class"));
  if (checkboxes.length === 0) {
    return autoBackFlickClassMask(fallback);
  }
  let mask = 0;
  checkboxes.forEach((checkbox) => {
    if (checkbox.checked) {
      mask |= 1 << Number(checkbox.dataset.classId);
    }
  });
  return mask & currentModelClassMask();
}

function finiteAutoBackFlickNumber(sourceValue, legacyValue, defaultValue) {
  const value = Number(sourceValue ?? legacyValue ?? defaultValue);
  return Number.isFinite(value) ? value : defaultValue;
}

function normalizeAutoBackFlickClassConfig(classId, source = {}, legacy = AUTO_BACK_FLICK_DEFAULTS) {
  return {
    class_id: classId,
    random_direction: Boolean(source.random_direction ?? legacy.random_direction),
    dodge_away_from_target: Boolean(source.random_direction ?? legacy.random_direction)
      ? false
      : Boolean(source.dodge_away_from_target ?? legacy.dodge_away_from_target),
    turn_pixels: clamp(
      finiteAutoBackFlickNumber(source.turn_pixels, legacy.turn_pixels, AUTO_BACK_FLICK_DEFAULTS.turn_pixels),
      -20000,
      20000
    ),
    wait_ms: clamp(
      finiteAutoBackFlickNumber(source.wait_ms, legacy.wait_ms, AUTO_BACK_FLICK_DEFAULTS.wait_ms),
      0,
      2000
    ),
    turn_random: clamp(
      finiteAutoBackFlickNumber(source.turn_random, legacy.turn_random, AUTO_BACK_FLICK_DEFAULTS.turn_random),
      0,
      1000
    ),
    return_random: clamp(
      finiteAutoBackFlickNumber(source.return_random, legacy.return_random, AUTO_BACK_FLICK_DEFAULTS.return_random),
      0,
      1000
    ),
    steps: Math.round(clamp(
      finiteAutoBackFlickNumber(source.steps, legacy.steps, AUTO_BACK_FLICK_DEFAULTS.steps),
      1,
      300
    )),
    cooldown_ms: Math.round(clamp(
      finiteAutoBackFlickNumber(source.cooldown_ms, legacy.cooldown_ms, AUTO_BACK_FLICK_DEFAULTS.cooldown_ms),
      500,
      10000
    )),
    trigger_delay_ms: Math.round(clamp(
      finiteAutoBackFlickNumber(
        source.trigger_delay_ms,
        legacy.trigger_delay_ms,
        AUTO_BACK_FLICK_DEFAULTS.trigger_delay_ms
      ),
      0,
      5000
    )),
    confidence: clamp(
      finiteAutoBackFlickNumber(source.confidence, legacy.confidence, AUTO_BACK_FLICK_DEFAULTS.confidence),
      0,
      1
    ),
  };
}

function setAutoBackFlickClassConfigs(autoBackFlick = AUTO_BACK_FLICK_DEFAULTS) {
  const byClassId = new Map();
  const sourceConfigs = Array.isArray(autoBackFlick.class_configs) ? autoBackFlick.class_configs : [];
  sourceConfigs.forEach((source) => {
    const classId = Math.floor(Number(source && source.class_id));
    if (classId >= 0 && classId < AIM_CLASS_MAX_COUNT && !byClassId.has(classId)) {
      byClassId.set(classId, normalizeAutoBackFlickClassConfig(classId, source, autoBackFlick));
    }
  });
  if (byClassId.size === 0) {
    const mask = autoBackFlickClassMask(autoBackFlick);
    for (let classId = 0; classId < currentModelClassCount(); classId += 1) {
      if ((mask & (1 << classId)) !== 0) {
        byClassId.set(classId, normalizeAutoBackFlickClassConfig(classId, {}, autoBackFlick));
      }
    }
  }
  state.autoBackFlickClassConfigs = Array.from(byClassId.values()).sort((a, b) => a.class_id - b.class_id);
}

function autoBackFlickClassConfigFor(classId) {
  const existing = state.autoBackFlickClassConfigs.find((config) => config.class_id === classId);
  if (existing) {
    return normalizeAutoBackFlickClassConfig(classId, existing);
  }
  const legacy = state.config && state.config.auto_back_flick
    ? state.config.auto_back_flick
    : AUTO_BACK_FLICK_DEFAULTS;
  return normalizeAutoBackFlickClassConfig(classId, {}, legacy);
}

function collectAutoBackFlickClassConfigs() {
  return state.autoBackFlickClassConfigs
    .filter((config) => config.class_id >= 0 && config.class_id < AIM_CLASS_MAX_COUNT)
    .map((config) => normalizeAutoBackFlickClassConfig(config.class_id, config))
    .sort((a, b) => a.class_id - b.class_id);
}

function currentAutoBackFlickClassConfig() {
  const fallback = state.config && state.config.auto_back_flick
    ? state.config.auto_back_flick
    : AUTO_BACK_FLICK_DEFAULTS;
  const mask = collectAutoBackFlickClassMask(fallback);
  return {
    ...fallback,
    class_id: firstClassIdFromMask(mask, fallback.class_id ?? AUTO_BACK_FLICK_DEFAULTS.class_id),
    class_filter_mask: mask,
    class_configs: collectAutoBackFlickClassConfigs(),
  };
}

function renderAutoBackFlickClassPicker(autoBackFlick = AUTO_BACK_FLICK_DEFAULTS) {
  const picker = $("autoBackFlickClassPicker");
  if (!picker) {
    return;
  }
  const classCount = currentModelClassCount();
  const classNames = currentModelClassNames();
  const mask = autoBackFlickClassMask(autoBackFlick);
  const classPickers = Array.from({ length: classCount }, (_, classId) => {
    const checked = mask & (1 << classId) ? "checked" : "";
    const className = classDisplayName(classId, classNames);
    const classLabel = `${classId} ${className}`;
    return `
      <div class="class-chip auto-back-flick-class-chip" title="${escapeAttr(classLabel)}">
        <label class="class-chip-label">
          <input class="aim-profile-class auto-back-flick-class" type="checkbox" data-class-id="${classId}" ${checked}>
          <span class="class-chip-name">${escapeHtml(classLabel)}</span>
        </label>
        <button class="icon-button auto-back-flick-settings-button" type="button" data-back-flick-class-settings="${classId}" aria-label="设置${escapeAttr(classLabel)}参数" title="类别参数">
          <svg viewBox="0 0 24 24" aria-hidden="true"><path d="M12 15.5a3.5 3.5 0 1 0 0-7 3.5 3.5 0 0 0 0 7Z"/><path d="M19.4 15a1.7 1.7 0 0 0 .34 1.88l.06.06-2.83 2.83-.06-.06a1.7 1.7 0 0 0-1.88-.34 1.7 1.7 0 0 0-1.03 1.56V21h-4v-.08A1.7 1.7 0 0 0 8.94 19.4a1.7 1.7 0 0 0-1.88.34l-.06.06-2.83-2.83.06-.06A1.7 1.7 0 0 0 4.57 15 1.7 1.7 0 0 0 3 14H3v-4h.08A1.7 1.7 0 0 0 4.6 8.94a1.7 1.7 0 0 0-.34-1.88L4.2 7l2.83-2.83.06.06A1.7 1.7 0 0 0 9 4.57 1.7 1.7 0 0 0 10 3h4v.08a1.7 1.7 0 0 0 1.06 1.52 1.7 1.7 0 0 0 1.88-.34L17 4.2 19.83 7l-.06.06A1.7 1.7 0 0 0 19.43 9 1.7 1.7 0 0 0 21 10v4h-.08A1.7 1.7 0 0 0 19.4 15Z"/></svg>
        </button>
      </div>
    `;
  }).join("");
  picker.innerHTML = `
    <div class="class-picker-head">
      <span>触发类别 · ${classCount} 类</span>
      <button class="mini-button" type="button" data-select-all-classes>全选</button>
    </div>
    <div class="class-chip-grid auto-back-flick-class-grid">
      ${classPickers}
    </div>
    <span class="field-hint">检测到任一勾选类别后触发背闪动作。</span>
  `;
  picker.querySelectorAll(".auto-back-flick-class").forEach((checkbox) => {
    checkbox.addEventListener("change", () => {
      const classId = Number(checkbox.dataset.classId);
      if (checkbox.checked && !state.autoBackFlickClassConfigs.some((config) => config.class_id === classId)) {
        state.autoBackFlickClassConfigs.push(autoBackFlickClassConfigFor(classId));
      }
      updateClassToggleButton(picker);
      requestConfigApply(80);
    });
  });
  picker.querySelectorAll("[data-back-flick-class-settings]").forEach((button) => {
    button.addEventListener("click", () => {
      setAutoBackFlickClassDialogOpen(true, Number(button.dataset.backFlickClassSettings));
    });
  });
  const toggleButton = picker.querySelector("[data-select-all-classes]");
  if (toggleButton) {
    toggleButton.addEventListener("click", () => {
      const checkboxes = Array.from(picker.querySelectorAll(".auto-back-flick-class"));
      const shouldCheck = !checkboxes.every((checkbox) => checkbox.checked);
      checkboxes.forEach((checkbox) => {
        checkbox.checked = shouldCheck;
        const classId = Number(checkbox.dataset.classId);
        if (shouldCheck && !state.autoBackFlickClassConfigs.some((config) => config.class_id === classId)) {
          state.autoBackFlickClassConfigs.push(autoBackFlickClassConfigFor(classId));
        }
      });
      updateClassToggleButton(picker);
      requestConfigApply(80);
    });
  }
  updateClassToggleButton(picker);
  state.autoBackFlickClassRenderSignature = currentModelClassRenderSignature();
}

function setAutoBackFlickClassDialogOpen(open, classId = -1) {
  const dialog = $("autoBackFlickClassDialog");
  if (!dialog) return;
  if (!open) {
    state.autoBackFlickEditingClassId = -1;
    dialog.hidden = true;
    setAnyModalOpen();
    return;
  }
  if (!Number.isInteger(classId) || classId < 0 || classId >= currentModelClassCount()) return;
  const config = autoBackFlickClassConfigFor(classId);
  state.autoBackFlickEditingClassId = classId;
  setText("autoBackFlickClassDialogTitle", `${classId} ${classDisplayName(classId)}`);
  setText("autoBackFlickClassDialogSubtitle", "此处参数只作用于当前类别");
  setCheckbox("autoBackFlickClassRandomDirection", config.random_direction);
  setCheckbox("autoBackFlickClassDodgeAway", config.dodge_away_from_target);
  setValue("autoBackFlickClassTriggerDelay", config.trigger_delay_ms);
  setValue("autoBackFlickClassConfidence", config.confidence);
  setValue("autoBackFlickClassTurnPixels", config.turn_pixels);
  setValue("autoBackFlickClassWait", config.wait_ms);
  setValue("autoBackFlickClassSteps", config.steps);
  setValue("autoBackFlickClassCooldown", config.cooldown_ms);
  setValue("autoBackFlickClassTurnRandom", config.turn_random);
  setValue("autoBackFlickClassReturnRandom", config.return_random);
  syncAutoBackFlickClassDialogRanges();
  dialog.hidden = false;
  setAnyModalOpen();
  $("autoBackFlickClassTriggerDelay").focus();
}

const AUTO_BACK_FLICK_DIALOG_RANGE_FIELDS = [
  "autoBackFlickClassTriggerDelay", "autoBackFlickClassConfidence", "autoBackFlickClassTurnPixels",
  "autoBackFlickClassWait", "autoBackFlickClassSteps", "autoBackFlickClassCooldown",
  "autoBackFlickClassTurnRandom", "autoBackFlickClassReturnRandom",
];

function syncAutoBackFlickClassDialogRanges() {
  AUTO_BACK_FLICK_DIALOG_RANGE_FIELDS.forEach((numberId) => {
    const numberInput = $(numberId);
    const rangeInput = $(`${numberId}Range`);
    if (numberInput && rangeInput && numberInput.value !== "") rangeInput.value = numberInput.value;
  });
}

function bindAutoBackFlickClassDialogRanges() {
  AUTO_BACK_FLICK_DIALOG_RANGE_FIELDS.forEach((numberId) => {
    const numberInput = $(numberId);
    const rangeInput = $(`${numberId}Range`);
    if (!numberInput || !rangeInput) return;
    enableThumbOnlyRangeInput(rangeInput);
    rangeInput.addEventListener("input", () => {
      numberInput.value = rangeInput.value;
    });
    numberInput.addEventListener("input", () => {
      if (numberInput.value !== "" && Number.isFinite(Number(numberInput.value))) {
        rangeInput.value = numberInput.value;
      }
    });
    numberInput.addEventListener("change", () => {
      clampNumberInputToLimits(numberInput);
      rangeInput.value = numberInput.value;
    });
  });
}

function saveAutoBackFlickClassDialog() {
  const classId = state.autoBackFlickEditingClassId;
  if (classId < 0) return;
  if (AUTO_BACK_FLICK_DIALOG_RANGE_FIELDS.some((id) => $(id).value === "" || !$(id).checkValidity())) {
    showToast("请填写有效的类别参数", true);
    return;
  }
  const randomDirection = getCheckbox("autoBackFlickClassRandomDirection");
  const config = normalizeAutoBackFlickClassConfig(classId, {
    random_direction: randomDirection,
    dodge_away_from_target: randomDirection ? false : getCheckbox("autoBackFlickClassDodgeAway"),
    trigger_delay_ms: Number($("autoBackFlickClassTriggerDelay").value),
    confidence: Number($("autoBackFlickClassConfidence").value),
    turn_pixels: Number($("autoBackFlickClassTurnPixels").value),
    wait_ms: Number($("autoBackFlickClassWait").value),
    steps: Number($("autoBackFlickClassSteps").value),
    cooldown_ms: Number($("autoBackFlickClassCooldown").value),
    turn_random: Number($("autoBackFlickClassTurnRandom").value),
    return_random: Number($("autoBackFlickClassReturnRandom").value),
  });
  state.autoBackFlickClassConfigs = state.autoBackFlickClassConfigs.filter((item) => item.class_id !== classId);
  state.autoBackFlickClassConfigs.push(config);
  state.autoBackFlickClassConfigs.sort((a, b) => a.class_id - b.class_id);
  setAutoBackFlickClassDialogOpen(false);
  requestConfigApply(0);
}

function classMaskForCount(count) {
  const safeCount = Math.min(AIM_CLASS_MAX_COUNT, Math.max(1, Math.floor(Number(count) || 1)));
  return safeCount >= AIM_CLASS_MAX_COUNT ? AIM_CLASS_ALL_MASK : ((1 << safeCount) - 1);
}

function currentModelClassMask() {
  return classMaskForCount(currentModelClassCount());
}

function classMaskFromProfile(profile) {
  const mask = Number(profile && profile.class_filter_mask);
  const visibleMask = currentModelClassMask();
  if (!Number.isFinite(mask) || mask < 0) {
    return visibleMask;
  }
  const normalizedMask = mask & visibleMask;
  if (mask > 0 && normalizedMask === 0) {
    return visibleMask;
  }
  return normalizedMask;
}

function clampAimProfileOffset(value, fallback = 0) {
  const number = Number(value);
  return clamp(
    Number.isFinite(number) ? number : fallback,
    AIM_PROFILE_AXIS_OFFSET_MIN,
    AIM_PROFILE_AXIS_OFFSET_MAX
  );
}

function clampAimClassPriority(value, fallback = 0) {
  const number = Number(value);
  return Math.round(clamp(Number.isFinite(number) ? number : fallback, 0, 9));
}

function clampAimClassForceSwitchDelay(value, fallback = 30) {
  const number = Number(value);
  return Math.round(clamp(Number.isFinite(number) ? number : fallback, 0, 1000));
}

function legacyAimPosToOffsetY(pos) {
  return clampAimProfileOffset(pos, 0.5);
}

function aimOffsetYToLegacyPos(offsetY) {
  return clampAimProfileOffset(offsetY, 0.5);
}

function aimProfileOffsetX(profile) {
  return clampAimProfileOffset(profile && profile.offset_x !== undefined ? profile.offset_x : 0.5, 0.5);
}

function aimProfileOffsetY(profile) {
  if (profile && profile.offset_y !== undefined) {
    return clampAimProfileOffset(profile.offset_y, 0.5);
  }
  return legacyAimPosToOffsetY(profile && profile.pos !== undefined ? profile.pos : 0.5);
}

function aimProfileOffsetSwitchEnabled(profile) {
  return Boolean(profile && profile.offset_switch_enabled);
}

function aimProfileOffsetSwitchHotkey(profile) {
  const hotkey = profile && typeof profile.offset_switch_hotkey === "string" ? profile.offset_switch_hotkey : "";
  return OPTIONAL_POINTER_HOTKEYS.includes(hotkey) ? hotkey : "";
}

function aimProfileAlternateOffsetRuntimeStates(aim = {}) {
  return Array.isArray(aim.aim_profile_alternate_offset_states)
    ? aim.aim_profile_alternate_offset_states
    : [];
}

function updateAimProfileOffsetSwitchStatus(aim = {}) {
  const states = aimProfileAlternateOffsetRuntimeStates(aim);
  document.querySelectorAll(".aim-profile-card").forEach((card, index) => {
    const status = card.querySelector(".aim-profile-offset-switch-status");
    if (!status) {
      return;
    }
    const enabled = Boolean(card.querySelector(".aim-profile-offset-switch-enabled")?.checked);
    const switched = enabled && states[index] === true;
    status.textContent = switched ? "已切换" : "未切换";
    status.classList.toggle("is-switched", switched);
    status.classList.toggle("is-disabled", !enabled);
  });
}

function aimProfileAlternateOffsetX(profile) {
  if (profile && profile.alternate_offset_x !== undefined) {
    return clampAimProfileOffset(profile.alternate_offset_x, aimProfileOffsetX(profile));
  }
  return aimProfileOffsetX(profile);
}

function aimProfileAlternateOffsetY(profile) {
  if (profile && profile.alternate_offset_y !== undefined) {
    return clampAimProfileOffset(profile.alternate_offset_y, aimProfileOffsetY(profile));
  }
  return aimProfileOffsetY(profile);
}

function aimProfileClassOffsets(profile) {
  const classCount = currentModelClassCount();
  const byClass = new Map();
  const items = Array.isArray(profile && profile.class_offsets) ? profile.class_offsets : [];
  const fallbackX = aimProfileOffsetX(profile);
  const fallbackY = aimProfileOffsetY(profile);
  items.forEach((item) => {
    const classId = Math.floor(Number(item && item.class_id));
    if (!Number.isFinite(classId) || classId < 0 || classId >= classCount) {
      return;
    }
    byClass.set(classId, {
      class_id: classId,
      offset_x: clampAimProfileOffset(item.offset_x, fallbackX),
      offset_y: clampAimProfileOffset(item.offset_y, fallbackY),
      priority: clampAimClassPriority(item.priority, 0),
      force_priority_switch: item.force_priority_switch === true,
      force_switch_delay_ms: clampAimClassForceSwitchDelay(item.force_switch_delay_ms, 30),
    });
  });
  return Array.from(byClass.values()).sort((a, b) => a.class_id - b.class_id);
}

function aimProfileClassOffset(profile, classId) {
  return aimProfileClassOffsets(profile).find((offset) => offset.class_id === classId) || null;
}

function aimProfileFovScale(profile) {
  const value = Number(profile && profile.fov_scale !== undefined ? profile.fov_scale : 1);
  return clamp(
    Number.isFinite(value) ? value : 1,
    AIM_PROFILE_FOV_SCALE_MIN,
    AIM_PROFILE_FOV_SCALE_MAX
  );
}

function profileTemplate(profile, index) {
  const classCount = currentModelClassCount();
  const classNames = currentModelClassNames();
  const mask = classMaskFromProfile(profile);
  const offsetX = aimProfileOffsetX(profile).toFixed(2);
  const offsetY = aimProfileOffsetY(profile).toFixed(2);
  const alternateOffsetX = aimProfileAlternateOffsetX(profile).toFixed(2);
  const alternateOffsetY = aimProfileAlternateOffsetY(profile).toFixed(2);
  const offsetSwitchChecked = aimProfileOffsetSwitchEnabled(profile) ? "checked" : "";
  const offsetSwitchPanelHidden = aimProfileOffsetSwitchEnabled(profile) ? "" : "hidden";
  const sensitivity = clamp(Number(profile && profile.sensitivity !== undefined ? profile.sensitivity : 1), 0.1, 3).toFixed(2);
  const fovScale = aimProfileFovScale(profile).toFixed(2);
  const classPickers = Array.from({ length: classCount }, (_, classId) => {
    const checked = mask & (1 << classId) ? "checked" : "";
    const className = classDisplayName(classId, classNames);
    const classLabel = `${classId} ${className}`;
    const classOffset = aimProfileClassOffset(profile, classId);
    const classOffsetEnabled = Boolean(classOffset);
    const classOffsetX = (classOffset ? classOffset.offset_x : Number(offsetX)).toFixed(2);
    const classOffsetY = (classOffset ? classOffset.offset_y : Number(offsetY)).toFixed(2);
    const classPriority = classOffset ? clampAimClassPriority(classOffset.priority, 0) : 0;
    const classForcePrioritySwitch = classOffset && classOffset.force_priority_switch === true;
    const classForceSwitchDelayMs = classOffset
      ? clampAimClassForceSwitchDelay(classOffset.force_switch_delay_ms, 30)
      : 30;
    const classOffsetActiveClass = classOffsetEnabled ? " has-class-offset" : "";
    const classOffsetEnabledValue = classOffsetEnabled ? "1" : "0";
    return `
      <div class="class-chip aim-class-chip${classOffsetActiveClass}" title="${escapeAttr(classLabel)}" data-class-id="${classId}" data-class-offset-enabled="${classOffsetEnabledValue}" data-class-offset-x="${classOffsetX}" data-class-offset-y="${classOffsetY}" data-class-priority="${classPriority}" data-class-force-priority-switch="${classForcePrioritySwitch ? "1" : "0"}" data-class-force-switch-delay-ms="${classForceSwitchDelayMs}">
        <label class="class-chip-label">
          <input class="aim-profile-class" type="checkbox" data-class-id="${classId}" ${checked}>
          <span class="class-chip-name">${escapeHtml(classLabel)}</span>
        </label>
        <button class="class-offset-button" type="button" title="类别设置" aria-label="${escapeAttr(`${classLabel} 类别设置`)}">
          <svg viewBox="0 0 24 24" aria-hidden="true" fill="none" stroke="currentColor" stroke-width="2" stroke-linecap="round" stroke-linejoin="round"><path d="M12 15.5a3.5 3.5 0 1 0 0-7 3.5 3.5 0 0 0 0 7Z"/><path d="M19.4 15a1.65 1.65 0 0 0 .33 1.82l.06.06a2 2 0 0 1-2.83 2.83l-.06-.06A1.65 1.65 0 0 0 15 19.4a1.65 1.65 0 0 0-1 .6V20a2 2 0 0 1-4 0v-.1a1.65 1.65 0 0 0-1-.6 1.65 1.65 0 0 0-1.82.33l-.06.06a2 2 0 0 1-2.83-2.83l.06-.06A1.65 1.65 0 0 0 4.6 15a1.65 1.65 0 0 0-.6-1H4a2 2 0 0 1 0-4h.1a1.65 1.65 0 0 0 .6-1 1.65 1.65 0 0 0-.33-1.82l-.06-.06a2 2 0 0 1 2.83-2.83l.06.06A1.65 1.65 0 0 0 9 4.6a1.65 1.65 0 0 0 1-.6V4a2 2 0 0 1 4 0v.1a1.65 1.65 0 0 0 1 .6 1.65 1.65 0 0 0 1.82-.33l.06-.06a2 2 0 0 1 2.83 2.83l-.06.06A1.65 1.65 0 0 0 19.4 9c.14.33.35.62.6 1H20a2 2 0 0 1 0 4h-.1a1.65 1.65 0 0 0-.5 1Z"/></svg>
        </button>
        <div class="class-offset-popover" hidden>
          <div class="class-offset-popover-head">
            <strong>${escapeHtml(classLabel)}</strong>
            <button class="mini-button class-offset-clear" type="button">清除</button>
          </div>
          <div class="class-offset-axis-grid">
            <div class="slider-field class-offset-axis-field">
              <div class="label-row">
                <label>X轴</label>
                <input class="aim-profile-class-offset-x value-input" type="number" min="0" max="1" step="0.01" autocomplete="off" value="${classOffsetX}">
              </div>
              <input class="aim-profile-class-offset-x-range" type="range" min="0" max="1" step="0.01" value="${classOffsetX}">
            </div>
            <div class="slider-field class-offset-axis-field">
              <div class="label-row">
                <label>Y轴</label>
                <input class="aim-profile-class-offset-y value-input" type="number" min="0" max="1" step="0.01" autocomplete="off" value="${classOffsetY}">
              </div>
              <input class="aim-profile-class-offset-y-range" type="range" min="0" max="1" step="0.01" value="${classOffsetY}">
            </div>
          </div>
          <div class="slider-field class-offset-axis-field class-priority-field">
            <div class="label-row">
              <label>锁定优先级</label>
              <input class="aim-profile-class-priority value-input" type="number" min="0" max="9" step="1" autocomplete="off" value="${classPriority}">
            </div>
            <input class="aim-profile-class-priority-range" type="range" min="0" max="9" step="1" value="${classPriority}">
          </div>
          <div class="class-force-switch-row">
            <span>强制模式</span>
            <label class="switch" title="高优先级类别稳定出现后抢占当前锁定目标">
              <input class="aim-profile-class-force-switch" type="checkbox" ${classForcePrioritySwitch ? "checked" : ""}>
              <span></span>
            </label>
          </div>
          <div class="slider-field class-offset-axis-field class-force-switch-delay-field"${classForcePrioritySwitch ? "" : " hidden"}>
            <div class="label-row">
              <label>切换延迟（ms）</label>
              <input class="aim-profile-class-force-delay value-input" type="number" min="0" max="1000" step="1" autocomplete="off" value="${classForceSwitchDelayMs}">
            </div>
            <input class="aim-profile-class-force-delay-range" type="range" min="0" max="1000" step="1" value="${classForceSwitchDelayMs}">
          </div>
        </div>
      </div>
    `;
  }).join("");

  return `
    <div class="aim-profile-head">
      <strong>热键 ${index + 1}</strong>
      <button class="mini-danger" type="button" data-remove-profile>删除</button>
    </div>
    <div class="form-subgrid">
      <div class="aim-profile-hotkey-grid span-all">
        <div class="hotkey-pair-grid">
          <label class="field">主按键
            <select class="aim-profile-hotkey"></select>
          </label>
          <label class="field">副按键
            <select class="aim-profile-hotkey2"></select>
          </label>
        </div>
        <label class="field">触发方式
          <select class="aim-profile-hotkey-mode">
            <option value="any">任一按键</option>
            <option value="all">同时按下</option>
          </select>
          <span class="field-hint">副按键可选不使用；选择副按键后决定任一触发还是同时按下触发。</span>
        </label>
      </div>
      <div class="slider-field span-all">
        <div class="label-row">
          <label>热键移动倍率</label>
          <input class="aim-profile-sensitivity value-input" type="number" min="0.1" max="3" step="0.01" autocomplete="off" value="${sensitivity}">
        </div>
        <input class="aim-profile-sensitivity-range" type="range" min="0.1" max="3" step="0.01" value="${sensitivity}">
        <span class="field-hint">当前热键生效时，在全局移动倍率之后再乘这个速度倍率。</span>
      </div>
      <div class="slider-field span-all">
        <div class="label-row">
          <label>热键 FOV 缩放</label>
          <input class="aim-profile-fov-scale value-input" type="number" min="0.1" max="1" step="0.01" autocomplete="off" value="${fovScale}">
        </div>
        <input class="aim-profile-fov-scale-range" type="range" min="0.1" max="1" step="0.01" value="${fovScale}">
        <span class="field-hint">当前热键生效时，在总览 FOV 半径之后再乘这个倍率。</span>
      </div>
      <div class="aim-profile-axis-grid span-all">
        <div class="slider-field aim-profile-axis-field">
          <div class="label-row">
            <label>X轴偏移</label>
            <input class="aim-profile-offset-x value-input" type="number" min="0" max="1" step="0.01" autocomplete="off" value="${offsetX}">
          </div>
          <input class="aim-profile-offset-x-range" type="range" min="0" max="1" step="0.01" value="${offsetX}">
          <span class="field-hint aim-profile-axis-hint">0 左 · 0.5 中 · 1 右</span>
        </div>
        <div class="slider-field aim-profile-axis-field">
          <div class="label-row">
            <label>Y轴偏移</label>
            <input class="aim-profile-offset-y value-input" type="number" min="0" max="1" step="0.01" autocomplete="off" value="${offsetY}">
          </div>
          <input class="aim-profile-offset-y-range" type="range" min="0" max="1" step="0.01" value="${offsetY}">
          <span class="field-hint aim-profile-axis-hint">0 上 · 0.5 中 · 1 下</span>
        </div>
      </div>
      <div class="aim-profile-offset-switch span-all">
        <div class="aim-profile-offset-switch-head">
          <label class="check-line"><input class="aim-profile-offset-switch-enabled" type="checkbox" ${offsetSwitchChecked}> <span>偏移切换</span><span class="aim-profile-offset-switch-status" aria-live="polite">未切换</span></label>
          <label class="field aim-profile-offset-switch-hotkey-field" ${offsetSwitchPanelHidden}>切换热键
            <select class="aim-profile-offset-switch-hotkey"></select>
          </label>
        </div>
        <div class="aim-profile-axis-grid aim-profile-offset-switch-panel" ${offsetSwitchPanelHidden}>
          <div class="slider-field aim-profile-axis-field">
            <div class="label-row">
              <label>备用X轴偏移</label>
              <input class="aim-profile-alternate-offset-x value-input" type="number" min="0" max="1" step="0.01" autocomplete="off" value="${alternateOffsetX}">
            </div>
            <input class="aim-profile-alternate-offset-x-range" type="range" min="0" max="1" step="0.01" value="${alternateOffsetX}">
            <span class="field-hint aim-profile-axis-hint">0 左 · 0.5 中 · 1 右</span>
          </div>
          <div class="slider-field aim-profile-axis-field">
            <div class="label-row">
              <label>备用Y轴偏移</label>
              <input class="aim-profile-alternate-offset-y value-input" type="number" min="0" max="1" step="0.01" autocomplete="off" value="${alternateOffsetY}">
            </div>
            <input class="aim-profile-alternate-offset-y-range" type="range" min="0" max="1" step="0.01" value="${alternateOffsetY}">
            <span class="field-hint aim-profile-axis-hint">0 上 · 0.5 中 · 1 下</span>
          </div>
        </div>
      </div>
      <div class="class-picker span-all">
        <div class="class-picker-head">
          <span>目标类别 · ${classCount} 类</span>
          <button class="mini-button" type="button" data-select-all-classes>全选</button>
        </div>
        <div class="class-chip-grid">
          ${classPickers}
        </div>
      </div>
    </div>
  `;
}

function renumberAimProfiles() {
  document.querySelectorAll(".aim-profile-card").forEach((card, index) => {
    const title = card.querySelector(".aim-profile-head strong");
    if (title) {
      title.textContent = `热键 ${index + 1}`;
    }
  });
}

function updateClassToggleButton(card) {
  const button = card.querySelector("[data-select-all-classes]");
  if (!button) {
    return;
  }
  const checkboxes = Array.from(card.querySelectorAll(".aim-profile-class"));
  const allChecked = checkboxes.length > 0 && checkboxes.every((checkbox) => checkbox.checked);
  button.textContent = allChecked ? "取消全选" : "全选";
}

function bindProfileNumberRange(card, numberSelector, rangeSelector, fallback, min, max, digits = 2) {
  const range = card.querySelector(rangeSelector);
  const number = card.querySelector(numberSelector);
  if (!range || !number) {
    return;
  }
  enableThumbOnlyRangeInput(range);
  range.addEventListener("input", () => {
    number.value = range.value;
  });
  range.addEventListener("change", () => {
    number.value = range.value;
    requestConfigApply(70);
  });
  number.addEventListener("input", () => {
    if (number.value !== "") {
      range.value = number.value;
    }
  });
  number.addEventListener("keydown", (event) => {
    if (event.key === "Enter") {
      event.preventDefault();
      number.blur();
    }
  });
  number.addEventListener("change", () => {
    if (number.value !== "") {
      const formatted = clamp(Number(number.value || fallback), min, max).toFixed(digits);
      number.value = formatted;
      range.value = formatted;
    }
    requestConfigApply(90);
  });
}

function closeClassOffsetPopovers(scope = document, except = null) {
  const root = scope && scope.querySelectorAll ? scope : document;
  root.querySelectorAll(".class-offset-popover").forEach((popover) => {
    if (popover === except) {
      return;
    }
    popover.hidden = true;
    const chip = popover.closest(".class-chip");
    if (chip) {
      chip.classList.remove("class-offset-open");
    }
  });
}

function setClassOffsetChipEnabled(chip, enabled) {
  chip.dataset.classOffsetEnabled = enabled ? "1" : "0";
  chip.classList.toggle("has-class-offset", enabled);
  const button = chip.querySelector(".class-offset-button");
  if (button) {
    button.setAttribute("aria-pressed", enabled ? "true" : "false");
  }
}

function profileDefaultOffsetFromCard(card, axis) {
  const selector = axis === "x" ? ".aim-profile-offset-x" : ".aim-profile-offset-y";
  const input = card.querySelector(selector);
  return clampAimProfileOffset(input ? input.value : 0.5, 0.5);
}

function setClassOffsetChipValue(chip, axis, value) {
  const formatted = clampAimProfileOffset(value, 0.5).toFixed(2);
  const number = chip.querySelector(`.aim-profile-class-offset-${axis}`);
  const range = chip.querySelector(`.aim-profile-class-offset-${axis}-range`);
  if (number) {
    number.value = formatted;
  }
  if (range) {
    range.value = formatted;
  }
  if (axis === "x") {
    chip.dataset.classOffsetX = formatted;
  } else {
    chip.dataset.classOffsetY = formatted;
  }
}

function setClassPriorityChipValue(chip, value) {
  const priority = clampAimClassPriority(value, 0);
  const input = chip.querySelector(".aim-profile-class-priority");
  const range = chip.querySelector(".aim-profile-class-priority-range");
  if (input) {
    input.value = String(priority);
  }
  if (range) {
    range.value = String(priority);
  }
  chip.dataset.classPriority = String(priority);
}

function setClassForceSwitchDelayChipValue(chip, value) {
  const delayMs = clampAimClassForceSwitchDelay(value, 30);
  const input = chip.querySelector(".aim-profile-class-force-delay");
  const range = chip.querySelector(".aim-profile-class-force-delay-range");
  if (input) {
    input.value = String(delayMs);
  }
  if (range) {
    range.value = String(delayMs);
  }
  chip.dataset.classForceSwitchDelayMs = String(delayMs);
}

function setClassForcePrioritySwitchValue(chip, enabled) {
  const forceSwitch = Boolean(enabled);
  const input = chip.querySelector(".aim-profile-class-force-switch");
  const delayField = chip.querySelector(".class-force-switch-delay-field");
  if (input) {
    input.checked = forceSwitch;
  }
  if (delayField) {
    delayField.hidden = !forceSwitch;
  }
  chip.dataset.classForcePrioritySwitch = forceSwitch ? "1" : "0";
}

function resetClassOffsetChipToProfileOffset(card, chip) {
  setClassOffsetChipValue(chip, "x", profileDefaultOffsetFromCard(card, "x"));
  setClassOffsetChipValue(chip, "y", profileDefaultOffsetFromCard(card, "y"));
  setClassPriorityChipValue(chip, 0);
  setClassForcePrioritySwitchValue(chip, false);
  setClassForceSwitchDelayChipValue(chip, 30);
  setClassOffsetChipEnabled(chip, false);
}

function updateClassOffsetChipDataset(chip, axis, value, fallback) {
  const number = Number(value);
  if (!Number.isFinite(number)) {
    return false;
  }
  const formatted = clampAimProfileOffset(number, fallback).toFixed(2);
  if (axis === "x") {
    chip.dataset.classOffsetX = formatted;
  } else {
    chip.dataset.classOffsetY = formatted;
  }
  return true;
}

function bindClassOffsetAxis(card, chip, axis) {
  const number = chip.querySelector(`.aim-profile-class-offset-${axis}`);
  const range = chip.querySelector(`.aim-profile-class-offset-${axis}-range`);
  if (!number || !range) {
    return;
  }
  enableThumbOnlyRangeInput(range);
  const enableWithValue = (value) => {
    const fallback = profileDefaultOffsetFromCard(card, axis);
    updateClassOffsetChipDataset(chip, axis, value, fallback);
    setClassOffsetChipEnabled(chip, true);
  };
  range.addEventListener("input", () => {
    setClassOffsetChipValue(chip, axis, range.value);
    setClassOffsetChipEnabled(chip, true);
  });
  range.addEventListener("change", () => {
    setClassOffsetChipValue(chip, axis, range.value);
    setClassOffsetChipEnabled(chip, true);
    requestConfigApply(70);
  });
  number.addEventListener("input", () => {
    if (number.value !== "") {
      range.value = number.value;
    }
    if (!isPendingNumberText(number.value)) {
      enableWithValue(number.value);
    }
  });
  number.addEventListener("keydown", (event) => {
    if (event.key === "Enter") {
      event.preventDefault();
      number.blur();
    }
  });
  number.addEventListener("change", () => {
    const fallback = profileDefaultOffsetFromCard(card, axis);
    const formatted = clampAimProfileOffset(number.value, fallback).toFixed(2);
    number.value = formatted;
    range.value = formatted;
    enableWithValue(formatted);
    requestConfigApply(90);
  });
}

function bindClassOffsetChip(card, chip) {
  if (chip.dataset.classOffsetBound === "1") {
    return;
  }
  chip.dataset.classOffsetBound = "1";
  const button = chip.querySelector(".class-offset-button");
  const popover = chip.querySelector(".class-offset-popover");
  const clearButton = chip.querySelector(".class-offset-clear");
  if (!button || !popover) {
    return;
  }
  button.setAttribute("aria-pressed", chip.dataset.classOffsetEnabled === "1" ? "true" : "false");
  bindClassOffsetAxis(card, chip, "x");
  bindClassOffsetAxis(card, chip, "y");
  const priorityInput = chip.querySelector(".aim-profile-class-priority");
  const priorityRange = chip.querySelector(".aim-profile-class-priority-range");
  if (priorityInput && priorityRange) {
    enableThumbOnlyRangeInput(priorityRange);
    priorityRange.addEventListener("input", () => {
      setClassPriorityChipValue(chip, priorityRange.value);
      setClassOffsetChipEnabled(chip, true);
    });
    priorityRange.addEventListener("change", () => {
      setClassPriorityChipValue(chip, priorityRange.value);
      setClassOffsetChipEnabled(chip, true);
      requestConfigApply(70);
    });
    priorityInput.addEventListener("input", () => {
      if (priorityInput.value !== "") {
        priorityRange.value = priorityInput.value;
      }
      if (!isPendingNumberText(priorityInput.value)) {
        chip.dataset.classPriority = String(clampAimClassPriority(priorityInput.value, 0));
        setClassOffsetChipEnabled(chip, true);
      }
    });
    priorityInput.addEventListener("keydown", (event) => {
      if (event.key === "Enter") {
        event.preventDefault();
        priorityInput.blur();
      }
    });
    priorityInput.addEventListener("change", () => {
      setClassPriorityChipValue(chip, priorityInput.value);
      setClassOffsetChipEnabled(chip, true);
      requestConfigApply(80);
    });
  }
  const forceSwitchInput = chip.querySelector(".aim-profile-class-force-switch");
  const forceDelayInput = chip.querySelector(".aim-profile-class-force-delay");
  const forceDelayRange = chip.querySelector(".aim-profile-class-force-delay-range");
  if (forceSwitchInput && forceDelayInput && forceDelayRange) {
    const syncForceSwitchState = () => {
      setClassForcePrioritySwitchValue(chip, forceSwitchInput.checked);
      setClassOffsetChipEnabled(chip, true);
    };
    enableThumbOnlyRangeInput(forceDelayRange);
    forceSwitchInput.addEventListener("change", () => {
      syncForceSwitchState();
      requestConfigApply(80);
    });
    forceDelayRange.addEventListener("input", () => {
      setClassForceSwitchDelayChipValue(chip, forceDelayRange.value);
      setClassOffsetChipEnabled(chip, true);
    });
    forceDelayRange.addEventListener("change", () => {
      setClassForceSwitchDelayChipValue(chip, forceDelayRange.value);
      setClassOffsetChipEnabled(chip, true);
      requestConfigApply(70);
    });
    forceDelayInput.addEventListener("input", () => {
      if (forceDelayInput.value !== "") {
        forceDelayRange.value = forceDelayInput.value;
      }
      if (!isPendingNumberText(forceDelayInput.value)) {
        chip.dataset.classForceSwitchDelayMs = String(clampAimClassForceSwitchDelay(forceDelayInput.value, 30));
        setClassOffsetChipEnabled(chip, true);
      }
    });
    forceDelayInput.addEventListener("keydown", (event) => {
      if (event.key === "Enter") {
        event.preventDefault();
        forceDelayInput.blur();
      }
    });
    forceDelayInput.addEventListener("change", () => {
      setClassForceSwitchDelayChipValue(chip, forceDelayInput.value);
      setClassOffsetChipEnabled(chip, true);
      requestConfigApply(80);
    });
    setClassForcePrioritySwitchValue(chip, forceSwitchInput.checked);
  }
  button.addEventListener("click", (event) => {
    event.preventDefault();
    event.stopPropagation();
    const shouldOpen = popover.hidden;
    closeClassOffsetPopovers(document, shouldOpen ? popover : null);
    if (shouldOpen) {
      if (chip.dataset.classOffsetEnabled !== "1") {
        resetClassOffsetChipToProfileOffset(card, chip);
      }
      popover.hidden = false;
      chip.classList.add("class-offset-open");
    } else {
      popover.hidden = true;
      chip.classList.remove("class-offset-open");
    }
  });
  popover.addEventListener("click", (event) => {
    event.stopPropagation();
  });
  if (clearButton) {
    clearButton.addEventListener("click", (event) => {
      event.preventDefault();
      event.stopPropagation();
      resetClassOffsetChipToProfileOffset(card, chip);
      requestConfigApply(80);
    });
  }
}

function bindAimProfileCard(card, profile) {
  const hotkeySelect = card.querySelector(".aim-profile-hotkey");
  const hotkey2Select = card.querySelector(".aim-profile-hotkey2");
  const hotkeyModeSelect = card.querySelector(".aim-profile-hotkey-mode");
  fillOptions(hotkeySelect, HOTKEYS);
  fillOptions(hotkey2Select, OPTIONAL_POINTER_HOTKEYS);
  hotkeySelect.value = profile && HOTKEYS.includes(profile.hotkey) ? profile.hotkey : "left";
  hotkey2Select.value = profile && OPTIONAL_POINTER_HOTKEYS.includes(profile.hotkey2) ? profile.hotkey2 : "";
  hotkeyModeSelect.value = profile && profile.hotkey_mode === "all" ? "all" : "any";
  hotkeySelect.addEventListener("change", () => requestConfigApply(80));
  hotkey2Select.addEventListener("change", () => requestConfigApply(80));
  hotkeyModeSelect.addEventListener("change", () => requestConfigApply(80));

  const offsetSwitchToggle = card.querySelector(".aim-profile-offset-switch-enabled");
  const offsetSwitchHotkey = card.querySelector(".aim-profile-offset-switch-hotkey");
  const offsetSwitchHotkeyField = card.querySelector(".aim-profile-offset-switch-hotkey-field");
  const offsetSwitchPanel = card.querySelector(".aim-profile-offset-switch-panel");
  fillOptions(offsetSwitchHotkey, OPTIONAL_POINTER_HOTKEYS);
  offsetSwitchHotkey.value = aimProfileOffsetSwitchHotkey(profile);
  const syncOffsetSwitchPanel = () => {
    const enabled = Boolean(offsetSwitchToggle && offsetSwitchToggle.checked);
    if (offsetSwitchPanel) {
      offsetSwitchPanel.hidden = !enabled;
    }
    if (offsetSwitchHotkeyField) {
      offsetSwitchHotkeyField.hidden = !enabled;
    }
    if (offsetSwitchHotkey) {
      offsetSwitchHotkey.disabled = !enabled;
    }
    card.classList.toggle("offset-switch-enabled", enabled);
    updateAimProfileOffsetSwitchStatus(state.data && state.data.state && state.data.state.aim || {});
  };
  if (offsetSwitchToggle) {
    offsetSwitchToggle.addEventListener("change", () => {
      syncOffsetSwitchPanel();
      requestConfigApply(80);
    });
  }
  if (offsetSwitchHotkey) {
    offsetSwitchHotkey.addEventListener("change", () => requestConfigApply(80));
  }

  bindProfileNumberRange(card, ".aim-profile-offset-x", ".aim-profile-offset-x-range", 0.5, 0, 1);
  bindProfileNumberRange(card, ".aim-profile-offset-y", ".aim-profile-offset-y-range", 0.5, 0, 1);
  bindProfileNumberRange(card, ".aim-profile-alternate-offset-x", ".aim-profile-alternate-offset-x-range", aimProfileOffsetX(profile), 0, 1);
  bindProfileNumberRange(card, ".aim-profile-alternate-offset-y", ".aim-profile-alternate-offset-y-range", aimProfileOffsetY(profile), 0, 1);
  bindProfileNumberRange(card, ".aim-profile-sensitivity", ".aim-profile-sensitivity-range", 1, 0.1, 3);
  bindProfileNumberRange(card, ".aim-profile-fov-scale", ".aim-profile-fov-scale-range", 1, 0.1, 1);
  syncOffsetSwitchPanel();

  card.querySelectorAll(".aim-profile-class").forEach((checkbox) => {
    checkbox.addEventListener("change", () => {
      updateClassToggleButton(card);
      requestConfigApply(80);
    });
  });
  card.querySelectorAll(".class-chip").forEach((chip) => {
    bindClassOffsetChip(card, chip);
  });

  card.querySelector("[data-remove-profile]").addEventListener("click", () => {
    card.remove();
    renumberAimProfiles();
    requestConfigApply(80);
  });

  card.querySelector("[data-select-all-classes]").addEventListener("click", () => {
    const checkboxes = Array.from(card.querySelectorAll(".aim-profile-class"));
    const shouldCheck = !checkboxes.every((checkbox) => checkbox.checked);
    checkboxes.forEach((checkbox) => {
      checkbox.checked = shouldCheck;
    });
    updateClassToggleButton(card);
    requestConfigApply(80);
  });
  updateClassToggleButton(card);
}

function addAimProfileCard(profile = {}, emitChange = true) {
  const editor = $("aimProfilesEditor");
  if (!editor) {
    return;
  }
  const index = editor.querySelectorAll(".aim-profile-card").length;
  const card = document.createElement("div");
  card.className = "aim-profile-card";
  card.innerHTML = profileTemplate(profile, index);
  editor.appendChild(card);
  bindAimProfileCard(card, profile);
  if (emitChange && state.configReady) {
    requestConfigApply(80);
  }
}

function renderAimProfiles(profiles) {
  const editor = $("aimProfilesEditor");
  if (!editor) {
    return;
  }
  editor.innerHTML = "";
  const safeProfiles = Array.isArray(profiles) && profiles.length > 0
    ? profiles
    : [{
      hotkey: "left",
      class_filter_mask: currentModelClassMask(),
      offset_x: defaultAimProfileOffsetX(),
      offset_y: defaultAimProfileOffsetY(),
      sensitivity: 1,
      fov_scale: 1,
    }];
  safeProfiles.forEach((profile) => addAimProfileCard(profile, false));
  state.aimClassRenderSignature = currentModelClassRenderSignature();
}

function defaultAimProfileOffsetX() {
  const firstProfileOffsetX = document.querySelector(".aim-profile-offset-x");
  if (firstProfileOffsetX && firstProfileOffsetX.value !== "") {
    const value = Number(firstProfileOffsetX.value);
    if (Number.isFinite(value)) {
      return clampAimProfileOffset(value, 0.5);
    }
  }
  return 0.5;
}

function defaultAimProfileOffsetY() {
  const firstProfileOffsetY = document.querySelector(".aim-profile-offset-y");
  if (firstProfileOffsetY && firstProfileOffsetY.value !== "") {
    const value = Number(firstProfileOffsetY.value);
    if (Number.isFinite(value)) {
      return clampAimProfileOffset(value, 0.5);
    }
  }
  return legacyAimPosToOffsetY(getNumber("pos", 0.5));
}

function collectAimProfiles() {
  const editor = $("aimProfilesEditor");
  if (!editor) {
    return [{
      hotkey: "left",
      class_filter_mask: currentModelClassMask(),
      offset_x: defaultAimProfileOffsetX(),
      offset_y: defaultAimProfileOffsetY(),
      class_offsets: [],
      sensitivity: 1,
      fov_scale: 1,
    }];
  }
  const profiles = Array.from(editor.querySelectorAll(".aim-profile-card")).map((card) => {
    let mask = 0;
    card.querySelectorAll(".aim-profile-class").forEach((checkbox) => {
      if (checkbox.checked) {
        mask |= 1 << Number(checkbox.dataset.classId);
      }
    });
    const offsetX = clampAimProfileOffset(card.querySelector(".aim-profile-offset-x").value, 0.5);
    const offsetY = clampAimProfileOffset(card.querySelector(".aim-profile-offset-y").value, 0.5);
    const alternateOffsetX = clampAimProfileOffset(
      card.querySelector(".aim-profile-alternate-offset-x").value,
      offsetX
    );
    const alternateOffsetY = clampAimProfileOffset(
      card.querySelector(".aim-profile-alternate-offset-y").value,
      offsetY
    );
    const offsetSwitchHotkey = card.querySelector(".aim-profile-offset-switch-hotkey").value || "";
    const classOffsets = Array.from(card.querySelectorAll(".class-chip[data-class-id]"))
      .filter((chip) => chip.dataset.classOffsetEnabled === "1")
      .map((chip) => {
        const classId = Math.floor(Number(chip.dataset.classId));
        if (!Number.isFinite(classId) || classId < 0 || classId >= currentModelClassCount()) {
          return null;
        }
        return {
          class_id: classId,
          offset_x: clampAimProfileOffset(chip.dataset.classOffsetX, offsetX),
          offset_y: clampAimProfileOffset(chip.dataset.classOffsetY, offsetY),
          priority: clampAimClassPriority(chip.dataset.classPriority, 0),
          force_priority_switch: chip.dataset.classForcePrioritySwitch === "1",
          force_switch_delay_ms: clampAimClassForceSwitchDelay(chip.dataset.classForceSwitchDelayMs, 30),
        };
      })
      .filter(Boolean);
    return {
      hotkey: card.querySelector(".aim-profile-hotkey").value || "left",
      hotkey2: card.querySelector(".aim-profile-hotkey2").value || "",
      hotkey_mode: card.querySelector(".aim-profile-hotkey-mode").value || "any",
      class_filter_mask: mask & currentModelClassMask(),
      offset_x: offsetX,
      offset_y: offsetY,
      pos: aimOffsetYToLegacyPos(offsetY),
      offset_switch_enabled: Boolean(card.querySelector(".aim-profile-offset-switch-enabled").checked),
      offset_switch_hotkey: OPTIONAL_POINTER_HOTKEYS.includes(offsetSwitchHotkey) ? offsetSwitchHotkey : "",
      alternate_offset_x: alternateOffsetX,
      alternate_offset_y: alternateOffsetY,
      class_offsets: classOffsets,
      sensitivity: clamp(Number(card.querySelector(".aim-profile-sensitivity").value || 1), 0.1, 3),
      fov_scale: aimProfileFovScale({
        fov_scale: card.querySelector(".aim-profile-fov-scale").value || 1,
      }),
    };
  });

  if (profiles.length === 0) {
    return [{
      hotkey: "left",
      class_filter_mask: currentModelClassMask(),
      offset_x: defaultAimProfileOffsetX(),
      offset_y: defaultAimProfileOffsetY(),
      class_offsets: [],
      sensitivity: 1,
      fov_scale: 1,
    }];
  }
  return profiles;
}

function defaultAutoTriggerProfile() {
  return {
    hotkey: AUTO_TRIGGER_DEFAULTS.activation_hotkey,
    class_filter_mask: currentModelClassMask(),
    fire_mode: AUTO_TRIGGER_DEFAULTS.fire_mode,
    enter_delay_min_ms: AUTO_TRIGGER_DEFAULTS.enter_delay_min_ms,
    enter_delay_max_ms: AUTO_TRIGGER_DEFAULTS.enter_delay_max_ms,
    cooldown_min_ms: AUTO_TRIGGER_DEFAULTS.cooldown_min_ms,
    cooldown_max_ms: AUTO_TRIGGER_DEFAULTS.cooldown_max_ms,
    spray_release_delay_ms: AUTO_TRIGGER_DEFAULTS.spray_release_delay_ms,
    continuous_mode: AUTO_TRIGGER_DEFAULTS.continuous_mode,
    spray_recoil_assist: AUTO_TRIGGER_DEFAULTS.spray_recoil_assist,
  };
}

function autoTriggerProfileTimingValue(profile, key, fallback, min, max) {
  const value = Number(profile && profile[key] !== undefined ? profile[key] : fallback);
  return Math.round(clamp(Number.isFinite(value) ? value : fallback, min, max));
}

function normalizeAutoTriggerFireMode(mode) {
  return AUTO_TRIGGER_FIRE_MODES.includes(mode) ? mode : AUTO_TRIGGER_DEFAULTS.fire_mode;
}

function autoTriggerProfilesFromConfig(autoTrigger) {
  if (!autoTrigger) {
    return [];
  }
  if (Object.prototype.hasOwnProperty.call(autoTrigger, "enabled") && autoTrigger.enabled !== true) {
    return [];
  }
  if (Array.isArray(autoTrigger.profiles)) {
    return autoTrigger.profiles.map((profile) => ({
      ...defaultAutoTriggerProfile(),
      enter_delay_min_ms: autoTrigger.enter_delay_min_ms ?? AUTO_TRIGGER_DEFAULTS.enter_delay_min_ms,
      enter_delay_max_ms: autoTrigger.enter_delay_max_ms ?? AUTO_TRIGGER_DEFAULTS.enter_delay_max_ms,
      cooldown_min_ms: autoTrigger.cooldown_min_ms ?? AUTO_TRIGGER_DEFAULTS.cooldown_min_ms,
      cooldown_max_ms: autoTrigger.cooldown_max_ms ?? AUTO_TRIGGER_DEFAULTS.cooldown_max_ms,
      spray_release_delay_ms:
        autoTrigger.spray_release_delay_ms ?? AUTO_TRIGGER_DEFAULTS.spray_release_delay_ms,
      fire_mode: autoTrigger.fire_mode ?? AUTO_TRIGGER_DEFAULTS.fire_mode,
      continuous_mode: autoTrigger.continuous_mode ?? AUTO_TRIGGER_DEFAULTS.continuous_mode,
      spray_recoil_assist: autoTrigger.spray_recoil_assist ?? AUTO_TRIGGER_DEFAULTS.spray_recoil_assist,
      ...profile,
    }));
  }
  const legacyHotkey = autoTrigger.activation_hotkey
    ? autoTrigger.activation_hotkey
    : AUTO_TRIGGER_DEFAULTS.activation_hotkey;
  return [{
    ...defaultAutoTriggerProfile(),
    hotkey: AUTO_TRIGGER_HOTKEYS.includes(legacyHotkey) ? legacyHotkey : AUTO_TRIGGER_DEFAULTS.activation_hotkey,
  }];
}

function autoTriggerProfileTemplate(profile, index) {
  const classCount = currentModelClassCount();
  const classNames = currentModelClassNames();
  const mask = classMaskFromProfile(profile);
  const enterDelayMin = autoTriggerProfileTimingValue(
    profile,
    "enter_delay_min_ms",
    AUTO_TRIGGER_DEFAULTS.enter_delay_min_ms,
    0,
    500
  );
  const enterDelayMax = Math.max(
    enterDelayMin,
    autoTriggerProfileTimingValue(profile, "enter_delay_max_ms", AUTO_TRIGGER_DEFAULTS.enter_delay_max_ms, 0, 500)
  );
  const cooldownMin = autoTriggerProfileTimingValue(
    profile,
    "cooldown_min_ms",
    AUTO_TRIGGER_DEFAULTS.cooldown_min_ms,
    50,
    3000
  );
  const cooldownMax = Math.max(
    cooldownMin,
    autoTriggerProfileTimingValue(profile, "cooldown_max_ms", AUTO_TRIGGER_DEFAULTS.cooldown_max_ms, 50, 3000)
  );
  const fireMode = normalizeAutoTriggerFireMode(profile && profile.fire_mode);
  const sprayReleaseDelayMs = autoTriggerProfileTimingValue(
    profile,
    "spray_release_delay_ms",
    AUTO_TRIGGER_DEFAULTS.spray_release_delay_ms,
    0,
    5000
  );
  const continuousChecked = profile && profile.continuous_mode ? "checked" : "";
  const sprayRecoilAssistChecked = profile && profile.spray_recoil_assist ? "checked" : "";
  const classPickers = Array.from({ length: classCount }, (_, classId) => {
    const checked = mask & (1 << classId) ? "checked" : "";
    const className = classDisplayName(classId, classNames);
    const classLabel = `${classId} ${className}`;
    return `
      <label class="class-chip" title="${escapeAttr(classLabel)}">
        <input class="aim-profile-class auto-trigger-profile-class" type="checkbox" data-class-id="${classId}" ${checked}>
        <span class="class-chip-name">${escapeHtml(classLabel)}</span>
      </label>
    `;
  }).join("");

  return `
    <div class="aim-profile-head auto-trigger-profile-head">
      <strong>开火配置 ${index + 1}</strong>
      <button class="mini-danger" type="button" data-remove-auto-trigger-profile>删除</button>
    </div>
    <div class="form-subgrid">
      <label class="field">按住热键
        <select class="auto-trigger-profile-hotkey"></select>
      </label>
      <label class="field">开火模式
        <select class="auto-trigger-profile-fire-mode">
          <option value="tap" ${fireMode === "tap" ? "selected" : ""}>点射</option>
          <option value="spray" ${fireMode === "spray" ? "selected" : ""}>扫射</option>
        </select>
      </label>
      <div class="class-picker span-all">
        <div class="class-picker-head">
          <span>目标类别 · ${classCount} 类</span>
          <button class="mini-button" type="button" data-select-all-classes>全选</button>
        </div>
        <div class="class-chip-grid">
          ${classPickers}
        </div>
      </div>
      <div class="field-grid compact auto-trigger-profile-timing-grid span-all">
        <div class="slider-field">
          <div class="label-row">
            <label>进入延迟下限</label>
            <input class="auto-trigger-profile-enter-delay-min-ms value-input" type="number" min="0" max="500" step="1" autocomplete="off" value="${enterDelayMin}">
          </div>
          <input class="auto-trigger-profile-enter-delay-min-ms-range" type="range" min="0" max="500" step="1" value="${enterDelayMin}">
          <span class="field-hint">准星进入触发范围后，最少等待多少毫秒再按左键。</span>
        </div>
        <div class="slider-field">
          <div class="label-row">
            <label>进入延迟上限</label>
            <input class="auto-trigger-profile-enter-delay-max-ms value-input" type="number" min="0" max="500" step="1" autocomplete="off" value="${enterDelayMax}">
          </div>
          <input class="auto-trigger-profile-enter-delay-max-ms-range" type="range" min="0" max="500" step="1" value="${enterDelayMax}">
          <span class="field-hint">准星进入触发范围后，最多随机等待多少毫秒再按左键。</span>
        </div>
        <div class="slider-field auto-trigger-profile-cooldown-field">
          <div class="label-row">
            <label>冷却下限</label>
            <input class="auto-trigger-profile-cooldown-min-ms value-input" type="number" min="50" max="3000" step="1" autocomplete="off" value="${cooldownMin}">
          </div>
          <input class="auto-trigger-profile-cooldown-min-ms-range" type="range" min="50" max="3000" step="1" value="${cooldownMin}">
          <span class="field-hint">两次自动开火之间的最短间隔，数值越低反应越快。</span>
        </div>
        <div class="slider-field auto-trigger-profile-cooldown-field">
          <div class="label-row">
            <label>冷却上限</label>
            <input class="auto-trigger-profile-cooldown-max-ms value-input" type="number" min="50" max="3000" step="1" autocomplete="off" value="${cooldownMax}">
          </div>
          <input class="auto-trigger-profile-cooldown-max-ms-range" type="range" min="50" max="3000" step="1" value="${cooldownMax}">
          <span class="field-hint">两次自动开火之间的最长随机间隔，数值越高越不固定。</span>
        </div>
      </div>
      <div class="check-field span-all auto-trigger-profile-continuous-field">
        <label class="check-line"><input class="auto-trigger-profile-continuous-mode" type="checkbox" ${continuousChecked}> 连续点射</label>
        <span class="field-hint">目标持续在触发范围内时，会按本配置的冷却间隔重复开火。</span>
      </div>
      <div class="check-field span-all auto-trigger-profile-spray-recoil-field">
        <label class="check-line"><input class="auto-trigger-profile-spray-recoil-assist" type="checkbox" ${sprayRecoilAssistChecked}> 自动扳机扫射带压枪</label>
        <span class="field-hint">开启后，本配置扫射按住左键期间也会触发压枪。</span>
      </div>
      <div class="slider-field span-all auto-trigger-profile-spray-release-field">
        <div class="label-row">
          <label>扫射释放延迟</label>
          <input class="auto-trigger-profile-spray-release-delay-ms value-input" type="number" min="0" max="5000" step="1" autocomplete="off" value="${sprayReleaseDelayMs}">
        </div>
        <input class="auto-trigger-profile-spray-release-delay-ms-range" type="range" min="0" max="5000" step="1" value="${sprayReleaseDelayMs}">
        <span class="field-hint">扫射按住左键后，准星短暂离开目标时等待多久再松开。</span>
      </div>
    </div>
  `;
}

function renumberAutoTriggerProfiles() {
  document.querySelectorAll(".auto-trigger-profile-card").forEach((card, index) => {
    const title = card.querySelector(".auto-trigger-profile-head strong");
    if (title) {
      title.textContent = `开火配置 ${index + 1}`;
    }
  });
}

function updateAutoTriggerProfileModeVisibility(card) {
  const mode = normalizeAutoTriggerFireMode(card.querySelector(".auto-trigger-profile-fire-mode")?.value);
  const tapMode = mode === "tap";
  card.querySelectorAll(".auto-trigger-profile-cooldown-field").forEach((field) => {
    field.hidden = !tapMode;
  });
  const continuousField = card.querySelector(".auto-trigger-profile-continuous-field");
  if (continuousField) {
    continuousField.hidden = !tapMode;
  }
  const sprayRecoilField = card.querySelector(".auto-trigger-profile-spray-recoil-field");
  if (sprayRecoilField) {
    sprayRecoilField.hidden = tapMode;
  }
  const sprayReleaseField = card.querySelector(".auto-trigger-profile-spray-release-field");
  if (sprayReleaseField) {
    sprayReleaseField.hidden = tapMode;
  }
}

function bindAutoTriggerProfileCard(card, profile) {
  const hotkeySelect = card.querySelector(".auto-trigger-profile-hotkey");
  fillOptions(hotkeySelect, AUTO_TRIGGER_HOTKEYS);
  hotkeySelect.value = profile && AUTO_TRIGGER_HOTKEYS.includes(profile.hotkey)
    ? profile.hotkey
    : AUTO_TRIGGER_DEFAULTS.activation_hotkey;
  hotkeySelect.addEventListener("change", () => requestConfigApply(80));

  const fireModeSelect = card.querySelector(".auto-trigger-profile-fire-mode");
  if (fireModeSelect) {
    fireModeSelect.value = normalizeAutoTriggerFireMode(profile && profile.fire_mode);
    fireModeSelect.addEventListener("change", () => {
      updateAutoTriggerProfileModeVisibility(card);
      requestConfigApply(80);
    });
  }

  bindProfileNumberRange(
    card,
    ".auto-trigger-profile-enter-delay-min-ms",
    ".auto-trigger-profile-enter-delay-min-ms-range",
    AUTO_TRIGGER_DEFAULTS.enter_delay_min_ms,
    0,
    500,
    0
  );
  bindProfileNumberRange(
    card,
    ".auto-trigger-profile-enter-delay-max-ms",
    ".auto-trigger-profile-enter-delay-max-ms-range",
    AUTO_TRIGGER_DEFAULTS.enter_delay_max_ms,
    0,
    500,
    0
  );
  bindProfileNumberRange(
    card,
    ".auto-trigger-profile-cooldown-min-ms",
    ".auto-trigger-profile-cooldown-min-ms-range",
    AUTO_TRIGGER_DEFAULTS.cooldown_min_ms,
    50,
    3000,
    0
  );
  bindProfileNumberRange(
    card,
    ".auto-trigger-profile-cooldown-max-ms",
    ".auto-trigger-profile-cooldown-max-ms-range",
    AUTO_TRIGGER_DEFAULTS.cooldown_max_ms,
    50,
    3000,
    0
  );
  bindProfileNumberRange(
    card,
    ".auto-trigger-profile-spray-release-delay-ms",
    ".auto-trigger-profile-spray-release-delay-ms-range",
    AUTO_TRIGGER_DEFAULTS.spray_release_delay_ms,
    0,
    5000,
    0
  );

  card.querySelectorAll(".auto-trigger-profile-class").forEach((checkbox) => {
    checkbox.addEventListener("change", () => {
      updateClassToggleButton(card);
      requestConfigApply(80);
    });
  });

  card.querySelector("[data-remove-auto-trigger-profile]").addEventListener("click", () => {
    card.remove();
    renumberAutoTriggerProfiles();
    requestConfigApply(80);
  });

  card.querySelector("[data-select-all-classes]").addEventListener("click", () => {
    const checkboxes = Array.from(card.querySelectorAll(".auto-trigger-profile-class"));
    const shouldCheck = !checkboxes.every((checkbox) => checkbox.checked);
    checkboxes.forEach((checkbox) => {
      checkbox.checked = shouldCheck;
    });
    updateClassToggleButton(card);
    requestConfigApply(80);
  });
  const continuousMode = card.querySelector(".auto-trigger-profile-continuous-mode");
  if (continuousMode) {
    continuousMode.addEventListener("change", () => requestConfigApply(80));
  }
  const sprayRecoilAssist = card.querySelector(".auto-trigger-profile-spray-recoil-assist");
  if (sprayRecoilAssist) {
    sprayRecoilAssist.addEventListener("change", () => requestConfigApply(80));
  }
  updateAutoTriggerProfileModeVisibility(card);
  updateClassToggleButton(card);
}

function addAutoTriggerProfileCard(profile = {}, emitChange = true) {
  const editor = $("autoTriggerProfilesEditor");
  if (!editor) {
    return;
  }
  const index = editor.querySelectorAll(".auto-trigger-profile-card").length;
  const card = document.createElement("div");
  card.className = "aim-profile-card auto-trigger-profile-card";
  card.innerHTML = autoTriggerProfileTemplate(profile, index);
  editor.appendChild(card);
  bindAutoTriggerProfileCard(card, profile);
  if (emitChange && state.configReady) {
    requestConfigApply(80);
  }
}

function renderAutoTriggerProfiles(autoTrigger) {
  const editor = $("autoTriggerProfilesEditor");
  if (!editor) {
    return;
  }
  editor.innerHTML = "";
  autoTriggerProfilesFromConfig(autoTrigger).forEach((profile) => addAutoTriggerProfileCard(profile, false));
  state.autoTriggerClassRenderSignature = currentModelClassRenderSignature();
}

function collectAutoTriggerProfileInt(card, selector, fallback, min, max) {
  const field = card.querySelector(selector);
  const value = Number(field ? field.value : fallback);
  return Math.round(clamp(Number.isFinite(value) ? value : fallback, min, max));
}

function collectAutoTriggerProfiles() {
  const editor = $("autoTriggerProfilesEditor");
  if (!editor) {
    return [];
  }
  return Array.from(editor.querySelectorAll(".auto-trigger-profile-card")).map((card) => {
    let mask = 0;
    card.querySelectorAll(".auto-trigger-profile-class").forEach((checkbox) => {
      if (checkbox.checked) {
        mask |= 1 << Number(checkbox.dataset.classId);
      }
    });
    const enterDelayMin = collectAutoTriggerProfileInt(
      card,
      ".auto-trigger-profile-enter-delay-min-ms",
      AUTO_TRIGGER_DEFAULTS.enter_delay_min_ms,
      0,
      500
    );
    const enterDelayMax = Math.max(
      enterDelayMin,
      collectAutoTriggerProfileInt(
        card,
        ".auto-trigger-profile-enter-delay-max-ms",
        AUTO_TRIGGER_DEFAULTS.enter_delay_max_ms,
        0,
        500
      )
    );
    const cooldownMin = collectAutoTriggerProfileInt(
      card,
      ".auto-trigger-profile-cooldown-min-ms",
      AUTO_TRIGGER_DEFAULTS.cooldown_min_ms,
      50,
      3000
    );
    const cooldownMax = Math.max(
      cooldownMin,
      collectAutoTriggerProfileInt(
        card,
        ".auto-trigger-profile-cooldown-max-ms",
        AUTO_TRIGGER_DEFAULTS.cooldown_max_ms,
        50,
        3000
      )
    );
    const fireMode = normalizeAutoTriggerFireMode(card.querySelector(".auto-trigger-profile-fire-mode")?.value);
    const sprayReleaseDelayMs = collectAutoTriggerProfileInt(
      card,
      ".auto-trigger-profile-spray-release-delay-ms",
      AUTO_TRIGGER_DEFAULTS.spray_release_delay_ms,
      0,
      5000
    );
    return {
      hotkey: card.querySelector(".auto-trigger-profile-hotkey").value || AUTO_TRIGGER_DEFAULTS.activation_hotkey,
      class_filter_mask: mask & currentModelClassMask(),
      fire_mode: fireMode,
      enter_delay_min_ms: enterDelayMin,
      enter_delay_max_ms: enterDelayMax,
      cooldown_min_ms: cooldownMin,
      cooldown_max_ms: cooldownMax,
      spray_release_delay_ms: sprayReleaseDelayMs,
      continuous_mode: Boolean(card.querySelector(".auto-trigger-profile-continuous-mode")?.checked),
      spray_recoil_assist: Boolean(card.querySelector(".auto-trigger-profile-spray-recoil-assist")?.checked),
    };
  });
}

function normalizeCrosshairPresetColor(value) {
  const key = String(value || "").trim().toLowerCase();
  return CROSSHAIR_PRESET_COLORS.includes(key) ? key : "";
}

function uniqueCrosshairPresetColors(colors) {
  const normalized = [];
  (Array.isArray(colors) ? colors : []).forEach((color) => {
    const key = normalizeCrosshairPresetColor(color);
    if (key && !normalized.includes(key) && normalized.length < CROSSHAIR_MAX_COLORS) {
      normalized.push(key);
    }
  });
  return normalized.length ? normalized : [...CROSSHAIR_DEFAULTS.preset_colors];
}

function crosshairPresetColorsFromConfig(crosshair = {}) {
  return uniqueCrosshairPresetColors(Array.isArray(crosshair.preset_colors) ? crosshair.preset_colors : []);
}

function crosshairMainHotkeyFromConfig(crosshair = {}) {
  if (OPTIONAL_POINTER_HOTKEYS.includes(crosshair.hotkey)) {
    return crosshair.hotkey;
  }
  if (crosshair.only_when_fire_held === false) {
    return "";
  }
  return POINTER_HOTKEYS.includes(crosshair.fire_hotkey) ? crosshair.fire_hotkey : CROSSHAIR_DEFAULTS.hotkey;
}

function setCrosshairColorSlots(colors) {
  const selected = uniqueCrosshairPresetColors(colors);
  for (let index = 0; index < CROSSHAIR_MAX_COLORS; index += 1) {
    const enabled = index < selected.length;
    const color = selected[index] || CROSSHAIR_SLOT_DEFAULT_COLORS[index] || CROSSHAIR_PRESET_COLORS[0];
    setCheckbox(`crosshair_slot_${index}_enabled`, enabled);
    setRadioValue(`crosshair_slot_${index}_color`, color);
    const details = $(`crosshair_slot_${index}`);
    if (details) {
      details.open = enabled || index === 0;
    }
  }
}

function populateCrosshairConfig(crosshair) {
  const safeCrosshair = crosshair || {};
  setCheckbox(
    "crosshair_detection_enabled",
    safeCrosshair.detection_enabled ?? CROSSHAIR_DEFAULTS.detection_enabled
  );
  setValue("crosshair_hotkey", crosshairMainHotkeyFromConfig(safeCrosshair));
  setValue("crosshair_hotkey2", safeCrosshair.hotkey2 ?? CROSSHAIR_DEFAULTS.hotkey2);
  setValue("crosshair_hotkey_mode", safeCrosshair.hotkey_mode ?? CROSSHAIR_DEFAULTS.hotkey_mode);
  setValue("crosshair_roi_w", safeCrosshair.roi_w ?? CROSSHAIR_DEFAULTS.roi_w);
  setValue("crosshair_roi_h", safeCrosshair.roi_h ?? CROSSHAIR_DEFAULTS.roi_h);
  setCrosshairColorSlots(crosshairPresetColorsFromConfig(safeCrosshair));
}

function crosshairDefaultControlValues() {
  const values = {
    crosshair_detection_enabled: CROSSHAIR_DEFAULTS.detection_enabled,
    crosshair_hotkey: CROSSHAIR_DEFAULTS.hotkey,
    crosshair_hotkey2: CROSSHAIR_DEFAULTS.hotkey2,
    crosshair_hotkey_mode: CROSSHAIR_DEFAULTS.hotkey_mode,
    crosshair_roi_w: CROSSHAIR_DEFAULTS.roi_w,
    crosshair_roi_h: CROSSHAIR_DEFAULTS.roi_h,
  };
  for (let index = 0; index < CROSSHAIR_MAX_COLORS; index += 1) {
    const defaultColor = CROSSHAIR_SLOT_DEFAULT_COLORS[index] || CROSSHAIR_PRESET_COLORS[0];
    values[`crosshair_slot_${index}_enabled`] = index < CROSSHAIR_DEFAULTS.preset_colors.length;
    CROSSHAIR_PRESET_COLORS.forEach((color) => {
      values[`crosshair_slot_${index}_color_${color}`] = color === defaultColor;
    });
  }
  return values;
}

function collectCrosshairPresetColors() {
  const colors = [];
  for (let index = 0; index < CROSSHAIR_MAX_COLORS; index += 1) {
    if (!getCheckbox(`crosshair_slot_${index}_enabled`)) {
      continue;
    }
    const color = normalizeCrosshairPresetColor(
      getRadioValue(
        `crosshair_slot_${index}_color`,
        CROSSHAIR_SLOT_DEFAULT_COLORS[index] || CROSSHAIR_PRESET_COLORS[0]
      )
    );
    if (color) {
      colors.push(color);
    }
  }
  return uniqueCrosshairPresetColors(colors);
}

function collectCrosshairConfig() {
  const hotkey = getString("crosshair_hotkey");
  const hotkey2 = getString("crosshair_hotkey2");
  const normalizedHotkey = OPTIONAL_POINTER_HOTKEYS.includes(hotkey) ? hotkey : CROSSHAIR_DEFAULTS.hotkey;
  const normalizedHotkey2 = normalizedHotkey && OPTIONAL_POINTER_HOTKEYS.includes(hotkey2)
    ? hotkey2
    : CROSSHAIR_DEFAULTS.hotkey2;
  const hotkeyMode = getString("crosshair_hotkey_mode") === "all" && normalizedHotkey2 ? "all" : "any";
  return {
    detection_enabled: getCheckbox("crosshair_detection_enabled"),
    only_when_fire_held: !!normalizedHotkey,
    fire_hotkey: POINTER_HOTKEYS.includes(normalizedHotkey) ? normalizedHotkey : CROSSHAIR_DEFAULTS.fire_hotkey,
    hotkey: normalizedHotkey,
    hotkey2: normalizedHotkey2,
    hotkey_mode: hotkeyMode,
    roi_w: Math.round(getNumberInRange("crosshair_roi_w", CROSSHAIR_DEFAULTS.roi_w)),
    roi_h: Math.round(getNumberInRange("crosshair_roi_h", CROSSHAIR_DEFAULTS.roi_h)),
    preset_colors: collectCrosshairPresetColors(),
  };
}

function normalizeFanTemperatureSource(value) {
  const source = String(value || "").trim();
  return ["auto", "soc", "cpu_big", "gpu", "npu", "hailo"].includes(source) ? source : "auto";
}

function populateFanControlConfig(fanControl) {
  const config = fanControl || {};
  setCheckbox("fan_control_enabled", config.enabled ?? FAN_CONTROL_DEFAULTS.enabled);
  setValue(
    "fan_control_temperature_source",
    normalizeFanTemperatureSource(config.temperature_source ?? FAN_CONTROL_DEFAULTS.temperature_source)
  );
  setValue("fan_control_start_celsius", config.start_celsius ?? FAN_CONTROL_DEFAULTS.start_celsius);
  setValue("fan_control_full_celsius", config.full_celsius ?? FAN_CONTROL_DEFAULTS.full_celsius);
  setValue("fan_control_min_pwm_percent", config.min_pwm_percent ?? FAN_CONTROL_DEFAULTS.min_pwm_percent);
  setValue("fan_control_max_pwm_percent", config.max_pwm_percent ?? FAN_CONTROL_DEFAULTS.max_pwm_percent);
}

function collectFanControlConfig(previousFanControl = {}) {
  const start = Math.round(getNumberInRange("fan_control_start_celsius", previousFanControl.start_celsius ?? FAN_CONTROL_DEFAULTS.start_celsius));
  const fullRaw = Math.round(getNumberInRange("fan_control_full_celsius", previousFanControl.full_celsius ?? FAN_CONTROL_DEFAULTS.full_celsius));
  return {
    enabled: getCheckbox("fan_control_enabled"),
    temperature_source: normalizeFanTemperatureSource(getString("fan_control_temperature_source")),
    start_celsius: start,
    full_celsius: Math.max(start + 1, fullRaw),
    min_pwm_percent: 100,
    max_pwm_percent: 100,
    stop_hysteresis_celsius: Number.isFinite(Number(previousFanControl.stop_hysteresis_celsius))
      ? Number(previousFanControl.stop_hysteresis_celsius)
      : FAN_CONTROL_DEFAULTS.stop_hysteresis_celsius,
  };
}

function enforceFanControlInputPairs(changedId = "") {
  if (changedId === "fan_control_start_celsius" || changedId === "fan_control_full_celsius") {
    const startInput = $("fan_control_start_celsius");
    const fullInput = $("fan_control_full_celsius");
    const start = startInput ? Number(startInput.value) : NaN;
    const full = fullInput ? Number(fullInput.value) : NaN;
    if (Number.isFinite(start) && Number.isFinite(full) && full <= start && fullInput) {
      fullInput.value = formatControlValue(fullInput, Math.min(90, start + 1));
      const fullRange = $("fan_control_full_celsius_range");
      if (fullRange) {
        fullRange.value = fullInput.value;
      }
    }
  } else if (changedId === "fan_control_min_pwm_percent" || changedId === "fan_control_max_pwm_percent") {
    const minInput = $("fan_control_min_pwm_percent");
    const maxInput = $("fan_control_max_pwm_percent");
    const minValue = minInput ? Number(minInput.value) : NaN;
    const maxValue = maxInput ? Number(maxInput.value) : NaN;
    if (Number.isFinite(minValue) && Number.isFinite(maxValue) && maxValue < minValue && maxInput) {
      maxInput.value = formatControlValue(maxInput, minValue);
      const maxRange = $("fan_control_max_pwm_percent_range");
      if (maxRange) {
        maxRange.value = maxInput.value;
      }
    }
  }
}

function renderFanControlStatus(fanControl) {
  const fan = fanControl || {};
  const controlAvailable = !!fan.control_available;
  const enabled = !!fan.enabled;
  const temp = Number(fan.temperature_celsius);
  const pwmPercent = Number(fan.pwm_percent);
  const pwmRaw = Number(fan.pwm_raw);
  const rpm = Number(fan.fan_rpm);
  setText("fanControlPill", controlAvailable ? (enabled ? "全速运行" : "PWM接口可用") : "未检测到PWM接口");
  setText("fanRuntimeMode", enabled ? "全速" : "未启用");
  setText("fanControlAvailableValue", controlAvailable ? "PWM接口可用" : "未检测到PWM接口");
  setText("fanTemperatureSourceValue", fan.source_label || "--");
  setText("fanTemperatureValue", Number.isFinite(temp) && temp > 0 ? `${temp.toFixed(1)} °C` : "--");
  setText(
    "fanPwmValue",
    Number.isFinite(pwmPercent)
      ? `${Math.max(0, Math.min(100, Math.round(pwmPercent)))}%${Number.isFinite(pwmRaw) ? ` (${Math.round(pwmRaw)})` : ""}`
      : "--"
  );
  setText(
    "fanRpmValue",
    fan.tachometer_available
      ? (Number.isFinite(rpm) && rpm > 0 ? `${Math.round(rpm)} RPM` : "0 RPM")
      : "无转速反馈"
  );
  setText("fanErrorValue", fan.last_error || "正常");
}

function normalizeBlockedPhysicalButtons(buttons) {
  const normalized = [];
  (Array.isArray(buttons) ? buttons : []).forEach((button) => {
    if (BLOCKABLE_PHYSICAL_BUTTONS.includes(button) && !normalized.includes(button)) {
      normalized.push(button);
    }
  });
  return normalized;
}

function updatePhysicalButtonBlockButton() {
  const button = $("physicalButtonBlockButton");
  if (!button) {
    return;
  }
  const count = state.blockedPhysicalButtons.length;
  button.textContent = count > 0 ? `屏蔽物理按键 (${count})` : "屏蔽物理按键";
}

function renderPhysicalMotionBlockStatus(mouseOutput = {}) {
  const config = state.config && state.config.mouse_output ? state.config.mouse_output : {};
  const runtimeMode = normalizeMouseOutputMode(mouseOutput.mode || config.mode);
  const runtimeSupport = String(mouseOutput.physical_motion_block_support || "unknown");
  const unsupportedMode = runtimeMode === "kmboxnet" || runtimeMode === "catnet";
  const unsupported = unsupportedMode || runtimeSupport === "unsupported";
  const appliedMask = Number(mouseOutput.physical_motion_block_mask) || 0;
  const error = String(mouseOutput.physical_motion_block_error || "");
  const badge = $("physicalMotionBlockSupport");
  const hint = $("physicalMotionBlockHint");

  ["controller_block_physical_mouse_x_while_aiming", "controller_block_physical_mouse_y_while_aiming"].forEach((id) => {
    const input = $(id);
    if (input) {
      input.disabled = false;
    }
  });

  if (badge) {
    if (unsupported) {
      badge.textContent = "不支持";
      badge.className = "status-badge error";
    } else if (appliedMask !== 0) {
      badge.textContent = "屏蔽中";
      badge.className = "status-badge live";
    } else if (runtimeSupport === "supported") {
      badge.textContent = "可用";
      badge.className = "status-badge live";
    } else {
      badge.textContent = "等待连接";
      badge.className = "status-badge idle";
    }
  }
  if (hint) {
    if (unsupportedMode) {
      hint.textContent = "当前输出后端没有可靠的物理移动轴锁定协议。";
    } else if (error) {
      hint.textContent = `轴屏蔽未生效：${error}`;
    } else if (appliedMask !== 0) {
      const axes = [(appliedMask & 1) ? "X" : "", (appliedMask & 2) ? "Y" : ""].filter(Boolean).join("/");
      hint.textContent = `正在屏蔽物理鼠标 ${axes} 轴，AI 移动保持正常。`;
    } else {
      hint.textContent = "锁定目标并进入瞄准状态后自动生效。";
    }
  }
}

function renderPhysicalButtonBlockEditor() {
  const list = $("physicalButtonBlockList");
  const empty = $("physicalButtonBlockEmpty");
  const select = $("physicalButtonBlockSelect");
  const addButton = $("addPhysicalButtonBlockButton");
  const blocked = normalizeBlockedPhysicalButtons(state.blockedPhysicalButtonsDraft);
  state.blockedPhysicalButtonsDraft = blocked;

  if (list) {
    list.replaceChildren();
    blocked.forEach((buttonName) => {
      const row = document.createElement("div");
      row.className = "physical-button-block-row";
      const label = document.createElement("span");
      label.textContent = HOTKEY_LABELS[buttonName] || buttonName;
      const removeButton = document.createElement("button");
      removeButton.className = "icon-button physical-button-block-remove";
      removeButton.type = "button";
      removeButton.dataset.physicalButton = buttonName;
      removeButton.setAttribute("aria-label", `移除${label.textContent}`);
      removeButton.title = `移除${label.textContent}`;
      removeButton.innerHTML = '<svg viewBox="0 0 24 24" aria-hidden="true"><path d="M18 6 6 18M6 6l12 12"/></svg>';
      row.append(label, removeButton);
      list.append(row);
    });
  }
  if (empty) {
    empty.hidden = blocked.length > 0;
  }

  const remaining = BLOCKABLE_PHYSICAL_BUTTONS.filter((button) => !blocked.includes(button));
  if (select) {
    select.replaceChildren();
    if (remaining.length === 0) {
      const option = document.createElement("option");
      option.value = "";
      option.textContent = "已全部添加";
      select.append(option);
      select.disabled = true;
    } else {
      remaining.forEach((buttonName) => {
        const option = document.createElement("option");
        option.value = buttonName;
        option.textContent = HOTKEY_LABELS[buttonName] || buttonName;
        select.append(option);
      });
      select.disabled = false;
    }
  }
  if (addButton) {
    addButton.disabled = remaining.length === 0;
  }
}

function setPhysicalButtonBlockDialogOpen(open) {
  const dialog = $("physicalButtonBlockDialog");
  if (!dialog) {
    return;
  }
  if (open) {
    state.blockedPhysicalButtonsDraft = [...state.blockedPhysicalButtons];
    renderPhysicalButtonBlockEditor();
  }
  dialog.hidden = !open;
  setAnyModalOpen();
  if (open) {
    $("physicalButtonBlockSelect")?.focus();
  }
}

function addPhysicalButtonBlock() {
  const select = $("physicalButtonBlockSelect");
  const buttonName = select ? select.value : "";
  if (!BLOCKABLE_PHYSICAL_BUTTONS.includes(buttonName) ||
      state.blockedPhysicalButtonsDraft.includes(buttonName)) {
    return;
  }
  state.blockedPhysicalButtonsDraft.push(buttonName);
  renderPhysicalButtonBlockEditor();
}

function savePhysicalButtonBlockList() {
  state.blockedPhysicalButtons = normalizeBlockedPhysicalButtons(state.blockedPhysicalButtonsDraft);
  updatePhysicalButtonBlockButton();
  setPhysicalButtonBlockDialogOpen(false);
  requestConfigApply(0);
  showToast("物理按键屏蔽列表已更新");
}

function populateForm(config) {
  if (!config) {
    return;
  }
  state.isPopulating = true;
  state.config = config;

  const capture = config.capture || {};
  const ai = config.ai || {};
  const controller = ai.controller || {};
  const recoil = config.recoil || {};
  const autoTrigger = config.auto_trigger || {};
  const rapidFire = config.rapid_fire || {};
  const crosshair = config.crosshair || {};
  const autoBackFlick = config.auto_back_flick || {};
  const hotkeyGuard = config.hotkey_guard || {};
  const mouseOutput = config.mouse_output || {};
  const latency = config.latency || {};
  const fanControl = config.fan_control || {};
  const loopoutOverlay = config.loopout_overlay || {};

  populateLoopoutOverlaySettings(loopoutOverlay);

  setValue("capture_device", capture.device);
  setCropSizeValue(capture.crop_size);
  updateDynamicOffsetControlLimits();
  setValue("capture_crop_offset_x", capture.crop_offset_x ?? 0);
  setValue("capture_crop_offset_y", capture.crop_offset_y ?? 0);
  setValue("capture_format_preference", capture.format_preference);
  setCheckbox("capture_nv12_uv_swapped", capture.nv12_uv_swapped);
  setCheckbox("capture_nv12_bt709", capture.nv12_bt709);
  setCheckbox("capture_nv12_full_range", capture.nv12_full_range);
  setValue("video_detection_confidence", config.video_detection_confidence);
  setValue("video_detection_iou", config.video_detection_iou);
  setValue("sens", config.sens);
  setRadioValue("mouse_output_mode", normalizeMouseOutputMode(mouseOutput.mode));
  populateKmboxConfig(mouseOutput.kmboxnet || {}, mouseOutput.mode);
  populateCatnetConfig(mouseOutput.catnet || {}, mouseOutput.mode);
  populateMakcuConfig(mouseOutput.makcu || {}, mouseOutput.mode);
  populateFerrumConfig(mouseOutput.ferrum || {}, mouseOutput.mode);
  populateKmboxbConfig(mouseOutput.kmboxb || {}, mouseOutput.mode);
  state.blockedPhysicalButtons = normalizeBlockedPhysicalButtons(mouseOutput.blocked_physical_buttons);
  if (!$("physicalButtonBlockDialog") || $("physicalButtonBlockDialog").hidden) {
    state.blockedPhysicalButtonsDraft = [...state.blockedPhysicalButtons];
  }
  updatePhysicalButtonBlockButton();
  setValue("range_factor", config.range_factor);
  setValue("pos", config.pos);
  setCheckbox("hotkey_guard_enabled", hotkeyGuard.enabled ?? HOTKEY_GUARD_DEFAULTS.enabled);
  setValue("hotkey_guard_toggle_hotkey", hotkeyGuard.toggle_hotkey ?? HOTKEY_GUARD_DEFAULTS.toggle_hotkey);
  renderAimProfiles(config.aim_profiles);

  setValue("controller_kp_x", controller.kp_x ?? CONTROLLER_DEFAULTS.kp_x);
  setValue("controller_kp_y", controller.kp_y ?? CONTROLLER_DEFAULTS.kp_y);
  setValue("controller_ki_x", controller.ki_x ?? CONTROLLER_DEFAULTS.ki_x);
  setValue("controller_ki_y", controller.ki_y ?? CONTROLLER_DEFAULTS.ki_y);
  setValue("controller_kd_x", controller.kd_x ?? CONTROLLER_DEFAULTS.kd_x);
  setValue("controller_kd_y", controller.kd_y ?? CONTROLLER_DEFAULTS.kd_y);
  setValue("controller_predict_x", controller.predict_x ?? CONTROLLER_DEFAULTS.predict_x);
  setValue("controller_predict_y", controller.predict_y ?? CONTROLLER_DEFAULTS.predict_y);
  setValue("controller_rate_x", controller.rate_x ?? CONTROLLER_DEFAULTS.rate_x);
  setValue("controller_rate_y", controller.rate_y ?? CONTROLLER_DEFAULTS.rate_y);
  setValue("controller_output_deadzone", controller.output_deadzone ?? CONTROLLER_DEFAULTS.output_deadzone);
  setCheckbox("controller_pull_curve_enabled", controller.pull_curve_enabled ?? CONTROLLER_DEFAULTS.pull_curve_enabled);
  setValue("controller_pull_curve_strength", controller.pull_curve_strength ?? CONTROLLER_DEFAULTS.pull_curve_strength);
  setValue("controller_pull_curve_jitter_px", controller.pull_curve_jitter_px ?? CONTROLLER_DEFAULTS.pull_curve_jitter_px);
  setValue("controller_pull_curve_min_distance", controller.pull_curve_min_distance ?? CONTROLLER_DEFAULTS.pull_curve_min_distance);
  setCheckbox("controller_continuous_lead_enabled", controller.continuous_lead_enabled ?? CONTROLLER_DEFAULTS.continuous_lead_enabled);
  setValue("controller_continuous_lead_enter_distance", controller.continuous_lead_enter_distance ?? CONTROLLER_DEFAULTS.continuous_lead_enter_distance);
  setValue("controller_continuous_lead_scale", controller.continuous_lead_scale ?? CONTROLLER_DEFAULTS.continuous_lead_scale);
  setValue("controller_continuous_lead_fade_in_ms", controller.continuous_lead_fade_in_ms ?? CONTROLLER_DEFAULTS.continuous_lead_fade_in_ms);
  setValue("controller_continuous_lead_fade_out_ms", controller.continuous_lead_fade_out_ms ?? CONTROLLER_DEFAULTS.continuous_lead_fade_out_ms);
  setValue(
    "controller_continuous_lead_near_disable_ratio",
    controller.continuous_lead_near_disable_ratio ?? CONTROLLER_DEFAULTS.continuous_lead_near_disable_ratio
  );
  setCheckbox(
    "controller_block_physical_mouse_x_while_aiming",
    controller.block_physical_mouse_x_while_aiming ?? CONTROLLER_DEFAULTS.block_physical_mouse_x_while_aiming
  );
  setCheckbox(
    "controller_block_physical_mouse_y_while_aiming",
    controller.block_physical_mouse_y_while_aiming ?? CONTROLLER_DEFAULTS.block_physical_mouse_y_while_aiming
  );
  setCheckbox("controller_aim_fire_lock_y", controller.aim_fire_lock_y ?? CONTROLLER_DEFAULTS.aim_fire_lock_y);
  setValue("controller_aim_reference_offset_x", controller.aim_reference_offset_x ?? CONTROLLER_DEFAULTS.aim_reference_offset_x);
  setValue("controller_aim_reference_offset_y", controller.aim_reference_offset_y ?? CONTROLLER_DEFAULTS.aim_reference_offset_y);
  updateDynamicOffsetControlLimits({ clampValues: true });
  setValue("controller_selector_lost_grace_ms", controller.selector_lost_grace_ms ?? CONTROLLER_DEFAULTS.selector_lost_grace_ms);
  setValue("controller_y_axis_fire_hotkey", controller.y_axis_fire_hotkey ?? CONTROLLER_DEFAULTS.y_axis_fire_hotkey);
  setValue("controller_y_axis_fire_release_delay_sec", controller.y_axis_fire_release_delay_sec ?? CONTROLLER_DEFAULTS.y_axis_fire_release_delay_sec);

  setCheckbox("recoil_enabled", recoil.enabled ?? RECOIL_DEFAULTS.enabled);
  setCheckbox("recoil_only_when_target_visible", recoil.only_when_target_visible ?? RECOIL_DEFAULTS.only_when_target_visible);
  setValue("recoil_target_lost_release_ms", recoil.target_lost_release_ms ?? RECOIL_DEFAULTS.target_lost_release_ms);
  setValue("recoil_hotkey", recoil.hotkey ?? RECOIL_DEFAULTS.hotkey);
  setValue("recoil_hotkey2", recoil.hotkey2 ?? RECOIL_DEFAULTS.hotkey2);
  setValue("recoil_hotkey_mode", recoil.hotkey_mode ?? RECOIL_DEFAULTS.hotkey_mode);
  setCheckbox("recoil_trigger_delay_enabled", recoil.trigger_delay_enabled ?? RECOIL_DEFAULTS.trigger_delay_enabled);
  setValue("recoil_trigger_delay_ms", recoil.trigger_delay_ms ?? RECOIL_DEFAULTS.trigger_delay_ms);
  setValue("recoil_strength", recoil.strength ?? RECOIL_DEFAULTS.strength);
  setValue("recoil_speed", recoil.speed ?? RECOIL_DEFAULTS.speed);
  setCheckbox("recoil_humanize_enabled", recoil.humanize_enabled ?? RECOIL_DEFAULTS.humanize_enabled);
  setValue("recoil_humanize_curve_strength", recoil.humanize_curve_strength ?? RECOIL_DEFAULTS.humanize_curve_strength);
  setValue("recoil_humanize_jitter_px", recoil.humanize_jitter_px ?? RECOIL_DEFAULTS.humanize_jitter_px);
  setValue("recoil_humanize_jitter_frequency", recoil.humanize_jitter_frequency ?? RECOIL_DEFAULTS.humanize_jitter_frequency);

  renderAutoTriggerProfiles(autoTrigger);

  setCheckbox("rapid_fire_enabled", rapidFire.enabled ?? RAPID_FIRE_DEFAULTS.enabled);
  setValue("rapid_fire_hotkey", rapidFire.hotkey ?? RAPID_FIRE_DEFAULTS.hotkey);
  setValue("rapid_fire_press_base_ms", rapidFire.press_base_ms ?? RAPID_FIRE_DEFAULTS.press_base_ms);
  setValue("rapid_fire_interval_base_ms", rapidFire.interval_base_ms ?? RAPID_FIRE_DEFAULTS.interval_base_ms);

  populateCrosshairConfig(crosshair);

  setCheckbox("auto_back_flick_enabled", autoBackFlick.enabled ?? AUTO_BACK_FLICK_DEFAULTS.enabled);
  setAutoBackFlickClassConfigs(autoBackFlick);
  renderAutoBackFlickClassPicker(autoBackFlick);
  setValue("hailo_pipeline_depth", latency.hailo_pipeline_depth ?? 3);
  populateFanControlConfig(fanControl);

  syncAllRangeFields();
  updateAimRangeOverlay();
  state.configReady = true;
  state.isPopulating = false;
  updateAssistModuleCollapseStates();
  renderKmboxStatus((state.data && state.data.state && state.data.state.mouse_output) || {});
  renderCatnetStatus((state.data && state.data.state && state.data.state.mouse_output) || {});
  renderMakcuStatus((state.data && state.data.state && state.data.state.mouse_output) || {});
  renderFerrumStatus((state.data && state.data.state && state.data.state.mouse_output) || {});
  renderKmboxbStatus((state.data && state.data.state && state.data.state.mouse_output) || {});
  renderPhysicalMotionBlockStatus((state.data && state.data.state && state.data.state.mouse_output) || {});
}

function setMovementControlDefaultsToForm() {
  const controller = MOVEMENT_CONTROL_DEFAULTS.controller;
  setValue("sens", MOVEMENT_CONTROL_DEFAULTS.sens);
  setRadioValue("mouse_output_mode", MOVEMENT_CONTROL_DEFAULTS.mouse_output_mode);
  setValue("controller_kp_x", controller.kp_x);
  setValue("controller_kp_y", controller.kp_y);
  setValue("controller_ki_x", controller.ki_x);
  setValue("controller_ki_y", controller.ki_y);
  setValue("controller_kd_x", controller.kd_x);
  setValue("controller_kd_y", controller.kd_y);
  setValue("controller_predict_x", controller.predict_x);
  setValue("controller_predict_y", controller.predict_y);
  setValue("controller_rate_x", controller.rate_x);
  setValue("controller_rate_y", controller.rate_y);
  setValue("controller_output_deadzone", controller.output_deadzone);
  setCheckbox("controller_pull_curve_enabled", controller.pull_curve_enabled);
  setValue("controller_pull_curve_strength", controller.pull_curve_strength);
  setValue("controller_pull_curve_jitter_px", controller.pull_curve_jitter_px);
  setValue("controller_pull_curve_min_distance", controller.pull_curve_min_distance);
  setCheckbox("controller_continuous_lead_enabled", controller.continuous_lead_enabled);
  setValue("controller_continuous_lead_enter_distance", controller.continuous_lead_enter_distance);
  setValue("controller_continuous_lead_scale", controller.continuous_lead_scale);
  setValue("controller_continuous_lead_fade_in_ms", controller.continuous_lead_fade_in_ms);
  setValue("controller_continuous_lead_fade_out_ms", controller.continuous_lead_fade_out_ms);
  setValue("controller_continuous_lead_near_disable_ratio", controller.continuous_lead_near_disable_ratio);
  setCheckbox("controller_block_physical_mouse_x_while_aiming", controller.block_physical_mouse_x_while_aiming);
  setCheckbox("controller_block_physical_mouse_y_while_aiming", controller.block_physical_mouse_y_while_aiming);
  setCheckbox("controller_aim_fire_lock_y", controller.aim_fire_lock_y);
  setValue("controller_selector_lost_grace_ms", controller.selector_lost_grace_ms);
  setValue("controller_y_axis_fire_hotkey", controller.y_axis_fire_hotkey);
  setValue("controller_y_axis_fire_release_delay_sec", controller.y_axis_fire_release_delay_sec);
}

function syncRangeFieldsForIds(ids) {
  const idSet = new Set(ids);
  RANGE_BINDINGS.forEach(([numberId, rangeId]) => {
    if (!idSet.has(numberId)) {
      return;
    }
    const numberInput = $(numberId);
    const rangeInput = $(rangeId);
    if (numberInput && rangeInput && numberInput.value !== "") {
      const formattedValue = formatControlValue(numberInput, numberInput.value);
      numberInput.value = formattedValue;
      rangeInput.value = formatControlValue(rangeInput, formattedValue);
    }
  });
  if (idSet.has("capture_crop_size")) {
    syncCropSizePresetRange(getCropSize());
  }
}

function resetFieldDefaults(fieldDefaults) {
  const changedIds = [];
  Object.entries(fieldDefaults).forEach(([id, value]) => {
    const el = $(id);
    if (!el) {
      return;
    }
    if (el.type === "checkbox") {
      setCheckbox(id, value);
    } else if (el.type === "radio") {
      el.checked = !!value;
    } else if (id === "mouse_output_mode") {
      setRadioValue("mouse_output_mode", value);
    } else {
      setValue(id, value);
    }
    changedIds.push(id);
  });
  syncRangeFieldsForIds(changedIds);
}

function resetOverviewDefaults() {
  state.isPopulating = true;
  resetFieldDefaults(overviewDefaults());
  state.isPopulating = false;
  updateDynamicOffsetControlLimits({ clampValues: true });
  requestConfigApply(0);
  showToast("总览已恢复默认值");
}

function movementDefaultsForSection(sectionId) {
  const controller = MOVEMENT_CONTROL_DEFAULTS.controller;
  const defaultsBySection = {
    "control-section-pid": {
      sens: MOVEMENT_CONTROL_DEFAULTS.sens,
      mouse_output_mode: MOVEMENT_CONTROL_DEFAULTS.mouse_output_mode,
      controller_kp_x: controller.kp_x,
      controller_kp_y: controller.kp_y,
      controller_ki_x: controller.ki_x,
      controller_ki_y: controller.ki_y,
      controller_kd_x: controller.kd_x,
      controller_kd_y: controller.kd_y,
      controller_predict_x: controller.predict_x,
      controller_predict_y: controller.predict_y,
      controller_rate_x: controller.rate_x,
      controller_rate_y: controller.rate_y,
      controller_output_deadzone: controller.output_deadzone,
      controller_selector_lost_grace_ms: controller.selector_lost_grace_ms,
      controller_aim_fire_lock_y: controller.aim_fire_lock_y,
      controller_y_axis_fire_hotkey: controller.y_axis_fire_hotkey,
      controller_y_axis_fire_release_delay_sec: controller.y_axis_fire_release_delay_sec,
    },
    "control-section-pull-curve": {
      controller_pull_curve_enabled: controller.pull_curve_enabled,
      controller_pull_curve_strength: controller.pull_curve_strength,
      controller_pull_curve_jitter_px: controller.pull_curve_jitter_px,
      controller_pull_curve_min_distance: controller.pull_curve_min_distance,
    },
    "control-section-continuous-lead": {
      controller_continuous_lead_enabled: controller.continuous_lead_enabled,
      controller_continuous_lead_enter_distance: controller.continuous_lead_enter_distance,
      controller_continuous_lead_scale: controller.continuous_lead_scale,
      controller_continuous_lead_fade_in_ms: controller.continuous_lead_fade_in_ms,
      controller_continuous_lead_fade_out_ms: controller.continuous_lead_fade_out_ms,
      controller_continuous_lead_near_disable_ratio: controller.continuous_lead_near_disable_ratio,
    },
    "control-section-physical-motion-block": {
      controller_block_physical_mouse_x_while_aiming: controller.block_physical_mouse_x_while_aiming,
      controller_block_physical_mouse_y_while_aiming: controller.block_physical_mouse_y_while_aiming,
    },
  };
  return defaultsBySection[sectionId] || defaultsBySection["control-section-pid"];
}

function resetCurrentMovementSectionDefaults() {
  const sectionId = state.activeControlSectionId || "control-section-auto-calibration";
  if (sectionId === "control-section-auto-calibration") {
    showToast("自动标定参数请使用“清除标定”恢复默认值");
    return;
  }
  state.isPopulating = true;
  resetFieldDefaults(movementDefaultsForSection(sectionId));
  state.isPopulating = false;
  requestConfigApply(0);
  showToast("当前移动控制子页面已恢复默认值");
}

function assistDefaultsForSection(sectionId) {
  const defaultsBySection = {
    "assist-section-recoil": {
      recoil_enabled: RECOIL_DEFAULTS.enabled,
      recoil_only_when_target_visible: RECOIL_DEFAULTS.only_when_target_visible,
      recoil_target_lost_release_ms: RECOIL_DEFAULTS.target_lost_release_ms,
      recoil_hotkey: RECOIL_DEFAULTS.hotkey,
      recoil_hotkey2: RECOIL_DEFAULTS.hotkey2,
      recoil_hotkey_mode: RECOIL_DEFAULTS.hotkey_mode,
      recoil_trigger_delay_enabled: RECOIL_DEFAULTS.trigger_delay_enabled,
      recoil_trigger_delay_ms: RECOIL_DEFAULTS.trigger_delay_ms,
      recoil_strength: RECOIL_DEFAULTS.strength,
      recoil_speed: RECOIL_DEFAULTS.speed,
      recoil_humanize_enabled: RECOIL_DEFAULTS.humanize_enabled,
      recoil_humanize_curve_strength: RECOIL_DEFAULTS.humanize_curve_strength,
      recoil_humanize_jitter_px: RECOIL_DEFAULTS.humanize_jitter_px,
      recoil_humanize_jitter_frequency: RECOIL_DEFAULTS.humanize_jitter_frequency,
    },
    "assist-section-trigger": {},
    "assist-section-rapid": {
      rapid_fire_enabled: RAPID_FIRE_DEFAULTS.enabled,
      rapid_fire_hotkey: RAPID_FIRE_DEFAULTS.hotkey,
      rapid_fire_press_base_ms: RAPID_FIRE_DEFAULTS.press_base_ms,
      rapid_fire_interval_base_ms: RAPID_FIRE_DEFAULTS.interval_base_ms,
    },
    "assist-section-back-flick": {
      auto_back_flick_enabled: AUTO_BACK_FLICK_DEFAULTS.enabled,
    },
    "assist-section-crosshair": crosshairDefaultControlValues(),
    "fan-page": {
      fan_control_enabled: FAN_CONTROL_DEFAULTS.enabled,
      fan_control_temperature_source: FAN_CONTROL_DEFAULTS.temperature_source,
    },
  };
  return defaultsBySection[sectionId] || defaultsBySection["assist-section-recoil"];
}

function assistSectionLabel(sectionId) {
  const labels = {
    "assist-section-recoil": "压枪",
    "assist-section-trigger": "自动开火",
    "assist-section-rapid": "连点",
    "assist-section-back-flick": "自动背闪",
    "assist-section-crosshair": "准星找色",
    "fan-page": "风扇设置",
  };
  return labels[sectionId] || "当前页";
}

function resetCurrentAssistSectionDefaults() {
  state.isPopulating = true;
  const sectionId = state.activeAssistSectionId || "assist-section-recoil";
  resetFieldDefaults(assistDefaultsForSection(sectionId));
  if (sectionId === "assist-section-trigger") {
    renderAutoTriggerProfiles({ enabled: false, profiles: [] });
  } else if (sectionId === "assist-section-back-flick") {
    setAutoBackFlickClassConfigs(AUTO_BACK_FLICK_DEFAULTS);
    renderAutoBackFlickClassPicker(AUTO_BACK_FLICK_DEFAULTS);
  }
  state.isPopulating = false;
  updateAssistModuleCollapseStates();
  requestConfigApply(0);
  showToast(`${assistSectionLabel(sectionId)}已恢复默认值`);
}

function collectConfig() {
  const previousCapture = state.config && state.config.capture ? state.config.capture : {};
  const previousLatency = state.config && state.config.latency ? state.config.latency : {};
  const previousFanControl = state.config && state.config.fan_control ? state.config.fan_control : {};
  const selectedModel = (state.config && state.config.model_id) ||
    (state.data && state.data.config && state.data.config.model_id) ||
    "";
  const autoTriggerProfiles = collectAutoTriggerProfiles();
  const primaryAutoTriggerProfile = autoTriggerProfiles[0] || defaultAutoTriggerProfile();
  const kmboxConfig = collectKmboxConfig();
  const catnetConfig = collectCatnetConfig();
  const makcuConfig = collectMakcuConfig();
  const ferrumConfig = collectFerrumConfig();
  const kmboxbConfig = collectKmboxbConfig();
  const autoBackFlickClassMask = collectAutoBackFlickClassMask(
    state.config && state.config.auto_back_flick
      ? state.config.auto_back_flick
      : AUTO_BACK_FLICK_DEFAULTS
  );
  const autoBackFlickClassConfigs = collectAutoBackFlickClassConfigs();
  const primaryAutoBackFlickClassConfig = autoBackFlickClassConfigFor(
    firstClassIdFromMask(autoBackFlickClassMask, AUTO_BACK_FLICK_DEFAULTS.class_id)
  );
  const mouseOutputMode = kmboxbConfig.enabled
    ? "kmboxb"
    : ferrumConfig.enabled
    ? "ferrum"
    : makcuConfig.enabled
    ? "makcu"
    : catnetConfig.enabled
    ? "catnet"
    : kmboxConfig.enabled
    ? "kmboxnet"
    : normalizeMouseOutputMode(getRadioValue("mouse_output_mode", MOVEMENT_CONTROL_DEFAULTS.mouse_output_mode));
  return {
    model_id: selectedModel,
    video_detection_confidence: getNumber("video_detection_confidence", 0.25),
    video_detection_iou: getNumber("video_detection_iou", 0.45),
    capture: {
      device: getString("capture_device") || previousCapture.device || "/dev/video0",
      crop_size: getCropSize(previousCapture.crop_size || 320),
      crop_offset_x: getNumberInRange("capture_crop_offset_x", previousCapture.crop_offset_x ?? 0),
      crop_offset_y: getNumberInRange("capture_crop_offset_y", previousCapture.crop_offset_y ?? 0),
      format_preference: getString("capture_format_preference") || previousCapture.format_preference || "auto",
      nv12_uv_swapped: getCheckbox("capture_nv12_uv_swapped"),
      nv12_bt709: getCheckbox("capture_nv12_bt709"),
      nv12_full_range: getCheckbox("capture_nv12_full_range"),
    },
    latency: {
      mode: previousLatency.mode || "ultra",
      preview_policy: previousLatency.preview_policy || "degrade",
      realtime: previousLatency.realtime || "try",
      preview_interval_ms: Number.isFinite(Number(previousLatency.preview_interval_ms))
        ? Number(previousLatency.preview_interval_ms)
        : 66,
      hailo_pipeline_depth: Math.round(getNumberInRange(
        "hailo_pipeline_depth",
        previousLatency.hailo_pipeline_depth ?? 3
      )),
    },
    fan_control: collectFanControlConfig(previousFanControl),
    loopout_overlay: (state.config && state.config.loopout_overlay) || {
      class_mask: AIM_CLASS_ALL_MASK,
      thickness: 2,
      use_class_colors: true,
      color: "#ff5edb",
    },
    mouse_output: {
      mode: mouseOutputMode,
      blocked_physical_buttons: normalizeBlockedPhysicalButtons(state.blockedPhysicalButtons),
      kmboxnet: kmboxConfig,
      catnet: catnetConfig,
      makcu: makcuConfig,
      ferrum: ferrumConfig,
      kmboxb: kmboxbConfig,
    },
    sens: getNumber("sens", MOVEMENT_CONTROL_DEFAULTS.sens),
    range_factor: getNumber("range_factor", 1),
    pos: getNumber("pos", 0.5),
    hotkey_guard: {
      enabled: getCheckbox("hotkey_guard_enabled"),
      toggle_hotkey: getString("hotkey_guard_toggle_hotkey") || HOTKEY_GUARD_DEFAULTS.toggle_hotkey,
    },
    aim_profiles: collectAimProfiles(),
    ai: {
      controller: {
        kp_x: getNumber("controller_kp_x", CONTROLLER_DEFAULTS.kp_x),
        kp_y: getNumber("controller_kp_y", CONTROLLER_DEFAULTS.kp_y),
        ki_x: getNumber("controller_ki_x", CONTROLLER_DEFAULTS.ki_x),
        ki_y: getNumber("controller_ki_y", CONTROLLER_DEFAULTS.ki_y),
        kd_x: getNumber("controller_kd_x", CONTROLLER_DEFAULTS.kd_x),
        kd_y: getNumber("controller_kd_y", CONTROLLER_DEFAULTS.kd_y),
        predict_x: getNumber("controller_predict_x", CONTROLLER_DEFAULTS.predict_x),
        predict_y: getNumber("controller_predict_y", CONTROLLER_DEFAULTS.predict_y),
        rate_x: getNumber("controller_rate_x", CONTROLLER_DEFAULTS.rate_x),
        rate_y: getNumber("controller_rate_y", CONTROLLER_DEFAULTS.rate_y),
        smooth_x: CONTROLLER_DEFAULTS.smooth_x,
        smooth_y: CONTROLLER_DEFAULTS.smooth_y,
        output_deadzone: getNumber("controller_output_deadzone", CONTROLLER_DEFAULTS.output_deadzone),
        pull_curve_enabled: getCheckbox("controller_pull_curve_enabled"),
        pull_curve_strength: getNumber("controller_pull_curve_strength", CONTROLLER_DEFAULTS.pull_curve_strength),
        pull_curve_jitter_px: getNumber("controller_pull_curve_jitter_px", CONTROLLER_DEFAULTS.pull_curve_jitter_px),
        pull_curve_min_distance: getNumber("controller_pull_curve_min_distance", CONTROLLER_DEFAULTS.pull_curve_min_distance),
        continuous_lead_enabled: getCheckbox("controller_continuous_lead_enabled"),
        continuous_lead_enter_distance: getNumber("controller_continuous_lead_enter_distance", CONTROLLER_DEFAULTS.continuous_lead_enter_distance),
        continuous_lead_scale: getNumber("controller_continuous_lead_scale", CONTROLLER_DEFAULTS.continuous_lead_scale),
        continuous_lead_fade_in_ms: getNumber("controller_continuous_lead_fade_in_ms", CONTROLLER_DEFAULTS.continuous_lead_fade_in_ms),
        continuous_lead_fade_out_ms: getNumber("controller_continuous_lead_fade_out_ms", CONTROLLER_DEFAULTS.continuous_lead_fade_out_ms),
        continuous_lead_near_disable_ratio: getNumber(
          "controller_continuous_lead_near_disable_ratio",
          CONTROLLER_DEFAULTS.continuous_lead_near_disable_ratio
        ),
        block_physical_mouse_x_while_aiming: getCheckbox("controller_block_physical_mouse_x_while_aiming"),
        block_physical_mouse_y_while_aiming: getCheckbox("controller_block_physical_mouse_y_while_aiming"),
        aim_fire_lock_y: getCheckbox("controller_aim_fire_lock_y"),
        aim_reference_offset_x: getNumberInRange("controller_aim_reference_offset_x", CONTROLLER_DEFAULTS.aim_reference_offset_x),
        aim_reference_offset_y: getNumberInRange("controller_aim_reference_offset_y", CONTROLLER_DEFAULTS.aim_reference_offset_y),
        y_axis_fire_hotkey: getString("controller_y_axis_fire_hotkey") || CONTROLLER_DEFAULTS.y_axis_fire_hotkey,
        y_axis_fire_release_delay_sec: getNumber("controller_y_axis_fire_release_delay_sec", CONTROLLER_DEFAULTS.y_axis_fire_release_delay_sec),
        selector_lost_grace_ms: getNumber("controller_selector_lost_grace_ms", CONTROLLER_DEFAULTS.selector_lost_grace_ms),
      },
    },
    recoil: {
      enabled: getCheckbox("recoil_enabled"),
      only_when_target_visible: getCheckbox("recoil_only_when_target_visible"),
      target_lost_release_ms: getNumber("recoil_target_lost_release_ms", RECOIL_DEFAULTS.target_lost_release_ms),
      hotkey: getString("recoil_hotkey") || RECOIL_DEFAULTS.hotkey,
      hotkey2: getString("recoil_hotkey2"),
      hotkey_mode: getString("recoil_hotkey_mode") || RECOIL_DEFAULTS.hotkey_mode,
      trigger_delay_enabled: getCheckbox("recoil_trigger_delay_enabled"),
      trigger_delay_ms: getNumber("recoil_trigger_delay_ms", RECOIL_DEFAULTS.trigger_delay_ms),
      strength: getNumber("recoil_strength", RECOIL_DEFAULTS.strength),
      speed: getNumber("recoil_speed", RECOIL_DEFAULTS.speed),
      humanize_enabled: getCheckbox("recoil_humanize_enabled"),
      humanize_curve_strength: getNumber("recoil_humanize_curve_strength", RECOIL_DEFAULTS.humanize_curve_strength),
      humanize_jitter_px: getNumber("recoil_humanize_jitter_px", RECOIL_DEFAULTS.humanize_jitter_px),
      humanize_jitter_frequency: getNumber("recoil_humanize_jitter_frequency", RECOIL_DEFAULTS.humanize_jitter_frequency),
    },
    auto_trigger: {
      enabled: autoTriggerProfiles.length > 0,
      activation_hotkey: primaryAutoTriggerProfile.hotkey || AUTO_TRIGGER_DEFAULTS.activation_hotkey,
      profiles: autoTriggerProfiles,
      fire_mode: primaryAutoTriggerProfile.fire_mode || AUTO_TRIGGER_DEFAULTS.fire_mode,
      enter_delay_min_ms: primaryAutoTriggerProfile.enter_delay_min_ms,
      enter_delay_max_ms: primaryAutoTriggerProfile.enter_delay_max_ms,
      cooldown_min_ms: primaryAutoTriggerProfile.cooldown_min_ms,
      cooldown_max_ms: primaryAutoTriggerProfile.cooldown_max_ms,
      spray_release_delay_ms: primaryAutoTriggerProfile.spray_release_delay_ms,
      continuous_mode: Boolean(primaryAutoTriggerProfile.continuous_mode),
      spray_recoil_assist: Boolean(primaryAutoTriggerProfile.spray_recoil_assist),
    },
    rapid_fire: {
      enabled: getCheckbox("rapid_fire_enabled"),
      hotkey: getString("rapid_fire_hotkey") || RAPID_FIRE_DEFAULTS.hotkey,
      press_base_ms: getNumber("rapid_fire_press_base_ms", RAPID_FIRE_DEFAULTS.press_base_ms),
      interval_base_ms: getNumber("rapid_fire_interval_base_ms", RAPID_FIRE_DEFAULTS.interval_base_ms),
    },
    crosshair: collectCrosshairConfig(),
    auto_back_flick: {
      enabled: getCheckbox("auto_back_flick_enabled"),
      random_direction: primaryAutoBackFlickClassConfig.random_direction,
      dodge_away_from_target: primaryAutoBackFlickClassConfig.dodge_away_from_target,
      class_id: firstClassIdFromMask(autoBackFlickClassMask, AUTO_BACK_FLICK_DEFAULTS.class_id),
      class_filter_mask: autoBackFlickClassMask,
      turn_pixels: primaryAutoBackFlickClassConfig.turn_pixels,
      wait_ms: primaryAutoBackFlickClassConfig.wait_ms,
      turn_random: primaryAutoBackFlickClassConfig.turn_random,
      return_random: primaryAutoBackFlickClassConfig.return_random,
      steps: primaryAutoBackFlickClassConfig.steps,
      cooldown_ms: primaryAutoBackFlickClassConfig.cooldown_ms,
      class_configs: autoBackFlickClassConfigs,
    },
  };
}

function hasInvalidConfigInput() {
  const fields = Array.from(document.querySelectorAll(
    "[data-config][type='number'], .aim-profile-offset-x, .aim-profile-offset-y, .aim-profile-alternate-offset-x, .aim-profile-alternate-offset-y, .aim-profile-class-offset-x, .aim-profile-class-offset-y, .aim-profile-class-priority, .aim-profile-class-force-delay, .aim-profile-sensitivity, .aim-profile-fov-scale, .auto-trigger-profile-enter-delay-min-ms, .auto-trigger-profile-enter-delay-max-ms, .auto-trigger-profile-cooldown-min-ms, .auto-trigger-profile-cooldown-max-ms, .auto-trigger-profile-spray-release-delay-ms"
  ));
  return fields.some((field) => {
    const value = field.value.trim();
    if (value === "") {
      return true;
    }
    if (document.activeElement === field && isPendingNumberText(value)) {
      return true;
    }
    return !Number.isFinite(Number(value));
  });
}

function isPendingNumberText(value) {
  const text = String(value || "").trim();
  return text === "" || text === "-" || text === "+" || text === "." ||
    text === "-." || text === "+." || text.endsWith(".") || /[eE][+-]?$/.test(text);
}

function requestApplyForNumberInput(input, delay = 90) {
  if (isPendingNumberText(input.value)) {
    setApplyStatus("pending", "等待数值");
    return;
  }
  clampNumberInputToLimits(input);
  requestConfigApply(delay);
}

function requestConfigApply(delay = 180) {
  if (!state.configReady || state.isPopulating) {
    return;
  }
  clearTimeout(state.applyTimer);
  setApplyStatus("pending", "待同步");
  state.applyTimer = setTimeout(() => applyConfigNow(), delay);
}

async function applyConfigNow() {
  if (!state.configReady) {
    return;
  }
  clearTimeout(state.applyTimer);
  updateDynamicOffsetControlLimits({ clampValues: true });
  if (hasInvalidConfigInput()) {
    setApplyStatus("pending", "等待数值");
    return;
  }
  if (state.isApplying) {
    state.applyQueued = true;
    return;
  }

  state.isApplying = true;
  setApplyStatus("saving", "同步中");
  try {
    const submittedConfig = collectConfig();
    const result = await api("/api/config", {
      method: "PUT",
      headers: { "Content-Type": "application/json" },
      body: JSON.stringify(submittedConfig),
    });
    if (result && result.config) {
      state.config = result.config;
      if (stableStringify(result.config) !== stableStringify(submittedConfig)) {
        populateForm(result.config);
      }
    }
    if (result && result.state && state.data) {
      state.data = { ...state.data, state: result.state, config: state.config || state.data.config };
      renderRuntime(state.data);
    }
    if (result && Array.isArray(result.models)) {
      const selectedModelId = (state.config && state.config.model_id) || submittedConfig.model_id || "";
      state.data = { ...(state.data || {}), models: result.models, config: state.config || submittedConfig };
      renderModels({ models: result.models, selected_model_id: selectedModelId }, selectedModelId);
    }
    updateAimRangeOverlay();
    const autosaveName = queueCurrentPresetAutosave(state.config || submittedConfig);
    setApplyStatus("ready", autosaveName ? "已同步，保存预设中" : "已同步");
  } catch (error) {
    setApplyStatus("error", "同步失败");
    showToast(error.message || String(error), true);
  } finally {
    state.isApplying = false;
    if (state.applyQueued) {
      state.applyQueued = false;
      requestConfigApply(80);
    }
  }
}

function renderRuntime(payload) {
  if (!payload || !payload.state) {
    return;
  }
  state.data = { ...(state.data || {}), ...payload };
  applyBrand(payload);
  syncAutoStartControls(payload.auto_start);

  const runtime = payload.state || {};
  const capture = runtime.capture || {};
  const detection = runtime.detection || {};
  const aim = runtime.aim || {};
  const latencyState = runtime.latency || {};
  const mouseOutput = runtime.mouse_output || {};
  const controllerConfig = (((payload.config || {}).ai || {}).controller || {});
  renderAutoCalibration({
    runtime: runtime.calibration || {},
    calibration: controllerConfig.calibration || ((state.autoCalibration || {}).calibration || {}),
  });
  renderKmboxStatus(mouseOutput);
  renderCatnetStatus(mouseOutput);
  renderMakcuStatus(mouseOutput);
  renderFerrumStatus(mouseOutput);
  renderKmboxbStatus(mouseOutput);
  renderPhysicalMotionBlockStatus(mouseOutput);
  renderFanControlStatus(runtime.fan_control || {});
  const hotkeyGuardStatus = $("hotkeyGuardRuntimeStatus");
  if (hotkeyGuardStatus) {
    const hotkeyGuardPanel = hotkeyGuardStatus.closest("[data-feature-status]");
    hotkeyGuardStatus.textContent = hotkeyGuardPanel?.dataset.featureStatus === "planned"
      ? "计划中"
      : (aim.hotkeys_suspended ? "已禁用" : "未禁用");
  }
  updateAimProfileOffsetSwitchStatus(aim);
  const badge = $("statusBadge");
  const isRunning = !!runtime.running;
  const isReconnecting = runtime.status === "reconnecting";
  const license = runtime.license || {};
  const licenseValid = !!license.valid;
  state.licenseStatusLoaded = true;
  setLicenseNavigationLock(!licenseValid);
  const lastError = runtime.last_error || aim.last_error || capture.last_error || "";
  const inferenceMs = isRunning ? detection.inference_ms : 0;
  const inferenceFps = isRunning ? detection.inference_fps : 0;
  const captureFps = isRunning ? capture.capture_fps : 0;
  const hailoTemperature = formatHailoTemperature(runtime.hailo_temperature);
  const detections = isRunning ? detection.detections || 0 : 0;
  const tracks = isRunning ? detection.tracks || 0 : 0;
  if (badge) {
    badge.textContent = formatStatus(runtime.status, runtime.running);
    badge.className = `status-badge ${runtime.status === "error" || runtime.status === "locked" ? "error" : (runtime.running || isReconnecting) ? "live" : "idle"}`;
  }
  renderLicensePanel({ license, core: runtime.core || {}, version: state.data && state.data.version });

  const runtimeSummary = $("runtimeSummary");
  if (runtimeSummary) {
    const modelLabel = selectedModelDisplayName(payload, runtime, detection);
    const preprocessBackend = formatPreprocessBackend(latencyState);
    setText("hailoTemperatureValue", hailoTemperature.value);
    runtimeSummary.innerHTML = `
      <div class="runtime-stat">
        <span>当前模型</span>
        <strong>${escapeHtml(formatMaybe(modelLabel, "未选择模型"))}</strong>
      </div>
      <div class="runtime-stat">
        <span>推理状态</span>
        <strong>${isRunning ? (detection.model_loaded ? "模型已加载" : "模型未加载") : "已停止"}</strong>
      </div>
      <div class="runtime-stat">
        <span>采集排队</span>
        <strong>${formatNumber(isRunning ? capture.buffer_age_ms : 0)} ms · ${isRunning ? (capture.last_dequeued_count || 0) : 0}/${capture.buffer_count || 0}</strong>
      </div>
      <div class="runtime-stat">
        <span>预处理路径</span>
        <strong>${escapeHtml(preprocessBackend)}</strong>
      </div>
      <div class="runtime-stat">
        <span>最后错误</span>
        <strong>${escapeHtml(lastError || latencyState.raw_preprocess_error || "无")}</strong>
      </div>
    `;
  }

  const preview = $("previewImage");
  if (preview) {
    const src = runtime.preview_path || "/api/preview.mjpg";
    preview.style.visibility = "visible";
    if (preview.dataset.src !== src) {
      preview.dataset.src = src;
      preview.src = src;
    }
  }
  updateAimRangeOverlay();
  updateTargetBoxOverlay();

  const latency = $("mobileLatency");
  if (latency) {
    const preprocessToTrackMs = positiveNumber(latencyState.preprocess_to_track_ms);
    const inferenceLatencyMs = positiveNumber(inferenceMs);
    const displayLatencyMs = preprocessToTrackMs ?? inferenceLatencyMs;
    latency.textContent = displayLatencyMs !== null ? `${formatNumber(displayLatencyMs)} ms` : "-- ms";
  }
  const mobileCaptureFps = $("mobileCaptureFps");
  if (mobileCaptureFps) {
    mobileCaptureFps.textContent = `${formatNumber(captureFps)} 帧/秒`;
  }
  const fps = $("mobileFps");
  if (fps) {
    fps.textContent = `${formatNumber(inferenceFps)} 帧/秒`;
  }
  const mobileHailoTemperature = $("mobileHailoTemperature");
  if (mobileHailoTemperature) {
    mobileHailoTemperature.textContent = hailoTemperature.value;
  }
  const videoStatus = $("mobileVideoStatus");
  if (videoStatus) {
    videoStatus.textContent = `${detections} 检出 / ${tracks} 跟踪`;
  }

  const startButton = $("startButton");
  if (startButton) {
    const shouldStop = runtime.running || runtime.status === "starting" || isReconnecting;
    startButton.disabled = !licenseValid;
    startButton.className = `power-button ${shouldStop ? "stop" : "start"}`;
    const label = startButton.querySelector("strong");
    if (label) {
      label.textContent = !licenseValid ? "未激活" : shouldStop ? "停止" : "启动";
    }
  }
}

function renderSystemStats(payload) {
  const summary = $("systemSummary");
  if (!summary) {
    return;
  }
  if (!payload) {
    summary.innerHTML = `
      <div class="runtime-stat">
        <span>硬件状态</span>
        <strong>读取失败</strong>
      </div>
    `;
    renderStorageExpansion(null);
    return;
  }

  const memory = payload.memory || {};
  const storage = payload.storage || {};
  const temperature = payload.temperature || {};
  const loadAverage = Array.isArray(payload.load_average) ? payload.load_average : [];
  const cpuTemp = Number(temperature.celsius);
  const loadText = loadAverage.length > 0
    ? loadAverage.map((value) => formatNumber(value, 2)).join(" / ")
    : "--";
  state.systemHostname = payload.hostname || "";
  state.webPort = Number(payload.web_port) || state.webPort || 8080;
  syncLanHostnameInputs(payload);
  syncWebPortInputs(payload);
  const lanUrl = String(payload.lan_url || payload.mdns_url || (payload.lan_ipv4 ? webUrl(payload.lan_ipv4, state.webPort) : "") || "");

  summary.innerHTML = `
    <div class="runtime-stat">
      <span>CPU 占用</span>
      <strong>${formatPercent(payload.cpu_percent)}</strong>
      <small>负载 ${escapeHtml(loadText)}</small>
    </div>
    <div class="runtime-stat">
      <span>内存占用</span>
      <strong>${formatPercent(memory.percent)}</strong>
      <small>${formatBytes(memory.used)} / ${formatBytes(memory.total)}</small>
    </div>
    <div class="runtime-stat">
      <span>CPU 温度</span>
      <strong>${Number.isFinite(cpuTemp) ? `${cpuTemp.toFixed(1)} °C` : "--"}</strong>
      <small>${escapeHtml(temperature.label || "thermal")}</small>
    </div>
    <div class="runtime-stat">
      <span>存储占用</span>
      <strong>${formatPercent(storage.percent)}</strong>
      <small>${formatBytes(storage.used)} / ${formatBytes(storage.total)}</small>
    </div>
    <div class="runtime-stat lan-url-stat">
      <span>局域网 IP</span>
      <strong>${escapeHtml(payload.lan_ipv4 || "--")}</strong>
      <small>${escapeHtml(payload.lan_url || payload.mdns_url || "")}</small>
      <button class="mini-button lan-url-copy-button" type="button" data-copy-lan-url="${escapeAttr(lanUrl)}" ${lanUrl ? "" : "disabled"}>复制</button>
    </div>
    <div class="runtime-stat">
      <span>运行时间</span>
      <strong>${formatDuration(payload.uptime_seconds)}</strong>
      <small>${escapeHtml(payload.hostname || "Orange Pi")}</small>
    </div>
  `;
  renderStorageExpansion(storage);
}

function storageUsage(storage) {
  const rootfs = (storage && storage.rootfs) || {};
  const rootUsage = rootfs.usage || {};
  return {
    total: storage && (storage.root_total ?? rootUsage.total ?? storage.total),
    used: storage && (storage.root_used ?? rootUsage.used ?? storage.used),
    free: storage && (storage.root_free ?? rootUsage.free ?? storage.free),
    percent: storage && (storage.root_percent ?? rootUsage.percent ?? storage.percent),
  };
}

function storageExpandLabel(rootfs) {
  if (!rootfs || Object.keys(rootfs).length === 0) {
    return "未读取";
  }
  if (rootfs.expandable) {
    return "可扩容";
  }
  if (rootfs.ok === false) {
    return "检测失败";
  }
  if (rootfs.reason === "already_expanded") {
    return "已扩容";
  }
  if (rootfs.reason === "filesystem_needs_resize") {
    return "待完成";
  }
  return rootfs.supported === false ? "不可用" : "无需扩容";
}

function storageExpandLog(storage) {
  const rootfs = (storage && storage.rootfs) || {};
  if (!rootfs || Object.keys(rootfs).length === 0) {
    return "暂无扩容信息";
  }
  const root = rootfs.root || {};
  const lines = [
    rootfs.message || "等待检测",
    root.device ? `${root.label || "根分区"}：${root.device}` : "",
    root.disk ? `磁盘：${root.disk}` : "",
    Number.isFinite(Number(root.free_after_partition))
      ? `可扩尾部空间：${formatBytes(root.free_after_partition)}`
      : "",
    rootfs.method ? `扩容方式：${rootfs.method}` : "",
  ].filter(Boolean);
  if (Array.isArray(rootfs.log) && rootfs.log.length > 0) {
    lines.push("", rootfs.log.join("\n\n"));
  }
  return lines.join("\n");
}

function renderStorageExpansion(storage) {
  const pill = $("storageExpandPill");
  const summary = $("storageExpandSummary");
  const button = $("expandStorageButton");
  const log = $("storageExpandLog");
  if (!pill && !summary && !button && !log) {
    return;
  }
  const rootfs = (storage && storage.rootfs) || {};
  const usage = storageUsage(storage || {});
  const expandable = !!rootfs.expandable;
  if (pill) {
    pill.textContent = state.storageExpandBusy ? "扩容中" : storageExpandLabel(rootfs);
    pill.className = `pill${rootfs.ok === false ? " danger-pill" : ""}`;
  }
  if (summary) {
    summary.innerHTML = `
      <div class="runtime-stat">
        <span>根分区容量</span>
        <strong>${formatBytes(usage.used)} / ${formatBytes(usage.total)}</strong>
        <small>已用 ${formatPercent(usage.percent)}</small>
      </div>
      <div class="runtime-stat">
        <span>剩余容量</span>
        <strong>${formatBytes(usage.free)}</strong>
        <small>${escapeHtml(rootfs.message || "等待检测根分区状态")}</small>
      </div>
    `;
  }
  if (button) {
    button.disabled = state.storageExpandBusy || !expandable;
  }
  if (log) {
    log.textContent = storageExpandLog(storage);
  }
}

async function refreshStorageStatus({ toast = false } = {}) {
  const storage = await api(`/api/system/storage${toast ? "?force=1" : ""}`);
  renderStorageExpansion(storage);
  if (toast) {
    showToast("容量状态已刷新");
  }
  return storage;
}

async function expandStorage() {
  const confirmed = window.confirm("扩容会修改当前系统盘分区并扩展文件系统。请确认设备供电稳定，过程中不要断电。确认现在扩容？");
  if (!confirmed) {
    return;
  }
  state.storageExpandBusy = true;
  const log = $("storageExpandLog");
  if (log) {
    log.textContent = "正在扩容根分区，请勿断电。";
  }
  renderStorageExpansion({ rootfs: { ok: true, message: "正在扩容根分区，请勿断电。" } });
  try {
    const storage = await api("/api/system/storage/expand", { method: "POST" });
    renderStorageExpansion(storage);
    const rootfs = (storage && storage.rootfs) || {};
    showToast(rootfs.message || "存储扩容完成");
    await refreshSystemStats().catch(() => {});
  } catch (error) {
    if (error && error.payload) {
      renderStorageExpansion({ rootfs: error.payload });
    }
    throw error;
  } finally {
    state.storageExpandBusy = false;
    await refreshStorageStatus().catch(() => {});
  }
}

function syncLanHostnameInputs(payload) {
  const input = $("lanHostnameInput");
  const hint = $("lanHostnameHint");
  const hostname = (payload && payload.hostname) || "";
  if (input && document.activeElement !== input) {
    input.value = hostname;
  }
  if (hint) {
    const fallbackHost = defaultMdnsHost();
    const mdnsUrl = (payload && payload.mdns_url) || (hostname ? webUrl(`${hostname}.local`) : webUrl(fallbackHost));
    hint.textContent = `用于路由器设备列表和访问 ${mdnsUrl}，路由器列表可能要等网络刷新后更新`;
  }
  updateNetworkAccessButtonState();
}

function webUrl(host, port = state.webPort || 8080) {
  return `http://${host}:${port}/`;
}

function defaultMdnsHost() {
  return `${currentBrandConfig().defaultLocalName}.local`;
}

function displayDefaultHotspotSsid(value = "") {
  const text = String(value || "").trim();
  if (!text || text === "TTBOX") {
    return currentBrandConfig().defaultHotspotSsid;
  }
  return text || currentBrandConfig().defaultHotspotSsid;
}

function brandHotspotSsid(value = "", fallback = "") {
  const text = String(value || "").trim();
  const fallbackText = String(fallback || "").trim();
  if (!text || text === "TTBOX" || text === fallbackText) {
    return displayDefaultHotspotSsid(fallbackText || text);
  }
  return text;
}

function displayDefaultWifiList(payload) {
  const ssids = payload && Array.isArray(payload.default_ssids) ? payload.default_ssids : [];
  const cleaned = ssids.map((item) => String(item || "").trim()).filter(Boolean);
  if (cleaned.length) {
    return cleaned.join(" / ");
  }
  return displayDefaultHotspotSsid(payload && payload.default_ssid);
}

function syncWebPortInputs(payload) {
  const input = $("webPortInput");
  const hint = $("webPortHint");
  const port = Number(payload && payload.web_port) || state.webPort || 8080;
  state.webPort = port;
  if (input && document.activeElement !== input) {
    input.value = String(port);
  }
  if (hint) {
    const url = (payload && (payload.lan_url || payload.mdns_url)) || webUrl(defaultMdnsHost(), port);
    hint.textContent = `当前访问 ${url}；修改后 Web 服务会短暂重启。`;
  }
  updateNetworkAccessButtonState();
}

function validateLanHostname(value) {
  const hostname = String(value || "").trim().toLowerCase();
  if (!/^[a-z0-9]([a-z0-9-]{0,61}[a-z0-9])?$/.test(hostname)) {
    throw new Error("局域网名称只能包含字母、数字和连字符，长度 1-63，且不能以连字符开头或结尾");
  }
  return hostname;
}

function validateWebPort(value) {
  const text = String(value || "").trim();
  if (!/^\d+$/.test(text)) {
    throw new Error("访问端口必须是 1024-65535 的数字");
  }
  const port = Number(text);
  if (!Number.isInteger(port) || port < 1024 || port > 65535) {
    throw new Error("访问端口必须在 1024-65535 之间");
  }
  return port;
}

function networkAccessDraft() {
  const hostnameInput = $("lanHostnameInput");
  const portInput = $("webPortInput");
  const hostname = validateLanHostname(hostnameInput ? hostnameInput.value : "");
  const port = validateWebPort(portInput ? portInput.value : "");
  return {
    hostname,
    port,
    hostnameChanged: hostname !== state.systemHostname,
    portChanged: port !== Number(state.webPort || 8080),
  };
}

function updateNetworkAccessButtonState() {
  const button = $("applyNetworkAccessButton");
  const hostnameInput = $("lanHostnameInput");
  const portInput = $("webPortInput");
  if (!button || !hostnameInput || !portInput) {
    return;
  }
  let disabled = true;
  let title = "";
  try {
    const draft = networkAccessDraft();
    disabled = !draft.hostnameChanged && !draft.portChanged;
    hostnameInput.classList.remove("is-invalid");
    portInput.classList.remove("is-invalid");
  } catch (error) {
    title = error.message || String(error);
    disabled = true;
    hostnameInput.classList.toggle("is-invalid", !HOSTNAME_PATTERN_FOR_UI.test(String(hostnameInput.value || "").trim().toLowerCase()));
    portInput.classList.toggle("is-invalid", !/^\d+$/.test(String(portInput.value || "").trim()) || Number(portInput.value) < 1024 || Number(portInput.value) > 65535);
  }
  button.disabled = disabled;
  button.title = title;
}

const HOSTNAME_PATTERN_FOR_UI = /^[a-z0-9]([a-z0-9-]{0,61}[a-z0-9])?$/;

async function applyNetworkAccessSettings() {
  const draft = networkAccessDraft();
  if (!draft.hostnameChanged && !draft.portChanged) {
    return;
  }
  if (draft.portChanged) {
    const confirmed = window.confirm(`确认把 Web 访问端口修改为 ${draft.port}？页面会短暂断开，需要用新端口重新打开。`);
    if (!confirmed) {
      return;
    }
  }

  let payload = null;
  if (draft.hostnameChanged) {
    payload = await api("/api/system/hostname", {
      method: "PUT",
      headers: { "Content-Type": "application/json" },
      body: JSON.stringify({ hostname: draft.hostname }),
    });
    state.systemHostname = payload.hostname || draft.hostname;
    syncLanHostnameInputs(payload);
    syncWebPortInputs(payload);
  }

  if (draft.portChanged) {
    payload = await api("/api/system/web-port", {
      method: "PUT",
      headers: { "Content-Type": "application/json" },
      body: JSON.stringify({ port: draft.port }),
    });
    state.webPort = Number(payload.web_port) || draft.port;
    syncLanHostnameInputs(payload);
    syncWebPortInputs(payload);
    const nextUrl = payload.lan_url || payload.mdns_url || `${window.location.protocol}//${window.location.hostname}:${state.webPort}/`;
    showToast(`访问设置已应用，请访问 ${nextUrl}`);
    window.setTimeout(() => {
      window.location.href = nextUrl;
    }, 1800);
    return;
  }

  await refreshSystemStats().catch(() => {});
  showToast("网络访问设置已应用");
}

function validateLanBlockIp(value) {
  const text = String(value || "").trim();
  if (!/^(\d{1,3}\.){3}\d{1,3}$/.test(text)) {
    throw new Error("请输入有效的 IPv4 地址");
  }
  const parts = text.split(".").map((part) => Number(part));
  if (parts.some((part) => !Number.isInteger(part) || part < 0 || part > 255)) {
    throw new Error("请输入有效的 IPv4 地址");
  }
  if (parts[0] === 0 || parts[0] === 127 || parts[0] >= 224) {
    throw new Error("不能拉黑本机、组播或无效地址");
  }
  return parts.join(".");
}

function lanDeviceLabel(device) {
  const bits = [device.ip || ""];
  if (device.mac) bits.push(device.mac);
  if (device.interface) bits.push(device.interface);
  if (device.state) bits.push(device.state);
  return bits.filter(Boolean).join(" · ");
}

function selectedLanBlockIp() {
  const input = $("lanBlockIpInput");
  return validateLanBlockIp(input ? input.value : "");
}

function updateLanBlockButtonState(payload = null) {
  const applyButton = $("applyLanBlockButton");
  const clearButton = $("clearLanBlockButton");
  const input = $("lanBlockIpInput");
  let valid = false;
  let title = "";
  try {
    selectedLanBlockIp();
    valid = true;
    if (input) input.classList.remove("is-invalid");
  } catch (error) {
    title = error.message || String(error);
    if (input) input.classList.toggle("is-invalid", !!String(input.value || "").trim());
  }
  if (applyButton) {
    applyButton.disabled = !valid;
    applyButton.title = title;
  }
  if (clearButton && payload) {
    clearButton.disabled = !((payload.blocked_ips || []).length);
  }
}

function renderLanBlocklist(payload) {
  const pill = $("lanBlockStatusPill");
  const summary = $("lanBlockSummary");
  const select = $("lanBlockDeviceSelect");
  const hint = $("lanBlockDeviceHint");
  const clearButton = $("clearLanBlockButton");
  const blocked = (payload && payload.blocked_ips) || [];
  const devices = (payload && payload.devices) || state.lanBlockDevices || [];
  state.lanBlockDevices = devices;
  if (pill) {
    if (!payload) {
      pill.textContent = "未读取";
      pill.className = "pill danger-pill";
    } else if (!payload.supported) {
      pill.textContent = "不可用";
      pill.className = "pill danger-pill";
    } else if (blocked.length) {
      pill.textContent = "已启用";
      pill.className = "pill";
    } else {
      pill.textContent = "未启用";
      pill.className = "pill";
    }
  }
  if (summary) {
    const current = blocked[0] || "--";
    const message = payload ? (payload.message || "未设置局域网黑名单") : "读取失败";
    summary.innerHTML = `
      <div class="runtime-stat">
        <span>当前拉黑 IP</span>
        <strong>${escapeHtml(current)}</strong>
        <small>${escapeHtml(message)}</small>
      </div>
      <div class="runtime-stat">
        <span>发现设备</span>
        <strong>${devices.length ? `${devices.length} 个` : "--"}</strong>
        <small>同网段设备可能需要先扫描一次</small>
      </div>
    `;
  }
  if (select) {
    const currentValue = select.value;
    select.innerHTML = `<option value="">选择扫描到的设备</option>${devices.map((device) => {
      return `<option value="${escapeAttr(device.ip)}">${escapeHtml(lanDeviceLabel(device))}</option>`;
    }).join("")}`;
    if (devices.some((device) => device.ip === currentValue)) {
      select.value = currentValue;
    }
  }
  if (hint) {
    hint.textContent = devices.length
      ? "选择一个设备后点击拉黑；被拉黑 IP 将无法访问本机。"
      : "点击扫描设备后，从列表里选择要拉黑的 IP。";
  }
  if (clearButton) {
    clearButton.disabled = !blocked.length;
  }
  updateLanBlockButtonState(payload);
}

async function refreshLanBlocklist() {
  try {
    renderLanBlocklist(await api("/api/system/lan-blocklist"));
  } catch {
    renderLanBlocklist(null);
  }
}

async function scanLanDevices() {
  const payload = await api("/api/system/lan-blocklist/scan", { method: "POST" });
  renderLanBlocklist(payload);
  showToast((payload.devices || []).length ? "局域网设备已刷新" : "未发现可选择设备");
}

async function applyLanBlock() {
  const ip = selectedLanBlockIp();
  const confirmed = window.confirm(`确认拉黑 ${ip}？该设备将无法访问本机网页和服务。`);
  if (!confirmed) {
    return;
  }
  const payload = await api("/api/system/lan-blocklist", {
    method: "POST",
    headers: { "Content-Type": "application/json" },
    body: JSON.stringify({ ip }),
  });
  renderLanBlocklist(payload);
  showToast(`已拉黑 ${ip}`);
}

async function clearLanBlock() {
  const confirmed = window.confirm("确认清除局域网黑名单？");
  if (!confirmed) {
    return;
  }
  const payload = await api("/api/system/lan-blocklist", { method: "DELETE" });
  renderLanBlocklist(payload);
  showToast("局域网黑名单已清除");
}

function wifiSecurityLabel(value) {
  const text = String(value || "").trim();
  if (!text || text.toLowerCase() === "open") {
    return "开放";
  }
  return text;
}

function wifiRequiresPassword(network) {
  const security = String((network && network.security) || "").trim().toLowerCase();
  return !!security && security !== "open";
}

function wifiApPayload(payload = state.wifi) {
  return (payload && payload.ap) || {};
}

function wifiIsApMode(payload = state.wifi) {
  const ap = wifiApPayload(payload);
  return payload && (payload.mode === "ap" || ap.active);
}

function wifiApAddress(ap = wifiApPayload()) {
  return ap.ip4 || "10.42.0.1";
}

function wifiApAccessHint(ap = wifiApPayload()) {
  return ap.gateway_url || webUrl(wifiApAddress(ap));
}

function wifiIpText(payload) {
  if (wifiIsApMode(payload)) {
    return wifiApAddress(wifiApPayload(payload));
  }
  const connected = payload && payload.connected;
  return (connected && connected.ip4) || "--";
}

function wifiStatusText(payload) {
  if (!payload) {
    return "读取失败";
  }
  if (!payload.available) {
    return "无无线网卡";
  }
  if (wifiIsApMode(payload)) {
    return "AP 热点";
  }
  if (payload.connected && payload.connected.ssid) {
    return "已连接";
  }
  if (payload.ethernet_connected) {
    return "有线在线";
  }
  return "待连接";
}

function wifiCurrentText(payload) {
  if (!payload) {
    return "--";
  }
  if (wifiIsApMode(payload)) {
    const ap = wifiApPayload(payload);
    return brandHotspotSsid(ap.ssid, ap.default_ssid) || "AP 热点";
  }
  if (payload.connected && payload.connected.ssid) {
    return payload.connected.ssid;
  }
  return payload.ethernet_connected ? "有线网络" : "未连接";
}

function setWifiPill(id, payload) {
  const pill = $(id);
  if (!pill) {
    return;
  }
  const danger = !payload || !payload.available || (!!payload.error && !(payload.connected && payload.connected.ssid) && !wifiIsApMode(payload));
  pill.textContent = wifiStatusText(payload);
  pill.className = `pill${danger ? " danger-pill" : ""}`;
}

function wifiNetworks(payload) {
  return payload && Array.isArray(payload.networks) ? payload.networks : [];
}

function selectedWifiNetwork(payload = state.wifi) {
  return wifiNetworks(payload).find((network) => network.ssid === state.wifiSelectedSsid) || null;
}

function normalizeWifiSelection(payload) {
  if (wifiIsApMode(payload)) {
    return;
  }
  const networks = wifiNetworks(payload);
  const connectedSsid = payload && payload.connected && payload.connected.ssid;
  const selectedExists = networks.some((network) => network.ssid === state.wifiSelectedSsid);
  if (!state.wifiSelectedSsid || !selectedExists) {
    state.wifiSelectedSsid = connectedSsid || (networks[0] && networks[0].ssid) || "";
  }
}

function renderWifiSummary(id, payload, compact = false) {
  const summary = $(id);
  if (!summary) {
    return;
  }
  if (!payload) {
    summary.innerHTML = `
      <div class="runtime-stat">
        <span>无线状态</span>
        <strong>读取失败</strong>
      </div>
    `;
    return;
  }
  const error = payload.error || "";
  const ap = wifiApPayload(payload);
  if (compact) {
    summary.innerHTML = `
      <div class="runtime-stat">
        <span>当前网络</span>
        <strong>${escapeHtml(wifiCurrentText(payload))}</strong>
        <small>${escapeHtml(error || (payload.connected ? "Wi-Fi 已连接" : "等待连接"))}</small>
      </div>
      <div class="runtime-stat">
        <span>设备地址</span>
        <strong>${escapeHtml(wifiIpText(payload))}</strong>
        <small>${escapeHtml(payload.interface || displayDefaultHotspotSsid(payload.default_ssid))}</small>
      </div>
    `;
    return;
  }
  summary.innerHTML = `
    <div class="runtime-stat">
      <span>无线网卡</span>
      <strong>${payload.available ? escapeHtml(payload.interface || "已检测") : "未检测到"}</strong>
      <small>${escapeHtml(error || payload.manager || "NetworkManager")}</small>
    </div>
    <div class="runtime-stat">
      <span>有线状态</span>
      <strong>${payload.ethernet_connected ? "已连接" : "未连接"}</strong>
      <small>${wifiIsApMode(payload) ? (payload.ethernet_connected ? "热点共享有线网络" : "热点仅本地访问") : (payload.ethernet_connected ? "不会强制切换 Wi-Fi" : `默认回退 ${escapeHtml(displayDefaultWifiList(payload))}`)}</small>
    </div>
    <div class="runtime-stat">
      <span>当前网络</span>
      <strong>${escapeHtml(wifiCurrentText(payload))}</strong>
      <small>${wifiIsApMode(payload) ? "AP 热点模式" : (payload.connected && payload.connected.signal !== null ? `${payload.connected.signal}% 信号` : "--")}</small>
    </div>
    <div class="runtime-stat">
      <span>设备地址</span>
      <strong>${escapeHtml(wifiIpText(payload))}</strong>
      <small>${escapeHtml(wifiIsApMode(payload) ? wifiApAccessHint(ap) : (payload.connected && payload.connected.connection || defaultMdnsHost()))}</small>
    </div>
  `;
}

function renderWifiSelect(id, payload) {
  const select = $(id);
  if (!select) {
    return;
  }
  const networks = wifiNetworks(payload);
  if (!payload || !payload.available) {
    select.innerHTML = '<option value="">无无线网卡</option>';
    select.disabled = true;
    return;
  }
  if (networks.length === 0) {
    select.innerHTML = '<option value="">未扫描到 Wi-Fi</option>';
    select.disabled = true;
    return;
  }
  select.disabled = false;
  select.innerHTML = networks.map((network) => {
    const active = network.active ? " / 已连接" : "";
    const lock = wifiRequiresPassword(network) ? " · 锁" : "";
    const label = `${network.ssid} · ${network.signal}% · ${wifiSecurityLabel(network.security)}${lock}${active}`;
    return `<option value="${escapeAttr(network.ssid)}">${escapeHtml(label)}</option>`;
  }).join("");
  select.value = state.wifiSelectedSsid;
}

function renderWifiNetworkList(payload) {
  const list = $("wifiNetworkList");
  if (!list) {
    return;
  }
  const networks = wifiNetworks(payload);
  if (!payload || !payload.available) {
    list.innerHTML = '<span class="wifi-empty">未检测到无线网卡</span>';
    return;
  }
  if (networks.length === 0) {
    list.innerHTML = '<span class="wifi-empty">未扫描到 Wi-Fi</span>';
    return;
  }
  list.innerHTML = networks.map((network) => {
    const active = network.active ? " is-active" : "";
    const selected = network.ssid === state.wifiSelectedSsid ? " is-selected" : "";
    const meta = `${network.signal}% · ${wifiSecurityLabel(network.security)}${network.active ? " · 已连接" : ""}`;
    const lock = wifiRequiresPassword(network) ? '<span class="wifi-lock" aria-label="需要密码">锁</span>' : "";
    return `
      <button class="wifi-network-button${active}${selected}" type="button" data-wifi-ssid="${escapeAttr(network.ssid)}">
        <strong>${escapeHtml(network.ssid)}${lock}</strong>
        <small>${escapeHtml(meta)}</small>
      </button>
    `;
  }).join("");
  list.querySelectorAll("[data-wifi-ssid]").forEach((button) => {
    button.addEventListener("click", () => {
      state.wifiSelectedSsid = button.dataset.wifiSsid || "";
      renderWifiPanels(state.wifi);
    });
  });
}

function setWifiModePanel(mode) {
  state.wifiModePanel = mode === "ap" ? "ap" : "client";
  const clientPanel = $("wifiClientPanel");
  const apPanel = $("wifiApPanel");
  const clientButton = $("wifiClientModeButton");
  const apButton = $("wifiApModeButton");
  if (clientPanel) {
    clientPanel.hidden = state.wifiModePanel !== "client";
  }
  if (apPanel) {
    apPanel.hidden = state.wifiModePanel !== "ap";
  }
  if (clientButton) {
    clientButton.classList.toggle("is-active", state.wifiModePanel === "client");
  }
  if (apButton) {
    apButton.classList.toggle("is-active", state.wifiModePanel === "ap");
  }
}

function maybeInitializeWifiApInputs(payload) {
  if (!payload || state.wifiApCredentialsInitialized) {
    return;
  }
  const ap = wifiApPayload(payload);
  const ssidInput = $("wifiApSsid");
  const passwordInput = $("wifiApPassword");
  if (ssidInput) {
    ssidInput.value = brandHotspotSsid(ap.ssid, ap.default_ssid);
  }
  if (passwordInput) {
    passwordInput.value = ap.default_password || "12345678";
  }
  state.wifiApCredentialsInitialized = true;
}

function renderWifiApSummary(payload) {
  const summary = $("wifiApSummary");
  if (!summary) {
    return;
  }
  if (!payload || !payload.available) {
    summary.innerHTML = '<span class="wifi-empty">未检测到无线网卡</span>';
    return;
  }
  const ap = wifiApPayload(payload);
  const supportText = ap.supported ? "支持 AP" : "不可用";
  const activeText = ap.active ? "已开启" : (ap.autoconnect ? "重启后开启" : "未开启");
  const accessUrl = wifiApAccessHint(ap);
  summary.innerHTML = `
    <div class="runtime-stat">
      <span>AP 支持</span>
      <strong>${escapeHtml(supportText)}</strong>
      <small>${escapeHtml(ap.support_error || payload.interface || "--")}</small>
    </div>
    <div class="runtime-stat">
      <span>热点状态</span>
      <strong>${escapeHtml(activeText)}</strong>
      <small>${escapeHtml(brandHotspotSsid(ap.ssid, ap.default_ssid))}</small>
    </div>
    <div class="runtime-stat">
      <span>热点地址</span>
      <strong>${escapeHtml(wifiApAddress(ap))}</strong>
      <small>${escapeHtml(accessUrl)}</small>
    </div>
    <div class="runtime-stat">
      <span>共享网络</span>
      <strong>${payload.ethernet_connected ? "有线共享" : "本地访问"}</strong>
      <small>${payload.ethernet_connected ? "热点客户端可尝试访问外网" : "连接热点后访问前端"}</small>
    </div>
  `;
}

function renderWifiPanels(payload) {
  state.wifi = payload;
  if (payload && wifiIsApMode(payload)) {
    state.wifiModePanel = "ap";
  }
  normalizeWifiSelection(payload);
  maybeInitializeWifiApInputs(payload);
  setWifiPill("wifiStatusPill", payload);
  renderWifiSummary("wifiSummary", payload, false);
  renderWifiSelect("wifiNetworkSelect", payload);
  renderWifiNetworkList(payload);
  renderWifiApSummary(payload);
  setWifiModePanel(state.wifiModePanel);
  updateWifiConnectButtonState();
  updateWifiApButtonState();
}

function updateWifiConnectButtonState() {
  const connectButton = $("wifiConnectButton");
  if (connectButton) {
    const network = selectedWifiNetwork();
    const needsPassword = wifiRequiresPassword(network);
    const passwordReady = !needsPassword || wifiPassword("wifiPassword").trim().length >= 8;
    const disabled = !state.wifi || !state.wifi.available || !network || !passwordReady;
    connectButton.disabled = disabled;
    connectButton.title = needsPassword && !passwordReady ? "请输入至少 8 位 Wi-Fi 密码" : "";
  }
  const fallback = $("wifiFallbackButton");
  if (fallback) {
    fallback.disabled = !state.wifi || !state.wifi.available || wifiIsApMode(state.wifi);
  }
}

function updateWifiApButtonState() {
  const ap = wifiApPayload();
  const applyButton = $("wifiApApplyButton");
  const ssidInput = $("wifiApSsid");
  const passwordInput = $("wifiApPassword");
  const ssid = ssidInput ? ssidInput.value.trim() : "";
  const password = passwordInput ? passwordInput.value : "";
  const ssidBytes = new TextEncoder().encode(ssid).length;
  const credentialsReady = ssidBytes >= 1 && ssidBytes <= 32 && password.length >= 8 && password.length <= 63;
  if (applyButton) {
    applyButton.disabled = !state.wifi || !state.wifi.available || !ap.supported || !credentialsReady;
    applyButton.title = !ap.supported
      ? (ap.support_error || "当前无线网卡不支持 AP 模式")
      : (!credentialsReady ? "热点名称需 1-32 字节，密码需 8-63 位" : "");
  }
  const clientButton = $("wifiClientActivateButton");
  if (clientButton) {
    clientButton.disabled = !state.wifi || !state.wifi.available || !wifiIsApMode(state.wifi);
  }
}

async function refreshWifi(forceScan = false) {
  const payload = forceScan
    ? await api("/api/network/wifi/scan", { method: "POST" })
    : await api("/api/network/wifi");
  renderWifiPanels(payload);
  return payload;
}

function selectedWifiSsid(selectId) {
  const select = $(selectId);
  return (select && select.value) || state.wifiSelectedSsid || "";
}

function wifiPassword(inputId) {
  const input = $(inputId);
  return input ? input.value : "";
}

function clearWifiPasswords() {
  const input = $("wifiPassword");
  if (input) {
    input.value = "";
  }
}

function validateWifiApInputs() {
  const ssidInput = $("wifiApSsid");
  const passwordInput = $("wifiApPassword");
  const ssid = brandHotspotSsid(ssidInput ? ssidInput.value : "", state.wifi && state.wifi.ap && state.wifi.ap.default_ssid);
  const password = passwordInput ? passwordInput.value : "";
  const ssidBytes = new TextEncoder().encode(ssid).length;
  if (ssidBytes < 1 || ssidBytes > 32) {
    throw new Error("热点名称需 1-32 字节");
  }
  if (password.length < 8 || password.length > 63) {
    throw new Error("热点密码需 8-63 位");
  }
  return { ssid, password };
}

async function connectWifiFrom(selectId, passwordId) {
  const ssid = selectedWifiSsid(selectId);
  if (!ssid) {
    throw new Error("请选择 Wi-Fi");
  }
  const network = selectedWifiNetwork();
  if (wifiRequiresPassword(network) && wifiPassword(passwordId).trim().length < 8) {
    throw new Error("请输入至少 8 位 Wi-Fi 密码");
  }
  try {
    const payload = await api("/api/network/wifi/connect", {
      method: "POST",
      headers: { "Content-Type": "application/json" },
      body: JSON.stringify({ ssid, password: wifiPassword(passwordId) }),
    });
    renderWifiPanels(payload);
    clearWifiPasswords();
    showToast(`Wi-Fi 已连接：${ssid}`);
  } catch (error) {
    if (error instanceof TypeError) {
      showToast(`Wi-Fi 切换中，请连到同一网络后访问 ${defaultMdnsHost()}`);
      return;
    }
    await refreshWifi(false).catch(() => {});
    throw error;
  }
}

async function fallbackWifi() {
  const payload = await api("/api/network/wifi/fallback", { method: "POST" });
  renderWifiPanels(payload);
  clearWifiPasswords();
  showToast("已重置默认 Wi-Fi");
}

async function applyWifiApHotspot() {
  const credentials = validateWifiApInputs();
  try {
    const payload = await api("/api/network/wifi/ap/apply", {
      method: "POST",
      headers: { "Content-Type": "application/json" },
      body: JSON.stringify(credentials),
    });
    renderWifiPanels(payload);
    setWifiModePanel("ap");
    const ap = wifiApPayload(payload);
    showToast(`AP 热点已开启：${ap.ssid || credentials.ssid}`);
  } catch (error) {
    if (error instanceof TypeError) {
      showToast(`AP 热点切换中，请连接热点后访问 ${webUrl("10.42.0.1")} 或 ${webUrl(defaultMdnsHost())}`);
      return;
    }
    await refreshWifi(false).catch(() => {});
    throw error;
  }
}

async function activateWifiClientMode() {
  const payload = await api("/api/network/wifi/client/activate", { method: "POST" });
  renderWifiPanels(payload);
  setWifiModePanel("client");
  showToast("已切回 Wi-Fi 模式");
}

function initWifiPolling() {
  refreshWifi(false).catch(() => renderWifiPanels(null));
  setInterval(() => {
    refreshWifi(false).catch(() => renderWifiPanels(null));
  }, 7000);
}

async function refreshSystemStats() {
  try {
    renderSystemStats(await api("/api/system"));
  } catch {
    renderSystemStats(null);
  }
}

function initSystemPolling() {
  refreshSystemStats();
  setInterval(refreshSystemStats, 2500);
}

function renderLicensePanel(payload) {
  const license = (payload && payload.license) || {};
  state.currentVersion = (payload && payload.version) || state.currentVersion || "";
  applyBrand(payload);
  const statusPill = $("licenseStatusPill");
  const plan = $("licensePlan");
  const summary = $("licenseSummary");
  const installButton = $("installUpdateButton");
  const activated = !!license.valid;
  const recovery = (payload && payload.recovery) || {};
  const recoveryMessage = recovery.message || "";
  const statusText = recovery.recovered
    ? "已自动修复当前设备授权"
    : recoveryMessage
      ? friendlyLicenseMessage(recoveryMessage, recoveryMessage)
      : friendlyLicenseMessage(license.message, activated ? "已激活" : "未激活");
  if (statusPill) {
    statusPill.textContent = activated ? "已激活" : "未激活";
    statusPill.className = `pill${activated ? "" : " danger-pill"}`;
  }
  if (plan) {
    plan.textContent = license.plan === "permanent"
      ? "永久授权"
      : (license.plan === "trial" ? "试用授权" : (license.plan || "未授权"));
  }
  if (summary) {
    const expiresAt = license.expires_at ? formatDateTime(license.expires_at) : "--";
    summary.innerHTML = `
      <div class="runtime-stat">
        <span>授权状态</span>
        <strong>${escapeHtml(statusText)}</strong>
      </div>
      <div class="runtime-stat">
        <span>到期时间</span>
        <strong>${escapeHtml(expiresAt)}</strong>
      </div>
      <div class="runtime-stat">
        <span>当前版本</span>
        <strong>${escapeHtml((payload && payload.version) || "--")}</strong>
      </div>
    `;
  }
  if (installButton && !state.updatePlan) {
    installButton.disabled = true;
  }
  updateLicenseGateStatus(statusText || (activated ? "已激活" : "设备未激活，请输入激活码后继续使用。"));
}

async function refreshLicenseStatus() {
  const payload = await api("/api/license");
  renderLicensePanel(payload);
  if (state.data && state.data.state) {
    state.data.state.license = payload.license;
    state.data.state.core = payload.core || state.data.state.core;
    renderRuntime(state.data);
  }
  if (payload && payload.auto_recovered) {
    showToast("已自动修复当前设备授权");
    return;
  }
  showToast("授权状态已刷新");
}

function needsLicenseRecovery(payload) {
  const runtime = (payload && payload.state) || {};
  const license = runtime.license || (payload && payload.license) || {};
  const core = runtime.core || (payload && payload.core) || {};
  const text = `${license.status || ""} ${core.status || ""} ${license.message || ""} ${core.message || ""}`;
  return text.includes("device_mismatch") || text.includes("不属于当前设备");
}

function noteLicenseRecoveryResult(payload) {
  const recovery = payload && payload.recovery;
  if (!recovery || !recovery.message || recovery.message === state.lastLicenseRecoveryMessage) {
    return payload;
  }
  state.lastLicenseRecoveryMessage = recovery.message;
  showToast(recovery.message, !recovery.recovered);
  return payload;
}

async function maybeRunLicenseRecovery(payload) {
  noteLicenseRecoveryResult(payload);
  if (payload && payload.recovery && payload.recovery.message) {
    return payload;
  }
  if (!needsLicenseRecovery(payload) || state.licenseRecoveryInProgress) {
    return payload;
  }
  state.licenseRecoveryInProgress = true;
  state.lastLicenseRecoveryMessage = "检测到授权绑定信息变化，正在自动修复授权。";
  updateLicenseGateStatus(state.lastLicenseRecoveryMessage);
  setActivationSetupProgress(10, 0, state.lastLicenseRecoveryMessage);
  try {
    const recovered = await api("/api/license");
    const message = recovered && recovered.recovery && recovered.recovery.message
      ? recovered.recovery.message
      : recovered && recovered.auto_recovered
        ? "已自动修复当前设备授权"
        : "";
    if (message) {
      state.lastLicenseRecoveryMessage = message;
      showToast(message, !(recovered && recovered.auto_recovered));
    }
    if (recovered && recovered.auto_recovered) {
      const fresh = await api("/api/state");
      clearActivationSetupProgress();
      return fresh;
    }
  } catch (error) {
    state.lastLicenseRecoveryMessage = error.message || String(error);
  } finally {
    state.licenseRecoveryInProgress = false;
    clearActivationSetupProgress();
  }
  return payload;
}

async function activateLicenseFromInput(sourceInputId = "licenseKeyInput") {
  const sourceInput = $(sourceInputId) || $("licenseKeyInput") || $("licenseGateKeyInput");
  syncLicenseKeyInputs(sourceInput);
  const licenseKey = (sourceInput && sourceInput.value.trim()) || "";
  if (!licenseKey) {
    throw new Error("请输入卡密");
  }
  let activationSaved = false;
  setActivationBusy(true);
  setActivationSetupProgress(6, 0, "正在验证激活码并下载授权组件。");
  updateLicenseGateStatus("正在验证激活码，请稍候。");
  await nextPaint();
  try {
    const result = await api("/api/license/activate", {
      method: "POST",
      headers: { "Content-Type": "application/json" },
      body: JSON.stringify({ license_key: licenseKey }),
    });
    activationSaved = true;
    setActivationSetupProgress(16, 0, "授权验证完成，正在保存核心模块和鼠标输出组件。");
    renderLicensePanel({ license: result.license, core: result.core, version: result.version });
    if (state.data && state.data.state) {
      state.data.state.license = result.license;
      state.data.state.core = result.core || state.data.state.core;
      renderRuntime(state.data);
    }
    updateDisclaimerActions();
    ["licenseKeyInput", "licenseGateKeyInput"].forEach((id) => {
      const input = $(id);
      if (input) {
        input.value = "";
      }
    });
    await nextPaint();
    await runFirstActivationSetup();
    const installedUpdate = await installFullUpdateAfterActivation();
    if (installedUpdate) {
      setActivationSetupProgress(96, 4, "完整更新包已安装，服务正在恢复。");
      showToast("设备已激活，完整更新包正在安装");
      window.setTimeout(() => refreshAll().catch(() => {}), 3500);
      return;
    }
    setActivationSetupProgress(100, 4, "初始化完成，正在刷新设备状态。");
    await sleep(650);
    await refreshAll();
    clearActivationSetupProgress();
    showToast("设备已激活");
  } catch (error) {
    if (activationSaved) {
      await refreshAll().catch(() => {});
    }
    clearActivationSetupProgress();
    throw error;
  } finally {
    setActivationBusy(false);
  }
}

function renderUpdateResult(payload) {
  const pill = $("updateStatusPill");
  const summary = $("updateSummary");
  const notes = $("updateReleaseNotes");
  const installButton = $("installUpdateButton");
  const components = (payload && payload.components) || {};
  const core = components.core || {};
  const usbProxy = components.usb_proxy || {};
  const hasAppUpdate = !!(payload && payload.package);
  const hasCoreUpdate = !!core.update_available;
  const hasUsbProxyUpdate = !!usbProxy.update_available;
  const themeUpdates = Array.isArray(payload && payload.theme_updates) ? payload.theme_updates : [];
  const themeCompatibility = Array.isArray(payload && payload.theme_compatibility) ? payload.theme_compatibility : [];
  const themeFallback = payload && payload.theme_fallback && typeof payload.theme_fallback === "object"
    ? payload.theme_fallback
    : null;
  const hasThemeUpdate = themeUpdates.length > 0;
  const hasUpdate = !!(payload && payload.update_available && (hasAppUpdate || hasCoreUpdate || hasUsbProxyUpdate || hasThemeUpdate));
  if (payload && payload.current_version_reconciled && payload.current_version) {
    const currentVersion = String(payload.current_version);
    state.currentVersion = currentVersion;
    if (state.data) {
      state.data.version = currentVersion;
    }
    const runtime = (state.data && state.data.state) || {};
    renderLicensePanel({ license: runtime.license || {}, core: runtime.core || {}, version: currentVersion });
  }
  state.updatePlan = hasUpdate ? {
    latest_version: payload.latest_version || "",
    package: payload.package || null,
    components,
    theme_updates: themeUpdates,
    theme_compatibility: themeCompatibility,
    theme_fallback: themeFallback,
  } : null;
  if (pill) {
    pill.textContent = hasUpdate ? "发现更新" : "已是最新";
  }
  if (summary) {
    const componentText = [
      hasAppUpdate ? "应用" : "",
      hasCoreUpdate ? "核心" : "",
      hasUsbProxyUpdate ? "USB" : "",
      hasThemeUpdate ? `主题(${themeUpdates.length})` : "",
    ].filter(Boolean).join(" / ") || "--";
    const themeUpdateText = themeUpdates.map((item) => {
      const title = item.title || item.theme_id || "主题";
      const current = item.current_version ? `${item.current_version} → ` : "";
      return `${title} ${current}${item.version || "--"}`;
    }).join("；");
    const themeNotice = themeFallback
      ? `${themeFallback.title || themeFallback.theme_id || "当前主题"} 与目标系统不兼容，升级完成后将切回默认主题`
      : themeUpdateText;
    summary.innerHTML = `
      <div class="runtime-stat">
        <span>最新版本</span>
        <strong>${escapeHtml((payload && payload.latest_version) || "--")}</strong>
      </div>
      <div class="runtime-stat">
        <span>组件更新</span>
        <strong>${escapeHtml(componentText)}</strong>
      </div>
      ${themeNotice ? `<div class="runtime-stat"><span>主题计划</span><strong${themeFallback ? ' class="update-theme-warning"' : ""}>${escapeHtml(themeNotice)}</strong></div>` : ""}
    `;
  }
  if (notes) {
    notes.textContent = (payload && (payload.release_notes || payload.notes)) || "暂无更新信息";
  }
  if (installButton) {
    installButton.disabled = !hasUpdate;
  }
  if (!state.updateStatus || state.updateStatus.status !== "running") {
    renderUpdateStatus({
      status: "idle",
      progress: 0,
      message: hasUpdate ? "已发现可安装更新，点击安装更新开始。" : "暂无更新任务。",
    });
  }
}

function updatePackageLabel(item) {
  const version = item && item.version ? String(item.version) : "--";
  return version;
}

function selectedUpdateVersionPackage() {
  const select = $("updateVersionSelect");
  const version = select ? select.value : "";
  return state.updateVersions.find((item) => String(item.version || "") === version) || null;
}

function updateVersionSelectionSummary() {
  const item = selectedUpdateVersionPackage();
  const hint = $("updateVersionHint");
  const confirmButton = $("confirmUpdateVersionButton");
  const current = state.currentVersion || (state.data && state.data.version) || "";
  if (confirmButton) {
    confirmButton.disabled = !item;
  }
  if (!item) {
    if (hint) {
      hint.textContent = "服务器没有返回可切换版本";
    }
    return;
  }
  if (hint) {
    hint.textContent = current
      ? `当前 ${current}，目标 ${item.version || "--"}`
      : `目标 ${item.version || "--"}`;
  }
}

function setUpdateVersionDialogOpen(open) {
  const dialog = $("updateVersionDialog");
  if (!dialog) {
    return;
  }
  dialog.hidden = !open;
  setAnyModalOpen();
  if (open) {
    const select = $("updateVersionSelect");
    if (select) {
      select.focus();
    }
  }
}

function renderUpdateVersionDialog(payload) {
  state.updateVersionPayload = payload || {};
  state.updateVersions = Array.isArray(payload && payload.versions) ? payload.versions : [];
  const current = state.currentVersion || (state.data && state.data.version) || "";
  const currentLabel = $("updateVersionCurrent");
  const select = $("updateVersionSelect");
  if (currentLabel) {
    currentLabel.textContent = `当前版本 ${current || "--"}`;
  }
  if (select) {
    if (state.updateVersions.length === 0) {
      select.innerHTML = `<option value="">无可用版本</option>`;
      select.disabled = true;
    } else {
      select.disabled = false;
      select.innerHTML = state.updateVersions.map((item) => {
        return `<option value="${escapeAttr(item.version || "")}">${escapeHtml(updatePackageLabel(item))}</option>`;
      }).join("");
      const firstDifferent = state.updateVersions.find((item) => current && item.version !== current);
      select.value = (firstDifferent || state.updateVersions[0] || {}).version || "";
    }
  }
  updateVersionSelectionSummary();
}

async function openUpdateVersionDialog() {
  const button = $("switchUpdateVersionButton");
  if (button) {
    button.disabled = true;
  }
  try {
    const payload = await api("/api/update/versions", {
      method: "POST",
      headers: { "Content-Type": "application/json" },
      body: JSON.stringify({}),
    });
    renderUpdateVersionDialog(payload);
    setUpdateVersionDialogOpen(true);
  } finally {
    if (button) {
      button.disabled = false;
    }
  }
}

async function switchToSelectedUpdateVersion() {
  const item = selectedUpdateVersionPackage();
  if (!item || !item.version) {
    throw new Error("请选择目标版本");
  }
  const current = state.currentVersion || (state.data && state.data.version) || "";
  const relation = current ? compareVersionText(item.version, current) : 0;
  const confirmed = window.confirm(
    relation < 0
      ? `确认切换到旧版本 ${item.version}？安装过程会重启本地服务。`
      : `确认切换到版本 ${item.version}？安装过程会重启本地服务。`
  );
  if (!confirmed) {
    return;
  }
  const confirmButton = $("confirmUpdateVersionButton");
  if (confirmButton) {
    confirmButton.disabled = true;
  }
  try {
    const update = await checkUpdate({ preferFull: true, targetVersion: item.version });
    renderUpdateResult(update);
    if (!updatePayloadHasInstallableUpdate(update)) {
      showToast("选中版本已是当前版本");
      return;
    }
    const plan = {
      latest_version: update.latest_version || item.version,
      package: update.package || null,
      components: update.components || {},
      theme_updates: update.theme_updates || [],
      theme_compatibility: update.theme_compatibility || [],
      theme_fallback: update.theme_fallback || null,
      target_version: item.version,
    };
    if (plan.theme_fallback && !window.confirm(
      `${plan.theme_fallback.title || plan.theme_fallback.theme_id || "当前收费主题"} 与目标版本不兼容。继续切换系统版本会改用默认主题，已购买权益和已安装文件会保留。确认继续？`
    )) {
      return;
    }
    state.updatePlan = plan;
    setUpdateVersionDialogOpen(false);
    const result = await installUpdatePlan(plan);
    renderUpdateStatus({
      status: "running",
      stage: "scheduled",
      message: result.message || "版本切换任务已启动，正在安装",
      progress: 65,
      version: result.version || plan.latest_version || "",
      type: result.type || "",
      unit: result.unit || "",
    });
    showToast("版本切换任务已启动");
  } finally {
    if (confirmButton) {
      confirmButton.disabled = false;
    }
  }
}

function updateStatusLabel(status) {
  const value = String((status && status.status) || "idle");
  if (value === "running") return "更新中";
  if (value === "success") return "更新成功";
  if (value === "failed") return "更新失败";
  return "待检查";
}

function renderUpdateStatus(status) {
  const payload = status || {};
  state.updateStatus = payload;
  const panel = $("updateProgressPanel");
  const label = $("updateProgressLabel");
  const percent = $("updateProgressPercent");
  const bar = $("updateProgressBar");
  const message = $("updateProgressMessage");
  const pill = $("updateStatusPill");
  const notes = $("updateReleaseNotes");
  const installButton = $("installUpdateButton");
  const checkButton = $("checkUpdateButton");
  const switchButton = $("switchUpdateVersionButton");
  const statusValue = String(payload.status || "idle");
  const progress = Math.max(0, Math.min(100, Number(payload.progress) || 0));
  const running = statusValue === "running";
  const visible = running || statusValue === "success" || statusValue === "failed";
  if (statusValue === "success") {
    state.updatePlan = null;
  }

  if (panel) {
    panel.hidden = !visible;
    panel.classList.toggle("is-success", statusValue === "success");
    panel.classList.toggle("is-failed", statusValue === "failed");
  }
  if (label) {
    label.textContent = updateStatusLabel(payload);
  }
  if (percent) {
    percent.textContent = `${Math.round(progress)}%`;
  }
  if (bar) {
    bar.style.width = `${progress}%`;
  }
  if (message) {
    const details = payload.error ? `原因：${payload.error}` : (payload.message || "暂无更新任务。");
    message.textContent = details;
  }
  if (pill && visible) {
    pill.textContent = updateStatusLabel(payload);
    pill.className = `pill${statusValue === "failed" ? " danger-pill" : ""}`;
  }
  if (notes && payload.error) {
    notes.textContent = `更新失败\n${payload.error}`;
  } else if (notes && statusValue === "success") {
    const version = payload.version ? `版本：${payload.version}` : "";
    notes.textContent = ["更新安装成功", version, "页面即将刷新。"].filter(Boolean).join("\n");
  }
  if (installButton) {
    installButton.disabled = running || !state.updatePlan;
  }
  if (checkButton) {
    checkButton.disabled = running;
  }
  if (switchButton) {
    switchButton.disabled = running;
  }
}

async function fetchUpdateStatus({ silent = false, render = true } = {}) {
  try {
    const status = await api("/api/update/status");
    if (render) {
      renderUpdateStatus(status);
    }
    return status;
  } catch (error) {
    if (!silent) {
      throw error;
    }
    return null;
  }
}

function stopUpdateStatusPolling() {
  if (state.updateStatusTimer) {
    clearInterval(state.updateStatusTimer);
    state.updateStatusTimer = null;
  }
}

function scheduleUpdatePageRefresh(delayMs = 2600) {
  if (state.updateRefreshScheduled) {
    return;
  }
  state.updateRefreshScheduled = true;
  window.setTimeout(() => {
    window.location.reload();
  }, delayMs);
}

function startUpdateStatusPolling() {
  if (state.updateStatusTimer) {
    return;
  }
  state.updateStatusTimer = window.setInterval(async () => {
    if (state.updateStatusInFlight) {
      return;
    }
    state.updateStatusInFlight = true;
    try {
      const status = await fetchUpdateStatus({ silent: true });
      if (status && status.status === "success") {
        stopUpdateStatusPolling();
        showToast("更新成功，页面即将刷新");
        scheduleUpdatePageRefresh();
      } else if (status && status.status === "failed") {
        stopUpdateStatusPolling();
        showToast(status.error || "更新失败", true);
      }
    } finally {
      state.updateStatusInFlight = false;
    }
  }, 1000);
}

async function refreshInitialUpdateStatus() {
  const status = await fetchUpdateStatus({ silent: true, render: false });
  if (status && status.status === "running") {
    renderUpdateStatus(status);
    startUpdateStatusPolling();
  } else if (status && status.status === "failed") {
    renderUpdateStatus(status);
  } else {
    renderUpdateStatus({
      status: "idle",
      progress: 0,
      message: "暂无更新任务。",
    });
  }
}

function updatePayloadHasInstallableUpdate(payload) {
  const components = (payload && payload.components) || {};
  const core = components.core || {};
  const usbProxy = components.usb_proxy || {};
  const themeUpdates = Array.isArray(payload && payload.theme_updates) ? payload.theme_updates : [];
  return !!(
    payload &&
    payload.update_available &&
    (payload.package || core.update_available || usbProxy.update_available || themeUpdates.length)
  );
}

async function checkUpdate({ preferFull = false, targetVersion = "" } = {}) {
  const body = {};
  if (preferFull) {
    body.prefer_full = true;
  }
  if (targetVersion) {
    body.target_version = targetVersion;
  }
  return api("/api/update/check", {
    method: "POST",
    headers: { "Content-Type": "application/json" },
    body: JSON.stringify(body),
  });
}

async function installUpdatePlan(plan) {
  try {
    renderUpdateStatus({
      status: "running",
      stage: "submit",
      message: "正在提交更新任务",
      progress: 1,
      version: (plan && plan.latest_version) || "",
    });
    startUpdateStatusPolling();
    return await api("/api/update/install", {
      method: "POST",
      headers: { "Content-Type": "application/json" },
      body: JSON.stringify({ plan }),
    });
  } catch (error) {
    stopUpdateStatusPolling();
    renderUpdateStatus({
      status: "failed",
      stage: "failed",
      message: "更新失败",
      progress: 100,
      version: (plan && plan.latest_version) || "",
      error: error.message || String(error),
    });
    throw error;
  }
}

async function cleanupStuckUpdateStatus() {
  const confirmed = window.confirm("仅在更新进度卡住且长时间没有恢复时使用。若后台仍有真实安装任务，系统会拒绝清理。确认现在清理更新状态？");
  if (!confirmed) {
    return;
  }
  const button = $("cleanupUpdateStatusButton");
  if (button) {
    button.disabled = true;
  }
  try {
    const result = await api("/api/update/cleanup-stuck", { method: "POST" });
    stopUpdateStatusPolling();
    state.updatePlan = null;
    renderUpdateStatus(result.status || {
      status: "idle",
      progress: 0,
      message: "暂无更新任务。",
    });
    const deletedCount = Array.isArray(result.deleted) ? result.deleted.length : 0;
    showToast(deletedCount ? `已清理更新状态和 ${deletedCount} 个临时文件` : "已清理更新状态");
  } finally {
    if (button) {
      button.disabled = false;
    }
  }
}

async function installFullUpdateAfterActivation() {
  setActivationSetupProgress(72, 0, "正在检查完整更新包。");
  const update = await checkUpdate({ preferFull: true });
  renderUpdateResult(update);
  if (!updatePayloadHasInstallableUpdate(update)) {
    return false;
  }
  if (!update.package) {
    throw new Error("检测到组件更新，但服务器未提供全量更新包");
  }
  const plan = {
    latest_version: update.latest_version || "",
    package: update.package,
    components: update.components || {},
    theme_updates: update.theme_updates || [],
    theme_compatibility: update.theme_compatibility || [],
    theme_fallback: update.theme_fallback || null,
    prefer_full: true,
  };
  state.updatePlan = plan;
  setActivationSetupProgress(84, 0, "发现组件更新，正在安装完整更新包。");
  await installUpdatePlan(plan);
  return true;
}

function renderModelPanel(models, selectedModelId) {
  const availableModels = visibleModelLibraryModels(models);
  const currentModel = availableModels.find((model) => model.id === selectedModelId);
  const name = $("modelCurrentName");
  const meta = $("modelCurrentMeta");
  const summary = $("modelLibrarySummary");

  if (name) {
    name.textContent = currentModel ? modelFileName(currentModel) : "尚未选择模型";
  }
  if (meta) {
    meta.textContent = currentModel
      ? [
        modelDimension(currentModel),
        modelBackendLabel(currentModel),
        modelHailoPipelineDepthLabel(currentModel),
        modelOutputLabel(currentModel),
        modelProfileLabel(currentModel.game_profile),
        currentModel.preset_name ? `绑定预设：${currentModel.preset_name}` : "",
      ].filter(Boolean).join(" / ")
      : "等待模型信息";
  }
  if (summary) {
    summary.textContent = `${availableModels.length} 个可用`;
  }
}

function uniqueModelGames(models) {
  return Array.from(new Set(visibleModelLibraryModels(models).map((model) => model.game_profile || "generic")))
    .sort((lhs, rhs) => modelProfileLabel(lhs).localeCompare(modelProfileLabel(rhs), "zh-Hans-CN"));
}

function modelGamePickerOptions(models, currentGame) {
  const options = new Set(["generic"]);
  uniqueModelGames(models).forEach((game) => options.add(game || "generic"));
  options.add(currentGame || "generic");
  return Array.from(options)
    .sort((lhs, rhs) => modelProfileLabel(lhs).localeCompare(modelProfileLabel(rhs), "zh-Hans-CN"));
}

function filteredModelImportGames(games = state.modelGameOptions) {
  const input = $("modelImportGameProfile");
  const query = input ? input.value.trim().toLowerCase() : "";
  if (!query) {
    return games;
  }
  return games.filter((game) => {
    const value = String(game || "").toLowerCase();
    const label = modelProfileLabel(game).toLowerCase();
    return value.includes(query) || label.includes(query);
  });
}

function setModelGameSuggestionOpen(open) {
  const input = $("modelImportGameProfile");
  const list = $("modelGameSuggestionList");
  const combo = $("modelGameCombobox");
  const toggle = $("modelGameSuggestionToggle");
  const hasVisibleOptions = filteredModelImportGames().length > 0;
  const nextOpen = !!open && hasVisibleOptions;

  state.modelGameSuggestionOpen = nextOpen;
  if (list) {
    list.hidden = !nextOpen;
  }
  if (input) {
    input.setAttribute("aria-expanded", nextOpen ? "true" : "false");
  }
  if (combo) {
    combo.classList.toggle("is-open", nextOpen);
  }
  if (toggle) {
    toggle.disabled = state.modelGameOptions.length === 0;
    toggle.setAttribute("aria-expanded", nextOpen ? "true" : "false");
  }
}

function renderModelImportGameSuggestions(games = state.modelGameOptions) {
  state.modelGameOptions = Array.isArray(games) ? games : [];
  const list = $("modelGameSuggestionList");
  const toggle = $("modelGameSuggestionToggle");
  if (toggle) {
    toggle.disabled = state.modelGameOptions.length === 0;
  }
  if (!list) {
    return;
  }

  const visibleGames = filteredModelImportGames(state.modelGameOptions);
  list.innerHTML = "";
  visibleGames.forEach((game) => {
    const label = modelProfileLabel(game);
    const button = document.createElement("button");
    button.type = "button";
    button.className = "model-game-suggestion-option";
    button.dataset.modelGame = game;
    button.setAttribute("role", "option");
    button.innerHTML = `
      <span>${escapeHtml(label)}</span>
      ${label !== game ? `<small>${escapeHtml(game)}</small>` : ""}
    `;
    list.appendChild(button);
  });
  setModelGameSuggestionOpen(state.modelGameSuggestionOpen);
}

function renderModelGameFilters(models) {
  const filters = $("modelGameFilters");
  const games = uniqueModelGames(models);
  if (!games.includes(state.modelGameFilter) && state.modelGameFilter !== "all") {
    state.modelGameFilter = "all";
  }
  renderModelImportGameSuggestions(games);
  if (!filters) {
    return;
  }
  filters.innerHTML = "";
  [{ value: "all", label: "全部" }, ...games.map((game) => ({ value: game, label: modelProfileLabel(game) }))]
    .forEach((filter) => {
      const button = document.createElement("button");
      button.type = "button";
      button.className = `model-filter-chip${state.modelGameFilter === filter.value ? " is-active" : ""}`;
      button.textContent = filter.label;
      button.addEventListener("click", () => {
        state.modelGameFilter = filter.value;
        renderModels({ models, selected_model_id: state.config && state.config.model_id }, state.config && state.config.model_id);
      });
      filters.appendChild(button);
    });
}

function mergeRemoteModelPayload(payload) {
  if (!payload) {
    return;
  }
  if (payload.config && payload.config.host) {
    state.remoteHost = String(payload.config.host || "");
  }
  if (state.data && Array.isArray(payload.models)) {
    state.data.models = payload.models || [];
  }
  renderModels({
    models: (payload && payload.models) || (state.data && state.data.models) || [],
    selected_model_id: (payload && payload.selected_model_id) || (state.config && state.config.model_id) || "",
  }, (payload && payload.selected_model_id) || (state.config && state.config.model_id) || "");
}

function setRemoteConnectBusy(busy) {
  state.remoteConnecting = busy;
  const submit = $("submitRemoteConnectButton");
  const cancel = $("cancelRemoteConnectButton");
  const close = $("closeRemoteConnectButton");
  const input = $("remoteHostInput");
  if (submit) {
    submit.disabled = busy;
    submit.textContent = busy ? "连接中..." : "连接";
  }
  if (cancel) {
    cancel.disabled = busy;
  }
  if (close) {
    close.disabled = busy;
  }
  if (input) {
    input.disabled = busy;
  }
}

function setRemoteConnectError(message = "") {
  state.remoteError = message || "";
  const error = $("remoteConnectError");
  if (error) {
    error.hidden = !message;
    error.textContent = message || "";
  }
}

function setRemoteConnectDialogOpen(open, message = "") {
  const dialog = $("remoteConnectDialog");
  if (!dialog) {
    return;
  }
  dialog.hidden = !open;
  setRemoteConnectBusy(false);
  setRemoteConnectError(open ? message : "");
  setAnyModalOpen();
  if (open) {
    const input = $("remoteHostInput");
    if (input) {
      input.value = state.remoteHost || input.value || "";
      input.focus();
      input.select();
    }
  }
}

function remoteHostHint() {
  if (state.remoteHost) {
    return state.remoteHost;
  }
  const models = (state.data && Array.isArray(state.data.models)) ? state.data.models : [];
  const remoteModel = models.find((model) => modelBackend(model) === "remote" && model.remote_host);
  return remoteModel ? String(remoteModel.remote_host || "") : "";
}

async function connectRemoteHost(host) {
  const payload = await api("/api/remote/connect", {
    method: "POST",
    headers: { "Content-Type": "application/json" },
    body: JSON.stringify({ host }),
  });
  mergeRemoteModelPayload(payload);
  return payload;
}

async function refreshRemoteModels({ promptOnFailure = true, toastOnSuccess = false } = {}) {
  if (state.remoteRefreshing) {
    return null;
  }
  const hostHint = remoteHostHint();
  if (!hostHint) {
    if (promptOnFailure) {
      setRemoteConnectDialogOpen(true, "请输入 Windows 电脑局域网 IP");
    }
    return null;
  }
  state.remoteHost = hostHint;
  state.remoteRefreshing = true;
  try {
    const payload = await api("/api/remote/models");
    mergeRemoteModelPayload(payload);
    if (toastOnSuccess) {
      showToast("远端模型列表已刷新");
    }
    return payload;
  } catch (error) {
    const message = error.message || "连接失败，请重新输入局域网IP或检查Windows端程序是否启动";
    if (promptOnFailure) {
      setRemoteConnectDialogOpen(true, message);
    }
    throw error;
  } finally {
    state.remoteRefreshing = false;
  }
}

function renderModelBackendFilters(models) {
  const filters = $("modelBackendFilters");
  const allowedFilters = shouldShowCloudEncryptedModelsInLibrary()
    ? ["all", "rknn", "hef", "remote", "cloud_encrypted"]
    : ["all", "rknn", "hef", "remote"];
  if (!allowedFilters.includes(state.modelBackendFilter)) {
    state.modelBackendFilter = "all";
  }
  if (!filters) {
    return;
  }
  const options = [
    { value: "all", label: "全部" },
    { value: "rknn", label: "RKNN" },
    { value: "hef", label: "HEF" },
    { value: "remote", label: "远端" },
  ];
  if (shouldShowCloudEncryptedModelsInLibrary()) {
    options.push({ value: "cloud_encrypted", label: "云加密" });
  }
  filters.innerHTML = "";
  options.forEach((filter) => {
    const button = document.createElement("button");
    button.type = "button";
    button.className = `model-filter-chip model-format-chip${state.modelBackendFilter === filter.value ? " is-active" : ""}`;
    button.textContent = filter.label;
    button.addEventListener("click", () => {
      state.modelBackendFilter = filter.value;
      renderModels({ models, selected_model_id: state.config && state.config.model_id }, state.config && state.config.model_id);
      if (filter.value === "remote") {
        runUiAction(() => refreshRemoteModels({ promptOnFailure: true }));
      }
    });
    filters.appendChild(button);
  });
}

function modelMatchesActiveFilters(model) {
  if (!modelVisibleInLibrary(model)) {
    return false;
  }
  const gameMatches = state.modelGameFilter === "all" ||
    (model.game_profile || "generic") === state.modelGameFilter;
  const backendMatches = state.modelBackendFilter === "all" ||
    modelBackendFilterValue(model) === state.modelBackendFilter;
  return gameMatches && backendMatches;
}

function trashIconSvg() {
  return `
    <svg viewBox="0 0 24 24" aria-hidden="true">
      <path d="M3 6h18"/>
      <path d="M8 6V4h8v2"/>
      <path d="m6 6 1 15h10l1-15"/>
      <path d="M10 11v6M14 11v6"/>
    </svg>
  `;
}

function renderModelCards(models, selectedModelId) {
  const list = $("modelCardList");
  const empty = $("modelEmptyState");
  if (!list) {
    return;
  }
  const visibleModels = models.filter(modelMatchesActiveFilters);
  list.innerHTML = "";
  if (empty) {
    empty.hidden = visibleModels.length > 0;
  }
  visibleModels.forEach((model) => {
    const isSelected = model.id === selectedModelId;
    const modelBackendValue = modelBackend(model);
    const isRemote = modelBackendValue === "remote";
    const isCloudEncrypted = modelBackendValue === "cloud_encrypted";
    const usesRknnConcurrency = modelUsesRknnConcurrency(model);
    const usesHailoPipelineDepth = modelUsesHailoPipelineDepth(model);
    const remoteMissing = isRemote && model.remote_available === false;
    const card = document.createElement("article");
    card.className = `model-card${isSelected ? " is-active" : ""}${remoteMissing ? " is-missing" : ""}`;
    card.dataset.modelId = model.id;
    card.tabIndex = remoteMissing ? -1 : 0;
    card.setAttribute("role", "button");
    if (remoteMissing) {
      card.setAttribute("aria-disabled", "true");
    }
    const switchModel = () => runUiAction(async () => {
      if (remoteMissing) {
        showToast("远端模型丢失，请刷新远端模型列表或重新导入", true);
        return;
      }
      if (model.id === ((state.config && state.config.model_id) || "")) {
        return;
      }
      const result = await applySelectedModel(model.id, model);
      showToast(result && result.created_default_preset && result.created_default_preset_name
        ? `模型已应用，并创建默认预设 ${result.created_default_preset_name}`
        : result && result.applied_preset && result.applied_preset_name
        ? `模型已应用，并加载预设 ${result.applied_preset_name}`
        : result && result.cleared_missing_preset && result.cleared_missing_preset_name
          ? `模型已应用，已清除失效预设 ${result.cleared_missing_preset_name}`
        : "模型已应用");
    });
    card.addEventListener("click", switchModel);
    card.addEventListener("keydown", (event) => {
      if (event.key === "Enter" || event.key === " ") {
        event.preventDefault();
        switchModel();
      }
    });

    const description = (model.description || "").trim() || "暂无描述";
    const presetName = String(model.preset_name || "");
    const presetLabel = presetName || "未绑定（自动创建）";
    const currentGameProfile = model.game_profile || "generic";
    const gameOptions = modelGamePickerOptions(models, currentGameProfile);
    const hailoPipelineDepth = modelHailoPipelineDepth(model);
    const hailoPipelineOptions = [1, 2, 3, 4];
    const rknnConcurrency = modelRknnConcurrency(model);
    const rknnConcurrencyOptions = [1, 2, 3];
    const statusLabel = remoteMissing ? "远端模型丢失无法使用" : isSelected ? "当前使用" : "可切换";
    const remoteFrameFormat = normalizeRemoteFrameFormat(model.remote_frame_format);
    const remoteFrameOptions = [
      { value: "jpeg", label: "JPEG 编码" },
      { value: "nv12", label: "NV12 直发" },
      { value: "h264", label: "H.264 低延迟" },
    ];
    const presetOptions = [
      { value: "", label: presetName ? "清空绑定" : "未绑定（自动创建）" },
      ...state.presetNames.map((name) => ({ value: name, label: name })),
    ];
    const isGameOpen = state.modelGameBindingOpenId === model.id;
    const isPresetOpen = state.modelPresetBindingOpenId === model.id;
    const isRemoteFrameOpen = state.modelRemoteFrameBindingOpenId === model.id;
    const isRknnConcurrencyOpen = state.modelRknnConcurrencyBindingOpenId === model.id;
    const isHailoPipelineOpen = state.modelHailoPipelineBindingOpenId === model.id;
    card.innerHTML = `
      <span class="model-card-top">
        <span class="model-card-title">${escapeHtml(modelFileName(model))}</span>
        <span class="model-card-status">${escapeHtml(statusLabel)}</span>
      </span>
	      <span class="model-card-meta">
	        <span>${escapeHtml(modelDimension(model))}</span>
	        <span>${escapeHtml(modelBackendLabel(model))}</span>
	        ${isRemote && model.remote_host ? `<span>${escapeHtml(model.remote_host)}</span>` : ""}
	        ${isCloudEncrypted && model.file_name ? `<span>${escapeHtml(model.file_name)}</span>` : ""}
	        ${usesHailoPipelineDepth && hailoPipelineDepth ? `
        <span class="model-hailo-pipeline-binding${isHailoPipelineOpen ? " is-open" : ""}">
          <button class="model-game-tag-button model-hailo-pipeline-button" type="button"
            data-model-hailo-pipeline-toggle="${escapeAttr(model.id)}"
            aria-controls="modelHailoPipelineList-${escapeAttr(model.id)}"
            aria-expanded="${isHailoPipelineOpen ? "true" : "false"}"
            aria-haspopup="listbox">
            ${escapeHtml(modelHailoPipelineDepthLabel(hailoPipelineDepth))}
            <svg viewBox="0 0 24 24" aria-hidden="true">
              <path d="m6 9 6 6 6-6"/>
            </svg>
          </button>
          <div id="modelHailoPipelineList-${escapeAttr(model.id)}" class="model-game-suggestion-list model-card-game-option-list model-hailo-pipeline-option-list" role="listbox" ${isHailoPipelineOpen ? "" : "hidden"}>
            ${hailoPipelineOptions.map((option) => `
              <button class="model-game-suggestion-option${option === hailoPipelineDepth ? " is-active" : ""}" type="button"
                data-model-hailo-pipeline-option="${escapeAttr(model.id)}"
                data-hailo-pipeline-depth="${escapeAttr(option)}"
                role="option"
                aria-selected="${option === hailoPipelineDepth ? "true" : "false"}">
                <span>${escapeHtml(modelHailoPipelineDepthLabel(option))}</span>
              </button>
            `).join("")}
          </div>
        </span>
        ` : ""}
	        ${usesRknnConcurrency ? `
        <span class="model-rknn-concurrency-binding${isRknnConcurrencyOpen ? " is-open" : ""}">
          <button class="model-game-tag-button model-rknn-concurrency-button" type="button"
            data-model-rknn-concurrency-toggle="${escapeAttr(model.id)}"
            aria-controls="modelRknnConcurrencyList-${escapeAttr(model.id)}"
            aria-expanded="${isRknnConcurrencyOpen ? "true" : "false"}"
            aria-haspopup="listbox">
            ${escapeHtml(modelRknnConcurrencyLabel(rknnConcurrency))}
            <svg viewBox="0 0 24 24" aria-hidden="true">
              <path d="m6 9 6 6 6-6"/>
            </svg>
          </button>
          <div id="modelRknnConcurrencyList-${escapeAttr(model.id)}" class="model-game-suggestion-list model-card-game-option-list model-rknn-concurrency-option-list" role="listbox" ${isRknnConcurrencyOpen ? "" : "hidden"}>
            ${rknnConcurrencyOptions.map((option) => `
              <button class="model-game-suggestion-option${option === rknnConcurrency ? " is-active" : ""}" type="button"
                data-model-rknn-concurrency-option="${escapeAttr(model.id)}"
                data-rknn-concurrency="${escapeAttr(option)}"
                role="option"
                aria-selected="${option === rknnConcurrency ? "true" : "false"}">
                <span>${escapeHtml(modelRknnConcurrencyLabel(option))}</span>
              </button>
            `).join("")}
          </div>
        </span>
        ` : ""}
	        <span>${escapeHtml(modelClassLabel(model))}</span>
        <span class="model-game-binding${isGameOpen ? " is-open" : ""}">
          <button class="model-game-tag-button" type="button"
            data-model-game-toggle="${escapeAttr(model.id)}"
            aria-controls="modelGameList-${escapeAttr(model.id)}"
            aria-expanded="${isGameOpen ? "true" : "false"}"
            aria-haspopup="listbox">
            ${escapeHtml(modelProfileLabel(currentGameProfile))}
            <svg viewBox="0 0 24 24" aria-hidden="true">
              <path d="m6 9 6 6 6-6"/>
            </svg>
          </button>
          <div id="modelGameList-${escapeAttr(model.id)}" class="model-game-suggestion-list model-card-game-option-list" role="listbox" ${isGameOpen ? "" : "hidden"}>
            ${gameOptions.map((option) => {
              const optionLabel = modelProfileLabel(option);
              return `
                <button class="model-game-suggestion-option${option === currentGameProfile ? " is-active" : ""}" type="button"
                  data-model-game-option="${escapeAttr(model.id)}"
                  data-game-profile="${escapeAttr(option)}"
                  role="option"
                  aria-selected="${option === currentGameProfile ? "true" : "false"}">
                  <span>${escapeHtml(optionLabel)}</span>
                  ${optionLabel !== option ? `<small>${escapeHtml(option)}</small>` : ""}
                </button>
              `;
            }).join("")}
          </div>
        </span>
        ${isRemote ? `
        <span class="model-remote-frame-binding${isRemoteFrameOpen ? " is-open" : ""}">
          <button class="model-game-tag-button model-remote-frame-button" type="button"
            data-model-remote-frame-toggle="${escapeAttr(model.id)}"
            aria-controls="modelRemoteFrameList-${escapeAttr(model.id)}"
            aria-expanded="${isRemoteFrameOpen ? "true" : "false"}"
            aria-haspopup="listbox">
            ${escapeHtml(remoteFrameFormatLabel(remoteFrameFormat))}
            <svg viewBox="0 0 24 24" aria-hidden="true">
              <path d="m6 9 6 6 6-6"/>
            </svg>
          </button>
          <div id="modelRemoteFrameList-${escapeAttr(model.id)}" class="model-game-suggestion-list model-card-game-option-list model-remote-frame-option-list" role="listbox" ${isRemoteFrameOpen ? "" : "hidden"}>
            ${remoteFrameOptions.map((option) => `
              <button class="model-game-suggestion-option${option.value === remoteFrameFormat ? " is-active" : ""}" type="button"
                data-model-remote-frame-option="${escapeAttr(model.id)}"
                data-remote-frame-format="${escapeAttr(option.value)}"
                role="option"
                aria-selected="${option.value === remoteFrameFormat ? "true" : "false"}">
                <span>${escapeHtml(option.label)}</span>
              </button>
            `).join("")}
          </div>
        </span>
        ` : ""}
      </span>
      <span class="model-card-description">${escapeHtml(description)}</span>
      <div class="model-preset-binding">
        <span>绑定预设</span>
        <div class="model-preset-combobox${isPresetOpen ? " is-open" : ""}">
          <button class="preset-select-button model-preset-select-button" type="button"
            data-model-preset-toggle="${escapeAttr(model.id)}"
            aria-controls="modelPresetList-${escapeAttr(model.id)}"
            aria-expanded="${isPresetOpen ? "true" : "false"}"
            aria-haspopup="listbox">
            <span>${escapeHtml(presetLabel)}</span>
            <svg viewBox="0 0 24 24" aria-hidden="true">
              <path d="m6 9 6 6 6-6"/>
            </svg>
          </button>
          <div id="modelPresetList-${escapeAttr(model.id)}" class="model-game-suggestion-list model-preset-option-list" role="listbox" ${isPresetOpen ? "" : "hidden"}>
            ${presetOptions.map((option) => `
              <button class="model-game-suggestion-option${option.value === presetName ? " is-active" : ""}" type="button"
                data-model-preset-option="${escapeAttr(model.id)}"
                data-preset-name="${escapeAttr(option.value)}"
                role="option"
                aria-selected="${option.value === presetName ? "true" : "false"}">
                <span>${escapeHtml(option.label)}</span>
              </button>
            `).join("")}
          </div>
        </div>
        <small>${escapeHtml(presetName ? `切换到此模型时自动加载：${presetLabel}` : "首次切换到此模型时自动创建默认预设")}</small>
      </div>
      <div class="model-card-actions">
        <button class="mini-button model-class-edit-button" type="button" data-model-class-edit="${escapeAttr(model.id)}">编辑类别</button>
      </div>
    `;

    const presetBinding = card.querySelector(".model-preset-binding");
    if (presetBinding) {
      presetBinding.addEventListener("click", (event) => event.stopPropagation());
      presetBinding.addEventListener("keydown", (event) => event.stopPropagation());
    }
    const gameBinding = card.querySelector(".model-game-binding");
    if (gameBinding) {
      gameBinding.addEventListener("click", (event) => event.stopPropagation());
      gameBinding.addEventListener("keydown", (event) => event.stopPropagation());
    }
    const remoteFrameBinding = card.querySelector(".model-remote-frame-binding");
    if (remoteFrameBinding) {
      remoteFrameBinding.addEventListener("click", (event) => event.stopPropagation());
      remoteFrameBinding.addEventListener("keydown", (event) => event.stopPropagation());
    }
    const rknnConcurrencyBinding = card.querySelector(".model-rknn-concurrency-binding");
    if (rknnConcurrencyBinding) {
      rknnConcurrencyBinding.addEventListener("click", (event) => event.stopPropagation());
      rknnConcurrencyBinding.addEventListener("keydown", (event) => event.stopPropagation());
    }
    const hailoPipelineBinding = card.querySelector(".model-hailo-pipeline-binding");
    if (hailoPipelineBinding) {
      hailoPipelineBinding.addEventListener("click", (event) => event.stopPropagation());
      hailoPipelineBinding.addEventListener("keydown", (event) => event.stopPropagation());
    }
    const cardActions = card.querySelector(".model-card-actions");
    if (cardActions) {
      cardActions.addEventListener("click", (event) => event.stopPropagation());
      cardActions.addEventListener("keydown", (event) => event.stopPropagation());
    }
    const classEditButton = card.querySelector("[data-model-class-edit]");
    if (classEditButton) {
      classEditButton.addEventListener("click", (event) => {
        event.preventDefault();
        event.stopPropagation();
        setModelClassNamesDialogOpen(true, model.id);
      });
    }
    const gameToggle = card.querySelector("[data-model-game-toggle]");
    if (gameToggle) {
      gameToggle.addEventListener("click", (event) => {
        event.preventDefault();
        event.stopPropagation();
        setModelGameBindingOpen(state.modelGameBindingOpenId !== model.id ? model.id : "");
      });
      gameToggle.addEventListener("keydown", (event) => {
        event.stopPropagation();
        if (event.key === "ArrowDown") {
          event.preventDefault();
          setModelGameBindingOpen(model.id);
        } else if (event.key === "Escape") {
          event.preventDefault();
          setModelGameBindingOpen("");
        }
      });
    }
    card.querySelectorAll("[data-model-game-option]").forEach((option) => {
      option.addEventListener("click", (event) => {
        event.preventDefault();
        event.stopPropagation();
        const nextGameProfile = option.dataset.gameProfile || "generic";
        if (nextGameProfile === currentGameProfile) {
          setModelGameBindingOpen("");
          return;
        }
        runUiAction(async () => {
          state.modelGameBindingOpenId = "";
          await updateModelGameProfile(model.id, nextGameProfile);
          showToast(`模型游戏标签已切换为 ${modelProfileLabel(nextGameProfile)}`);
        });
      });
    });
    const remoteFrameToggle = card.querySelector("[data-model-remote-frame-toggle]");
    if (remoteFrameToggle) {
      remoteFrameToggle.addEventListener("click", (event) => {
        event.preventDefault();
        event.stopPropagation();
        setModelRemoteFrameBindingOpen(state.modelRemoteFrameBindingOpenId !== model.id ? model.id : "");
      });
      remoteFrameToggle.addEventListener("keydown", (event) => {
        event.stopPropagation();
        if (event.key === "ArrowDown") {
          event.preventDefault();
          setModelRemoteFrameBindingOpen(model.id);
        } else if (event.key === "Escape") {
          event.preventDefault();
          setModelRemoteFrameBindingOpen("");
        }
      });
    }
    card.querySelectorAll("[data-model-remote-frame-option]").forEach((option) => {
      option.addEventListener("click", (event) => {
        event.preventDefault();
        event.stopPropagation();
        const nextFormat = normalizeRemoteFrameFormat(option.dataset.remoteFrameFormat || "jpeg");
        if (nextFormat === remoteFrameFormat) {
          setModelRemoteFrameBindingOpen("");
          return;
        }
        runUiAction(async () => {
          state.modelRemoteFrameBindingOpenId = "";
          await updateModelRemoteFrameFormat(model.id, nextFormat);
          showToast(`远端发送方式已切换为 ${remoteFrameFormatLabel(nextFormat)}`);
        });
      });
    });
    const rknnConcurrencyToggle = card.querySelector("[data-model-rknn-concurrency-toggle]");
    if (rknnConcurrencyToggle) {
      rknnConcurrencyToggle.addEventListener("click", (event) => {
        event.preventDefault();
        event.stopPropagation();
        setModelRknnConcurrencyBindingOpen(state.modelRknnConcurrencyBindingOpenId !== model.id ? model.id : "");
      });
      rknnConcurrencyToggle.addEventListener("keydown", (event) => {
        event.stopPropagation();
        if (event.key === "ArrowDown") {
          event.preventDefault();
          setModelRknnConcurrencyBindingOpen(model.id);
        } else if (event.key === "Escape") {
          event.preventDefault();
          setModelRknnConcurrencyBindingOpen("");
        }
      });
    }
    card.querySelectorAll("[data-model-rknn-concurrency-option]").forEach((option) => {
      option.addEventListener("click", (event) => {
        event.preventDefault();
        event.stopPropagation();
        const nextConcurrency = Math.round(clamp(Number(option.dataset.rknnConcurrency) || 1, 1, 3));
        if (nextConcurrency === rknnConcurrency) {
          setModelRknnConcurrencyBindingOpen("");
          return;
        }
        runUiAction(async () => {
          state.modelRknnConcurrencyBindingOpenId = "";
          await updateModelRknnConcurrency(model.id, nextConcurrency);
          showToast(`RKNN 并发数已切换为 ${nextConcurrency}`);
        });
      });
    });
    const hailoPipelineToggle = card.querySelector("[data-model-hailo-pipeline-toggle]");
    if (hailoPipelineToggle) {
      hailoPipelineToggle.addEventListener("click", (event) => {
        event.preventDefault();
        event.stopPropagation();
        setModelHailoPipelineBindingOpen(state.modelHailoPipelineBindingOpenId !== model.id ? model.id : "");
      });
      hailoPipelineToggle.addEventListener("keydown", (event) => {
        event.stopPropagation();
        if (event.key === "ArrowDown") {
          event.preventDefault();
          setModelHailoPipelineBindingOpen(model.id);
        } else if (event.key === "Escape") {
          event.preventDefault();
          setModelHailoPipelineBindingOpen("");
        }
      });
    }
    card.querySelectorAll("[data-model-hailo-pipeline-option]").forEach((option) => {
      option.addEventListener("click", (event) => {
        event.preventDefault();
        event.stopPropagation();
        const nextDepth = Math.round(clamp(Number(option.dataset.hailoPipelineDepth) || 3, 1, 4));
        if (nextDepth === hailoPipelineDepth) {
          setModelHailoPipelineBindingOpen("");
          return;
        }
        runUiAction(async () => {
          state.modelHailoPipelineBindingOpenId = "";
          await updateModelHailoPipelineDepth(model.id, nextDepth);
          showToast(`Hailo 并发深度已切换为 ${nextDepth}`);
        });
      });
    });
    const presetToggle = card.querySelector("[data-model-preset-toggle]");
    if (presetToggle) {
      presetToggle.addEventListener("click", (event) => {
        event.preventDefault();
        event.stopPropagation();
        setModelPresetBindingOpen(state.modelPresetBindingOpenId !== model.id ? model.id : "");
      });
      presetToggle.addEventListener("keydown", (event) => {
        event.stopPropagation();
        if (event.key === "ArrowDown") {
          event.preventDefault();
          setModelPresetBindingOpen(model.id);
        } else if (event.key === "Escape") {
          event.preventDefault();
          setModelPresetBindingOpen("");
        }
      });
    }
    card.querySelectorAll("[data-model-preset-option]").forEach((option) => {
      option.addEventListener("click", (event) => {
        event.preventDefault();
        event.stopPropagation();
        runUiAction(async () => {
          const result = await bindModelPreset(model.id, option.dataset.presetName || "");
          const nextName = String(result && result.model && result.model.preset_name || "");
          state.modelPresetBindingOpenId = "";
          showToast(nextName ? `已绑定预设 ${nextName}` : "已取消模型预设绑定");
        });
      });
    });

    const deleteButton = document.createElement("button");
    deleteButton.type = "button";
    deleteButton.className = "icon-button delete-model-button";
    const canDelete = !isSelected || remoteMissing;
    deleteButton.title = canDelete ? "删除模型" : "当前模型不能删除";
    deleteButton.setAttribute("aria-label", canDelete ? `删除 ${modelFileName(model)}` : "当前模型不能删除");
    deleteButton.disabled = !canDelete;
    deleteButton.innerHTML = trashIconSvg();
    deleteButton.addEventListener("click", (event) => {
      event.stopPropagation();
      if (!canDelete) {
        return;
      }
      runUiAction(async () => {
        const confirmed = window.confirm(`确认完全删除模型 ${modelFileName(model)}？`);
        if (!confirmed) {
          return;
        }
        await deleteModel(model.id);
        showToast("模型已删除");
      });
    });
    card.appendChild(deleteButton);
    list.appendChild(card);
  });
}

function setModelGameBindingOpen(modelId) {
  state.modelGameBindingOpenId = modelId || "";
  if (state.modelGameBindingOpenId) {
    state.modelPresetBindingOpenId = "";
    state.modelRemoteFrameBindingOpenId = "";
    state.modelRknnConcurrencyBindingOpenId = "";
    state.modelHailoPipelineBindingOpenId = "";
  }
  rerenderCurrentModels();
}

function setModelPresetBindingOpen(modelId) {
  state.modelPresetBindingOpenId = modelId || "";
  if (state.modelPresetBindingOpenId) {
    state.modelGameBindingOpenId = "";
    state.modelRemoteFrameBindingOpenId = "";
    state.modelRknnConcurrencyBindingOpenId = "";
    state.modelHailoPipelineBindingOpenId = "";
  }
  rerenderCurrentModels();
}

function setModelRemoteFrameBindingOpen(modelId) {
  state.modelRemoteFrameBindingOpenId = modelId || "";
  if (state.modelRemoteFrameBindingOpenId) {
    state.modelGameBindingOpenId = "";
    state.modelPresetBindingOpenId = "";
    state.modelRknnConcurrencyBindingOpenId = "";
    state.modelHailoPipelineBindingOpenId = "";
  }
  rerenderCurrentModels();
}

function setModelRknnConcurrencyBindingOpen(modelId) {
  state.modelRknnConcurrencyBindingOpenId = modelId || "";
  if (state.modelRknnConcurrencyBindingOpenId) {
    state.modelGameBindingOpenId = "";
    state.modelPresetBindingOpenId = "";
    state.modelRemoteFrameBindingOpenId = "";
    state.modelHailoPipelineBindingOpenId = "";
  }
  rerenderCurrentModels();
}

function setModelHailoPipelineBindingOpen(modelId) {
  state.modelHailoPipelineBindingOpenId = modelId || "";
  if (state.modelHailoPipelineBindingOpenId) {
    state.modelGameBindingOpenId = "";
    state.modelPresetBindingOpenId = "";
    state.modelRemoteFrameBindingOpenId = "";
    state.modelRknnConcurrencyBindingOpenId = "";
  }
  rerenderCurrentModels();
}

function renderModels(payload, selectedModelId) {
  const models = payload && Array.isArray(payload.models) ? payload.models : [];
  const selected = selectedModelId ||
    (payload && payload.selected_model_id) ||
    (state.config && state.config.model_id) ||
    "";
  const nextSignature = modelListSignature(models);
  const modelExists = models.some((model) => model.id === selected);
  const nextSelected = modelExists ? selected : (models[0] && models[0].id) || "";
  if (state.modelPresetBindingOpenId && !models.some((model) => model.id === state.modelPresetBindingOpenId)) {
    state.modelPresetBindingOpenId = "";
  }
  if (state.modelGameBindingOpenId && !models.some((model) => model.id === state.modelGameBindingOpenId)) {
    state.modelGameBindingOpenId = "";
  }
  if (state.modelRemoteFrameBindingOpenId && !models.some((model) => model.id === state.modelRemoteFrameBindingOpenId)) {
    state.modelRemoteFrameBindingOpenId = "";
  }
  if (state.modelRknnConcurrencyBindingOpenId && !models.some((model) => model.id === state.modelRknnConcurrencyBindingOpenId)) {
    state.modelRknnConcurrencyBindingOpenId = "";
  }
  if (state.modelHailoPipelineBindingOpenId && !models.some((model) => model.id === state.modelHailoPipelineBindingOpenId)) {
    state.modelHailoPipelineBindingOpenId = "";
  }
  if (state.config && state.config.model_id !== nextSelected) {
    state.config = { ...state.config, model_id: nextSelected };
  }
  if (state.data && state.data.config && state.data.config.model_id !== nextSelected) {
    state.data.config = { ...state.data.config, model_id: nextSelected };
  }
  state.modelListSignature = nextSignature;
  renderModelGameFilters(models);
  renderModelBackendFilters(models);
  renderModelPanel(models, nextSelected);
  const nextCardsRenderSignature = [
    nextSignature,
    nextSelected,
    state.uiBrand,
    state.modelGameFilter,
    state.modelBackendFilter,
    state.presetListSignature || presetListSignature(state.presetNames || []),
    state.modelPresetBindingOpenId,
    state.modelGameBindingOpenId,
    state.modelRemoteFrameBindingOpenId,
    state.modelRknnConcurrencyBindingOpenId,
    state.modelHailoPipelineBindingOpenId,
  ].join("\u001e");
  if (state.modelCardsRenderSignature !== nextCardsRenderSignature) {
    state.modelCardsRenderSignature = nextCardsRenderSignature;
    renderModelCards(models, nextSelected);
  }

  const nextClassSignature = currentModelClassRenderSignature(nextSelected);
  if (state.configReady && state.aimClassRenderSignature && state.aimClassRenderSignature !== nextClassSignature) {
    renderAimProfiles(collectAimProfiles());
  }
  if (
    state.configReady &&
    state.autoTriggerClassRenderSignature &&
    state.autoTriggerClassRenderSignature !== nextClassSignature
  ) {
    renderAutoTriggerProfiles({ profiles: collectAutoTriggerProfiles() });
  }
  renderAutoBackFlickClassPicker(currentAutoBackFlickClassConfig());
  updatePresetCleanupButton();
}

function rerenderCurrentModels() {
  const models = state.data && Array.isArray(state.data.models) ? state.data.models : [];
  const selected = (state.config && state.config.model_id) ||
    (state.data && state.data.config && state.data.config.model_id) ||
    "";
  if (models.length > 0) {
    renderModels({ models, selected_model_id: selected }, selected);
  }
}

function presetListSignature(presets) {
  return presets.map((name) => String(name)).join("\u001f");
}

function presetBoundNameSet() {
  const models = state.data && Array.isArray(state.data.models) ? state.data.models : [];
  return new Set(models.map((model) => String(model && model.preset_name || "").trim()).filter(Boolean));
}

function unusedPresetNames(presets = state.presetNames) {
  if (!(state.data && Array.isArray(state.data.models))) {
    return [];
  }
  const boundNames = presetBoundNameSet();
  return (Array.isArray(presets) ? presets : [])
    .map((name) => String(name || "").trim())
    .filter((name) => name && !boundNames.has(name));
}

function updatePresetCleanupButton() {
  const button = $("cleanupUnusedPresetsButton");
  if (!button) {
    return;
  }
  const modelsReady = Boolean(state.data && Array.isArray(state.data.models));
  const unusedCount = unusedPresetNames().length;
  button.disabled = !modelsReady || unusedCount === 0;
  button.title = !modelsReady
    ? "模型列表尚未加载"
    : unusedCount > 0
      ? `可清理 ${unusedCount} 个未绑定模型的预设`
      : "没有未使用预设";
}

function getSelectedPresetName() {
  return state.presetSelectedName || "";
}

function currentPresetNameForAutosave() {
  const name = getSelectedPresetName();
  return name && state.presetNames.includes(name) ? name : "";
}

function syncSelectedPresetFromModelResult(result) {
  const presetName = String(result && (result.applied_preset_name || result.created_default_preset_name) || "");
  if (presetName && state.presetNames.includes(presetName)) {
    setSelectedPresetName(presetName);
  }
}

function queueCurrentPresetAutosave(config) {
  const name = currentPresetNameForAutosave();
  if (!name || !config) {
    return "";
  }
  state.presetAutoSaveName = name;
  state.presetAutoSaveConfig = cloneJson(config);
  state.presetAutoSaveQueued = true;
  flushCurrentPresetAutosave();
  return name;
}

async function flushCurrentPresetAutosave() {
  if (state.presetAutoSaveInFlight) {
    return;
  }
  state.presetAutoSaveInFlight = true;
  try {
    while (state.presetAutoSaveQueued) {
      const name = state.presetAutoSaveName;
      const config = state.presetAutoSaveConfig;
      state.presetAutoSaveQueued = false;
      if (!name || !config) {
        continue;
      }
      await api("/api/presets", {
        method: "POST",
        headers: { "Content-Type": "application/json" },
        body: JSON.stringify({ name, config }),
      });
      if (getSelectedPresetName() === name) {
        setApplyStatus("ready", "已同步并保存预设");
      }
    }
  } catch (error) {
    setApplyStatus("error", "预设保存失败");
    showToast(`预设自动保存失败：${error.message || String(error)}`, true);
  } finally {
    state.presetAutoSaveInFlight = false;
    if (state.presetAutoSaveQueued) {
      flushCurrentPresetAutosave();
    }
  }
}

function setPresetSuggestionOpen(open) {
  const list = $("presetSuggestionList");
  const combo = $("presetCombobox");
  const button = $("presetSelectButton");
  const nextOpen = !!open && state.presetNames.length > 0;

  state.presetSuggestionOpen = nextOpen;
  if (list) {
    list.hidden = !nextOpen;
  }
  if (combo) {
    combo.classList.toggle("is-open", nextOpen);
  }
  if (button) {
    button.disabled = state.presetNames.length === 0;
    button.setAttribute("aria-expanded", nextOpen ? "true" : "false");
  }
}

function updatePresetOptionSelection() {
  const list = $("presetSuggestionList");
  if (!list) {
    return;
  }
  Array.from(list.querySelectorAll("[data-preset-name]")).forEach((option) => {
    const selected = option.dataset.presetName === state.presetSelectedName;
    option.classList.toggle("is-active", selected);
    option.setAttribute("aria-selected", selected ? "true" : "false");
  });
}

function updatePresetCardSelection() {
  document.querySelectorAll("[data-preset-card]").forEach((card) => {
    const selected = card.dataset.presetCard === state.presetSelectedName;
    card.classList.toggle("is-active", selected);
    const status = card.querySelector("[data-preset-card-status]");
    if (status) {
      status.textContent = selected ? "自动保存目标" : "可加载";
    }
    const note = card.querySelector("[data-preset-card-note]");
    if (note) {
      note.textContent = selected ? "调整参数会自动保存到此预设" : "加载后会成为自动保存目标";
    }
  });
}

function setSelectedPresetName(name) {
  const nextName = name || "";
  state.presetSelectedName = nextName;
  document.documentElement.dataset.selectedPresetName = nextName;

  const value = $("presetSelectValue");
  if (value) {
    value.textContent = nextName || "暂无预设参数";
  }

  const button = $("presetSelectButton");
  if (button) {
    button.disabled = state.presetNames.length === 0;
    button.title = nextName || "暂无预设参数";
  }

  const meta = $("presetCurrentMeta");
  if (meta) {
    meta.textContent = nextName ? `当前选中：${nextName}，调整参数会自动保存` : "暂无";
  }

  setExportPresetLink(nextName);
  updatePresetOptionSelection();
  updatePresetCardSelection();
}

function renderPresetSuggestionList(presets) {
  const list = $("presetSuggestionList");
  if (!list) {
    return;
  }
  list.innerHTML = "";
  presets.forEach((name) => {
    const button = document.createElement("button");
    button.type = "button";
    button.className = "model-game-suggestion-option";
    button.dataset.presetName = name;
    button.setAttribute("role", "option");
    button.innerHTML = `<span>${escapeHtml(name)}</span>`;
    list.appendChild(button);
  });
  updatePresetOptionSelection();
}

function renderPresetCards(presets) {
  const list = $("presetCardList");
  const empty = $("presetEmptyState");
  if (!list) {
    return;
  }
  const safePresets = Array.isArray(presets) ? presets.map((name) => String(name)).filter(Boolean) : [];
  list.innerHTML = "";
  if (empty) {
    empty.hidden = safePresets.length > 0;
  }
  safePresets.forEach((name) => {
    const selected = name === getSelectedPresetName();
    const card = document.createElement("article");
    card.className = `preset-card${selected ? " is-active" : ""}`;
    card.dataset.presetCard = name;
    card.innerHTML = `
      <div class="preset-card-head">
        <div class="preset-card-title-row">
          <span class="preset-card-title">${escapeHtml(name)}</span>
          <button class="preset-rename-button" type="button" data-preset-action="rename" aria-label="重命名预设 ${escapeAttr(name)}" title="重命名">
            <svg viewBox="0 0 24 24" aria-hidden="true"><path d="M21.2 6.8a2.8 2.8 0 0 0-4-4L4 16l-1 5 5-1Z"/><path d="m15 5 4 4"/></svg>
          </button>
        </div>
        <span class="preset-card-status" data-preset-card-status>${selected ? "自动保存目标" : "可加载"}</span>
      </div>
      <small data-preset-card-note>${selected ? "调整参数会自动保存到此预设" : "加载后会成为自动保存目标"}</small>
      <div class="preset-card-actions">
        <button class="ghost-button" type="button" data-preset-action="load">加载</button>
        <button class="danger-button" type="button" data-preset-action="delete">删除</button>
        <a class="button-link" href="${escapeAttr(presetExportUrl(name))}" target="_blank" rel="noopener" data-preset-action="export">导出</a>
      </div>
    `;
    const renameButton = card.querySelector('[data-preset-action="rename"]');
    if (renameButton) {
      renameButton.addEventListener("click", (event) => {
        event.preventDefault();
        event.stopPropagation();
        setPresetRenameDialogOpen(true, name);
      });
    }
    const loadButton = card.querySelector('[data-preset-action="load"]');
    if (loadButton) {
      loadButton.addEventListener("click", (event) => {
        event.preventDefault();
        event.stopPropagation();
        runUiAction(() => loadPresetByName(name));
      });
    }
    const deleteButton = card.querySelector('[data-preset-action="delete"]');
    if (deleteButton) {
      deleteButton.addEventListener("click", (event) => {
        event.preventDefault();
        event.stopPropagation();
        runUiAction(() => deletePresetByName(name));
      });
    }
    const exportLink = card.querySelector('[data-preset-action="export"]');
    if (exportLink) {
      exportLink.addEventListener("click", (event) => event.stopPropagation());
    }
    list.appendChild(card);
  });
}

function renderPresets(presets) {
  const safePresets = Array.isArray(presets) ? presets.map((name) => String(name)).filter(Boolean) : [];
  const previous = getSelectedPresetName();
  const boundPreset = String(currentModel() && currentModel().preset_name || "");
  const nextSignature = presetListSignature(safePresets);
  const nextSelected = previous && safePresets.includes(previous)
    ? previous
    : boundPreset && safePresets.includes(boundPreset)
      ? boundPreset
    : (safePresets[0] || "");

  state.presetNames = safePresets;
  if (nextSignature !== state.presetListSignature) {
    state.presetListSignature = nextSignature;
    renderPresetSuggestionList(safePresets);
  }

  const summary = $("presetLibrarySummary");
  if (summary) {
    summary.textContent = safePresets.length > 0 ? `${safePresets.length} 个预设参数` : "暂无预设参数";
  }
  setSelectedPresetName(nextSelected);
  updatePresetCleanupButton();
  setPresetSuggestionOpen(state.presetSuggestionOpen);
  renderPresetCards(safePresets);
  rerenderCurrentModels();
}

function setHardwareStatus(id, text, live = false) {
  const el = $(id);
  if (!el) {
    return;
  }
  el.textContent = text;
  el.className = `pill ${live ? "status-badge live" : ""}`;
}

function randomInt(min, max) {
  const lower = Math.ceil(min);
  const upper = Math.floor(max);
  return Math.floor(Math.random() * (upper - lower + 1)) + lower;
}

function randomHex(width, min = 1, max = null) {
  const limit = max === null ? (16 ** width) - 1 : max;
  return `0x${randomInt(min, limit).toString(16).padStart(width, "0")}`;
}

function randomLetters(length) {
  const alphabet = "ABCDEFGHIJKLMNOPQRSTUVWXYZ";
  let text = "";
  for (let i = 0; i < length; i += 1) {
    text += alphabet[randomInt(0, alphabet.length - 1)];
  }
  return text;
}

function randomSerial(prefix = "") {
  return `${prefix}${randomInt(0x10000000, 0xffffffff).toString(16).toUpperCase()}`.slice(0, 32);
}

function randomUsbId(min, max, excludedValues = []) {
  const excluded = new Set(excludedValues);
  let value = 0;
  do {
    value = randomInt(min, max);
  } while (excluded.has(value));
  return `0x${value.toString(16).padStart(4, "0")}`;
}

function randomMouseBrand() {
  const brands = ["Logitech", "Razer", "SteelSeries", "Corsair", "HyperX", "BenQ", "Pulsar", "Endgame"];
  return brands[randomInt(0, brands.length - 1)];
}

function displayModeLine(label, value) {
  return `<div><span>${escapeHtml(label)}</span><strong>${escapeHtml(value || "--")}</strong></div>`;
}

function parseDynamicDisplayModeToken(token) {
  const match = DISPLAY_DYNAMIC_MODE_RE.exec(String(token || "").trim());
  if (!match) {
    return null;
  }
  return {
    width: Number(match[1]),
    height: Number(match[2]),
    refresh: Number(match[3]),
  };
}

function displayDynamicModePixelClockKHz(width, height, refresh) {
  if (!width || !height || !refresh) {
    return 0;
  }
  return Math.floor(((width + DISPLAY_DYNAMIC_MODE_H_BLANK) * (height + DISPLAY_DYNAMIC_MODE_V_BLANK) * refresh + 500) / 1000);
}

function displayDynamicModeMaxRefresh(width, height) {
  if (!width || !height) {
    return DISPLAY_DYNAMIC_MODE_MIN_REFRESH;
  }
  const totalPixels = (width + DISPLAY_DYNAMIC_MODE_H_BLANK) * (height + DISPLAY_DYNAMIC_MODE_V_BLANK);
  if (!Number.isFinite(totalPixels) || totalPixels <= 0) {
    return DISPLAY_DYNAMIC_MODE_MIN_REFRESH;
  }
  const maxByClock = Math.floor((DISPLAY_DYNAMIC_MODE_MAX_PIXEL_CLOCK_KHZ * 1000) / totalPixels);
  return clamp(maxByClock, DISPLAY_DYNAMIC_MODE_MIN_REFRESH, DISPLAY_DYNAMIC_MODE_MAX_REFRESH);
}

function displayDynamicModeToken(width, height, refresh) {
  return `${Math.round(width)}x${Math.round(height)}@${Math.round(refresh)}`;
}

function displayDynamicModeLabel(width, height, refresh, suffix = "自定义") {
  return `${Math.round(width)}x${Math.round(height)} @ ${Math.round(refresh)} Hz ${suffix}`;
}

function displayDynamicModeLabelFromToken(token) {
  const mode = parseDynamicDisplayModeToken(token);
  return mode ? displayDynamicModeLabel(mode.width, mode.height, mode.refresh) : String(token || "");
}

function renderDisplayModeSummary(payload) {
  const el = $("displayModeSummary");
  if (!el) {
    return;
  }
  const info = (payload && payload.display_mode) || {};
  const monitor = info.real_monitor || {};
  const loopout = (payload && payload.loopout) || {};
  const config = (payload && payload.config) || {};
  const modes = Array.isArray(info.advertised_modes) ? info.advertised_modes : [];
  const modeText = modes.length
    ? modes.map((mode) => mode.label || `${mode.width || "--"}x${mode.height || "--"}@${mode.refresh || "--"}`).join(" / ")
    : "未读取";
  const current = loopout.width && loopout.height
    ? `${loopout.width}x${loopout.height}${loopout.refresh ? ` @ ${loopout.refresh} Hz` : ""}`
    : "--";
  const overlayConfigured = !!config.loopout_overlay_enabled;
  let overlayStatus = "关闭";
  if (overlayConfigured && !loopout.enabled) {
    overlayStatus = "等待环出";
  } else if (overlayConfigured && !loopout.overlay_enabled) {
    overlayStatus = "等待 AI";
  } else if (overlayConfigured) {
    const plane = loopout.overlay_plane_id
      ? ` · plane ${loopout.overlay_plane_id}${loopout.overlay_plane_name ? ` ${loopout.overlay_plane_name}` : ""}`
      : "";
    const format = loopout.overlay_pixel_format ? ` · ${loopout.overlay_pixel_format}` : "";
    const draw = Number(loopout.overlay_draw_ms || 0) > 0
      ? ` · ${formatNumber(loopout.overlay_draw_ms)} ms`
      : "";
    overlayStatus = `${loopout.overlay_status || "等待"}${plane}${format}${draw}`;
  }
  el.innerHTML = [
    displayModeLine("真实显示器", monitor.connected ? (monitor.name || "已连接") : "未连接"),
    displayModeLine("硬件身份", `${monitor.vendor || "???"} ${monitor.product_id || "0x0000"} ${monitor.serial || ""}`.trim()),
    displayModeLine("Windows 可选模式", modeText),
    displayModeLine("环出颜色格式", loopout.enabled ? (loopout.pixel_format || config.loopout_pixel_format || "rgb888") : (config.loopout_pixel_format || loopout.pixel_format || "rgb888")),
    displayModeLine("环出状态", loopout.enabled ? `${loopout.status || "开启"}${loopout.fps ? ` · ${formatNumber(loopout.fps)} FPS` : ""}` : "关闭"),
    displayModeLine("当前环出", current),
    loopout.drm_device ? displayModeLine("DRM 设备", loopout.drm_device) : "",
    displayModeLine("检测框状态", overlayStatus),
    loopout.last_error ? displayModeLine("环出错误", loopout.last_error) : "",
    loopout.overlay_last_error ? displayModeLine("检测框错误", loopout.overlay_last_error) : "",
  ].filter(Boolean).join("");
  el.hidden = false;
}

function modeOptionLabel(mode) {
  if (!mode) {
    return "";
  }
  if (mode.label) {
    return mode.label.replace("@", " @ ") + (mode.refresh ? " Hz" : "");
  }
  const width = Number(mode.width || 0);
  const height = Number(mode.height || 0);
  const refresh = Number(mode.refresh || 0);
  if (width && height && refresh) {
    return `${width}x${height} @ ${refresh} Hz`;
  }
  return String(mode.token || "");
}

function normalizeModeToken(token) {
  return String(token || "").trim();
}

function isDisplayCompatNativeMode(token) {
  return /compat$/i.test(normalizeModeToken(token));
}

function refreshDisplayNativeModeOptions(payload) {
  const select = $("display_native_mode");
  if (!select) {
    return;
  }
  const config = (payload && payload.config) || {};
  const selected = normalizeModeToken(config.native_mode || select.value);
  const info = (payload && payload.display_mode) || {};
  const loopoutEnabled = !!(info.loopout_enabled || (payload && payload.config && payload.config.loopout_enabled));
  const availableModes = Array.isArray(info.available_modes) ? info.available_modes : [];
  const options = new Map();
  options.set("", DISPLAY_NATIVE_MODE_LABELS[""]);
  if (!loopoutEnabled || availableModes.length === 0) {
    DISPLAY_NATIVE_MODES.forEach((token) => {
      options.set(token, DISPLAY_NATIVE_MODE_LABELS[token] || token);
    });
  }
  if (loopoutEnabled) {
    availableModes.forEach((mode) => {
      const token = normalizeModeToken(mode.token);
      if (!token || isDisplayCompatNativeMode(token) || options.has(token)) {
        return;
      }
      options.set(token, modeOptionLabel(mode));
    });
  }
  if (selected && !isDisplayCompatNativeMode(selected) && !options.has(selected) && (!loopoutEnabled || parseDynamicDisplayModeToken(selected))) {
    options.set(selected, displayDynamicModeLabelFromToken(selected));
  }
  select.innerHTML = "";
  options.forEach((label, value) => {
    const option = document.createElement("option");
    option.value = value;
    option.textContent = label;
    select.appendChild(option);
  });
  select.value = options.has(selected) ? selected : "";
}

function displayNativeModeAllowedValues() {
  const select = $("display_native_mode");
  if (!select) {
    return DISPLAY_NATIVE_MODES;
  }
  return Array.from(select.options || []).map((option) => option.value);
}

function setDisplayEdidModeStatus(message, isError = false) {
  const status = $("displayEdidModeStatus");
  if (!status) {
    return;
  }
  status.textContent = message || "";
  status.classList.toggle("is-error", !!isError);
}

function setDisplayEdidModeInvalidFields(ids = []) {
  const invalidIds = new Set(ids);
  DISPLAY_EDID_MODE_FIELD_IDS.forEach((id) => {
    const el = $(id);
    if (el) {
      el.classList.toggle("is-invalid", invalidIds.has(id));
    }
  });
}

function readDisplayEdidModeInputs() {
  const width = Number(getString("displayEdidModeWidth"));
  const height = Number(getString("displayEdidModeHeight"));
  const refresh = Number(getString("displayEdidModeRefresh"));
  return {
    width: Number.isFinite(width) ? Math.round(width) : 0,
    height: Number.isFinite(height) ? Math.round(height) : 0,
    refresh: Number.isFinite(refresh) ? Math.round(refresh) : 0,
  };
}

function validateDisplayEdidModeInputs({ updateUi = true } = {}) {
  const { width, height, refresh } = readDisplayEdidModeInputs();
  const messages = [];
  const invalidIds = [];
  const widthValid = width >= DISPLAY_DYNAMIC_MODE_MIN_WIDTH && width <= DISPLAY_DYNAMIC_MODE_MAX_WIDTH;
  const heightValid = height >= DISPLAY_DYNAMIC_MODE_MIN_HEIGHT && height <= DISPLAY_DYNAMIC_MODE_MAX_HEIGHT;
  const refreshValid = refresh >= DISPLAY_DYNAMIC_MODE_MIN_REFRESH && refresh <= DISPLAY_DYNAMIC_MODE_MAX_REFRESH;

  if (!widthValid) {
    invalidIds.push("displayEdidModeWidth");
    messages.push(`宽度范围 ${DISPLAY_DYNAMIC_MODE_MIN_WIDTH}-${DISPLAY_DYNAMIC_MODE_MAX_WIDTH}`);
  }
  if (!heightValid) {
    invalidIds.push("displayEdidModeHeight");
    messages.push(`高度范围 ${DISPLAY_DYNAMIC_MODE_MIN_HEIGHT}-${DISPLAY_DYNAMIC_MODE_MAX_HEIGHT}`);
  }
  if (!refreshValid) {
    invalidIds.push("displayEdidModeRefresh");
    messages.push(`刷新率范围 ${DISPLAY_DYNAMIC_MODE_MIN_REFRESH}-${DISPLAY_DYNAMIC_MODE_MAX_REFRESH} Hz`);
  }

  const maxRefresh = widthValid && heightValid ? displayDynamicModeMaxRefresh(width, height) : 0;
  const pixelClockKHz = widthValid && heightValid && refreshValid
    ? displayDynamicModePixelClockKHz(width, height, refresh)
    : 0;
  if (widthValid && heightValid && refreshValid) {
    if (pixelClockKHz < DISPLAY_DYNAMIC_MODE_MIN_PIXEL_CLOCK_KHZ) {
      invalidIds.push("displayEdidModeRefresh");
      messages.push("当前刷新率的像素时钟低于 EDID DTD 下限 25 MHz");
    } else if (pixelClockKHz > DISPLAY_DYNAMIC_MODE_MAX_PIXEL_CLOCK_KHZ) {
      invalidIds.push("displayEdidModeRefresh");
      messages.push(`当前组合约 ${formatNumber(pixelClockKHz / 1000, 2)} MHz，超过 HDMI RX 600 MHz 上限`);
    }
  }

  if (updateUi) {
    setDisplayEdidModeInvalidFields(invalidIds);
    if (messages.length) {
      setDisplayEdidModeStatus(messages.join("；"), true);
    } else {
      setDisplayEdidModeStatus(
        `自动计算最高 ${maxRefresh} Hz；当前约 ${formatNumber(pixelClockKHz / 1000, 2)} MHz / 600 MHz`,
        false,
      );
    }
  }
  return {
    valid: messages.length === 0,
    width,
    height,
    refresh,
    maxRefresh,
    pixelClockKHz,
  };
}

function updateDisplayEdidModeRefreshFromSize() {
  const width = Number(getString("displayEdidModeWidth"));
  const height = Number(getString("displayEdidModeHeight"));
  if (
    Number.isFinite(width)
    && Number.isFinite(height)
    && width >= DISPLAY_DYNAMIC_MODE_MIN_WIDTH
    && width <= DISPLAY_DYNAMIC_MODE_MAX_WIDTH
    && height >= DISPLAY_DYNAMIC_MODE_MIN_HEIGHT
    && height <= DISPLAY_DYNAMIC_MODE_MAX_HEIGHT
  ) {
    setValue("displayEdidModeRefresh", displayDynamicModeMaxRefresh(Math.round(width), Math.round(height)));
  }
  validateDisplayEdidModeInputs();
}

function parseDisplayModeText(text) {
  const match = String(text || "").match(/(\d{3,4})\s*x\s*(\d{3,4})(?:\s*@\s*(\d{2,3}))?/i);
  if (!match) {
    return null;
  }
  return {
    width: Number(match[1]),
    height: Number(match[2]),
    refresh: match[3] ? Number(match[3]) : 0,
  };
}

function displayEdidModeDefaults() {
  const select = $("display_native_mode");
  let mode = select
    ? (parseDynamicDisplayModeToken(select.value) || parseDisplayModeText(select.selectedOptions && select.selectedOptions[0] && select.selectedOptions[0].textContent))
    : null;
  if (!mode) {
    const loopout = state.displayHardwarePayload && state.displayHardwarePayload.loopout;
    if (loopout && loopout.width && loopout.height) {
      mode = { width: Number(loopout.width), height: Number(loopout.height), refresh: Number(loopout.refresh || 0) };
    }
  }
  const width = clamp(
    Math.round((mode && mode.width) || 1920),
    DISPLAY_DYNAMIC_MODE_MIN_WIDTH,
    DISPLAY_DYNAMIC_MODE_MAX_WIDTH,
  );
  const height = clamp(
    Math.round((mode && mode.height) || 1080),
    DISPLAY_DYNAMIC_MODE_MIN_HEIGHT,
    DISPLAY_DYNAMIC_MODE_MAX_HEIGHT,
  );
  return {
    width,
    height,
    refresh: displayDynamicModeMaxRefresh(width, height),
  };
}

function addDisplayNativeModeOption(token, label) {
  const select = $("display_native_mode");
  if (!select) {
    return;
  }
  let option = Array.from(select.options || []).find((item) => item.value === token);
  if (!option) {
    option = document.createElement("option");
    option.value = token;
    select.appendChild(option);
  }
  option.textContent = label || displayDynamicModeLabelFromToken(token);
  select.value = token;
}

function setDisplayEdidModeDialogOpen(open) {
  const dialog = $("displayEdidModeDialog");
  if (!dialog) {
    return;
  }
  dialog.hidden = !open;
  setAnyModalOpen();
  if (open) {
    const defaults = displayEdidModeDefaults();
    setValue("displayEdidModeWidth", defaults.width);
    setValue("displayEdidModeHeight", defaults.height);
    setValue("displayEdidModeRefresh", defaults.refresh);
    validateDisplayEdidModeInputs();
    const widthInput = $("displayEdidModeWidth");
    if (widthInput) {
      widthInput.focus();
      widthInput.select();
    }
  } else {
    setDisplayEdidModeInvalidFields([]);
  }
}

function saveDisplayEdidModeFromDialog() {
  const result = validateDisplayEdidModeInputs();
  if (!result.valid) {
    return false;
  }
  const token = displayDynamicModeToken(result.width, result.height, result.refresh);
  addDisplayNativeModeOption(token, displayDynamicModeLabel(result.width, result.height, result.refresh));
  state.displayHardwarePayload = {
    ...(state.displayHardwarePayload || {}),
    config: {
      ...((state.displayHardwarePayload && state.displayHardwarePayload.config) || {}),
      native_mode: token,
    },
  };
  setValidation("displayHardwareValidation", ["display_native_mode"], []);
  setDisplayEdidModeDialogOpen(false);
  showToast("EDID模式已加入首选模式，点击保存并应用写入设备");
  return true;
}

function updateDisplayCustomModeButtonUi() {
  const button = $("addDisplayEdidModeButton");
  if (!button) {
    return;
  }
  button.disabled = false;
  button.title = getCheckbox("display_loopout_enabled")
    ? "自定义模式会作为环出 EDID 首选模式写入"
    : "";
}

function realMonitorIdentityFromPayload(payload) {
  const monitor = payload && payload.display_mode && payload.display_mode.real_monitor;
  if (!monitor || !monitor.connected || !monitor.edid_valid) {
    return null;
  }
  const name = printableAscii(monitor.name || "", 13);
  const vendor = vendorCode(monitor.vendor || "");
  const productId = normalizeHexValue(monitor.product_id || "", 4);
  const serial = normalizeHexValue(monitor.serial || "", 8);
  if (!name || !vendor || !productId || !serial) {
    return null;
  }
  return { name, vendor, product_id: productId, serial };
}

function setDisplayIdentityReadonly(readonly) {
  DISPLAY_IDENTITY_FIELD_IDS.forEach((id) => {
    const el = $(id);
    if (!el) {
      return;
    }
    el.readOnly = readonly;
    el.setAttribute("aria-readonly", readonly ? "true" : "false");
    const field = el.closest(".display-identity-field");
    if (field) {
      field.classList.toggle("is-readonly", readonly);
    }
  });
  const randomButton = $("randomDisplayHardwareButton");
  if (randomButton) {
    randomButton.disabled = readonly;
  }
}

function applyDisplayLoopoutIdentityUi() {
  const loopoutEnabled = getCheckbox("display_loopout_enabled");
  const identity = state.displayRealMonitor;
  if (loopoutEnabled && identity) {
    setValue("display_name", identity.name);
    setValue("display_vendor", identity.vendor);
    setValue("display_product_id", identity.product_id);
    setValue("display_serial", identity.serial);
  }
  setDisplayIdentityReadonly(loopoutEnabled);
  updateDisplayCustomModeButtonUi();
}

function populateDisplayHardware(payload) {
  const config = (payload && payload.config) || {};
  state.displayHardwarePayload = payload || null;
  state.displayRealMonitor = realMonitorIdentityFromPayload(payload);
  setValue("display_device", config.device || "auto");
  setCheckbox("display_native_only", !!config.native_only);
  setCheckbox("display_loopout_enabled", !!config.loopout_enabled);
  setCheckbox("display_loopout_overlay_enabled", !!config.loopout_overlay_enabled);
  setValue("display_loopout_pixel_format", config.loopout_pixel_format || "rgb888");
  refreshDisplayNativeModeOptions(payload);
  setValue("display_native_mode", config.native_mode || "");
  setValue("display_name", config.name || "OPI-COMPAT");
  setValue("display_vendor", config.vendor || "OPI");
  setValue("display_product_id", config.product_id || "0x3588");
  setValue("display_serial", config.serial || "0x20260414");

  const status = payload && payload.status;
  const log = $("displayHardwareLog");
  if (log) {
    log.textContent = status && status.output ? status.output.trim() || "无输出" : "等待读取";
  }
  setHardwareStatus("displayHardwareStatus", payload && payload.available ? "可用" : "未安装", !!(payload && payload.available));
  renderDisplayModeSummary(payload);
  // door display removed
}

function collectDisplayHardware() {
  const vendor = vendorCode(getString("display_vendor")) || "OPI";
  return {
    device: getString("display_device") || "auto",
    profile: "boot-safe-full",
    native_mode: getString("display_native_mode"),
    native_only: getCheckbox("display_native_only"),
    loopout_enabled: getCheckbox("display_loopout_enabled"),
    loopout_overlay_enabled: getCheckbox("display_loopout_overlay_enabled"),
    loopout_pixel_format: getString("display_loopout_pixel_format") || "rgb888",
    name: printableAscii(getString("display_name"), 13) || "OPI-COMPAT",
    vendor,
    product_id: hexText(getString("display_product_id"), 4) || "0x3588",
    serial: hexText(getString("display_serial"), 8) || "0x20260414",
  };
}

function displayHardwareEdidChanged(config) {
  const current = (state.displayHardwarePayload && state.displayHardwarePayload.config) || {};
  const keys = ["device", "profile", "native_mode", "native_only", "loopout_enabled", "name", "vendor", "product_id", "serial"];
  return keys.some((key) => String(config[key] ?? "") !== String(current[key] ?? ""));
}

function validateDisplayHardware() {
  const fieldIds = ["display_native_mode", "display_name", "display_vendor", "display_product_id", "display_serial", "display_native_only", "display_loopout_enabled", "display_loopout_overlay_enabled", "display_loopout_pixel_format", "display_loopout_overlay_thickness", "display_loopout_overlay_color"];
  const messages = [];
  const nativeMode = getString("display_native_mode");
  const nativeOnly = getCheckbox("display_native_only");
  const loopoutEnabled = getCheckbox("display_loopout_enabled");
  const loopoutOverlayEnabled = getCheckbox("display_loopout_overlay_enabled");
  const loopoutPixelFormat = getString("display_loopout_pixel_format") || "rgb888";
  if (loopoutEnabled && state.displayRealMonitor) {
    // door display removed
  }
  const name = getString("display_name");
  const vendor = getString("display_vendor").toUpperCase();
  const productId = normalizeHexValue(getString("display_product_id"), 4);
  const serial = normalizeHexValue(getString("display_serial"), 8);

  if (!displayNativeModeAllowedValues().includes(nativeMode)) {
    messages.push({ id: "display_native_mode", text: "首选模式必须从列表中选择。" });
  }
  if (nativeOnly && !nativeMode) {
    messages.push({ id: "display_native_only", text: "仅输出首选模式需要先选择一个首选模式。" });
  }
  if (loopoutOverlayEnabled && !loopoutEnabled) {
    messages.push({ id: "display_loopout_overlay_enabled", text: "显示检测框需要先开启环出模式。" });
  }
  if (!["bgr888", "rgb888"].includes(loopoutPixelFormat)) {
    messages.push({ id: "display_loopout_pixel_format", text: "环出颜色格式必须选择 BGR888 或 RGB888。" });
  }
  if (!name || name !== printableAscii(name, 13)) {
    messages.push({ id: "display_name", text: "显示器名称只能填写 1-13 个英文、数字或 ASCII 符号。" });
  }
  if (!/^[A-Z]{3}$/.test(vendor)) {
    messages.push({ id: "display_vendor", text: "厂商代码必须是 3 个大写英文字母，例如 LVP、OPI、DEL。" });
  }
  if (!productId) {
    messages.push({ id: "display_product_id", text: "产品 ID 必须是非零十六进制，范围 0x0001-0xffff。" });
  }
  if (!serial) {
    messages.push({ id: "display_serial", text: "序列号必须是非零十六进制，范围 0x00000001-0xffffffff。" });
  }

  setValidation("displayHardwareValidation", fieldIds, messages);
  if (messages.length > 0) {
    throw new Error("显示器硬件信息不符合规范");
  }

  setValue("display_vendor", vendor);
  setValue("display_product_id", productId);
  setValue("display_serial", serial);
  return {
    device: getString("display_device") || "auto",
    profile: "boot-safe-full",
    native_mode: nativeMode,
    native_only: nativeOnly,
    loopout_enabled: loopoutEnabled,
    loopout_overlay_enabled: loopoutOverlayEnabled,
    loopout_pixel_format: loopoutPixelFormat,
    name,
    vendor,
    product_id: productId,
    serial,
  };
}

function randomizeDisplayHardware() {
  if (getCheckbox("display_loopout_enabled")) {
    // door display removed
    return;
  }
  const vendor = randomLetters(3);
  setValue("display_vendor", vendor);
  setValue("display_name", `${vendor}-${randomInt(0x100000, 0xffffff).toString(16).toUpperCase()}`.slice(0, 13));
  setValue("display_product_id", randomHex(4, 1, 0xfffe));
  setValue("display_serial", randomHex(8, 1, 0xffffffff));
  setValidation("displayHardwareValidation", ["display_name", "display_vendor", "display_product_id", "display_serial"], []);
}

function populateMouseProxyTiming(payload) {
  const timing = (payload && payload.timing) || payload || {};
  const mouseSettleDelay = Number(timing.mouse_settle_delay_sec);
  const identityChangeSettleDelay = Number(timing.identity_change_settle_delay_sec);
  setValue("mouse_settle_delay_sec", Number.isFinite(mouseSettleDelay) ? mouseSettleDelay : 1);
  setValue(
    "mouse_identity_change_settle_delay_sec",
    Number.isFinite(identityChangeSettleDelay) ? identityChangeSettleDelay : 0.5,
  );
}

function populateMouseHardware(payload) {
  const config = (payload && payload.config) || {};
  Object.entries(config).forEach(([key, value]) => {
    setValue(`mouse_${key}`, value);
  });
  const mode = (payload && payload.mode) || "full_passthrough";
  const isSynthetic = mode === "synthetic";
  const connected = !!(payload && payload.connected);
  const usingOriginalProfile = payload && payload.config_source === "original_mouse_profile";
  const usingPhysicalMouse = payload && payload.config_source === "sysfs_usb_mouse";
  const modeInput = document.querySelector(`input[name="mouse_proxy_mode"][value="${mode}"]`);
  if (modeInput) {
    modeInput.checked = true;
  }
  populateMouseProxyTiming(payload);
  setHardwareStatus(
    "mouseHardwareStatus",
    connected
      ? (isSynthetic
        ? "合成鼠标随机身份"
        : (usingOriginalProfile || usingPhysicalMouse ? "完整透传真实鼠标" : "完整透传"))
      : "未连接",
    connected,
  );
  updateMouseHardwareModeUi(payload);
}

function updateMouseHardwareModeUi(payload = null) {
  const selected = document.querySelector('input[name="mouse_proxy_mode"]:checked');
  const mode = selected ? selected.value : ((payload && payload.mode) || "full_passthrough");
  const isSynthetic = mode === "synthetic";
  const editableIds = [
    "mouse_usb_vid",
    "mouse_usb_pid",
    "mouse_usb_manufacturer",
    "mouse_usb_product",
    "mouse_usb_serial",
    "mouse_usb_configuration",
  ];
  editableIds.forEach((id) => {
    const el = $(id);
    if (el) {
      el.disabled = !isSynthetic;
    }
  });
  const randomButton = $("randomMouseHardwareButton");
  const saveButton = $("saveMouseHardwareButton");
  if (randomButton) {
    randomButton.disabled = !isSynthetic;
  }
  if (saveButton) {
    saveButton.disabled = !isSynthetic;
  }
  const hint = $("mouseHardwareModeHint");
  if (hint) {
    if (isSynthetic) {
      hint.textContent = "合成模式可保存随机身份，并通过标准 HID 鼠标输出。";
    } else {
      hint.textContent = "完整透传会暴露原始复合 HID 接口，用于厂商驱动和真实鼠标信息。";
    }
  }
}

function validateMouseProxyTiming() {
  const fields = [
    ["mouse_settle_delay_sec", "普通启动等待", "mouse_settle_delay_sec"],
    ["mouse_identity_change_settle_delay_sec", "身份切换等待", "identity_change_settle_delay_sec"],
  ];
  const timing = {};
  const messages = [];
  fields.forEach(([id, label, key]) => {
    const input = $(id);
    const raw = input ? input.value.trim() : "";
    const value = raw === "" ? NaN : Number(raw);
    if (!Number.isFinite(value) || value < 0 || value > 30) {
      messages.push({ id, text: `${label}必须在 0-30 秒之间。` });
      return;
    }
    timing[key] = Number(value.toFixed(3));
  });
  setValidation("mouseHardwareValidation", fields.map(([id]) => id), messages);
  if (messages.length > 0) {
    throw new Error("USB 鼠标等待时间不符合规范");
  }
  return timing;
}

function collectMouseHardware() {
  return {
    usb_vid: hexText(getString("mouse_usb_vid"), 4),
    usb_pid: hexText(getString("mouse_usb_pid"), 4),
    usb_bcd_usb: getString("mouse_usb_bcd_usb"),
    usb_bcd_device: getString("mouse_usb_bcd_device"),
    usb_device_class: getNumber("mouse_usb_device_class", 0),
    usb_device_subclass: getNumber("mouse_usb_device_subclass", 0),
    usb_device_protocol: getNumber("mouse_usb_device_protocol", 0),
    usb_max_power: getNumber("mouse_usb_max_power", 250),
    hid_protocol: getNumber("mouse_hid_protocol", 2),
    hid_subclass: getNumber("mouse_hid_subclass", 1),
    hid_report_length: getNumber("mouse_hid_report_length", 4),
    hid_interval: getNumber("mouse_hid_interval", 1),
    usb_manufacturer: printableAscii(getString("mouse_usb_manufacturer"), 48),
    usb_product: printableAscii(getString("mouse_usb_product"), 64),
    usb_serial: printableAscii(getString("mouse_usb_serial"), 64),
    usb_configuration: printableAscii(getString("mouse_usb_configuration"), 32) || "Mouse",
    hid_report_desc_hex: getString("mouse_hid_report_desc_hex").replace(/\s+/g, ""),
  };
}

function validateMouseHardware() {
  const fieldIds = [
    "mouse_usb_vid",
    "mouse_usb_pid",
    "mouse_usb_manufacturer",
    "mouse_usb_product",
    "mouse_usb_serial",
    "mouse_usb_configuration",
  ];
  const messages = [];
  const vid = normalizeHexValue(getString("mouse_usb_vid"), 4);
  const pid = normalizeHexValue(getString("mouse_usb_pid"), 4);
  const manufacturer = getString("mouse_usb_manufacturer");
  const product = getString("mouse_usb_product");
  const serial = getString("mouse_usb_serial");
  const configuration = getString("mouse_usb_configuration");

  if (!vid) {
    messages.push({ id: "mouse_usb_vid", text: "VID 必须是非零十六进制，范围 0x0001-0xffff。" });
  }
  if (!pid) {
    messages.push({ id: "mouse_usb_pid", text: "PID 必须是非零十六进制，范围 0x0001-0xffff。" });
  }
  [
    ["mouse_usb_manufacturer", "制造商", manufacturer, 48],
    ["mouse_usb_product", "产品名", product, 64],
    ["mouse_usb_serial", "序列号", serial, 64],
    ["mouse_usb_configuration", "配置名", configuration, 32],
  ].forEach(([id, label, value, maxLength]) => {
    if (!value || value !== printableAscii(value, maxLength)) {
      messages.push({ id, text: `${label}只能填写 1-${maxLength} 个英文、数字或 ASCII 符号。` });
    }
  });

  setValidation("mouseHardwareValidation", fieldIds, messages);
  if (messages.length > 0) {
    throw new Error("鼠标硬件信息不符合规范");
  }

  setValue("mouse_usb_vid", vid);
  setValue("mouse_usb_pid", pid);
  return {
    ...collectMouseHardware(),
    usb_vid: vid,
    usb_pid: pid,
    usb_manufacturer: manufacturer,
    usb_product: product,
    usb_serial: serial,
    usb_configuration: configuration,
  };
}

function randomizeMouseHardware() {
  const brand = randomMouseBrand();
  setValue("mouse_usb_vid", randomUsbId(0x1000, 0xefff, [0x1d6b]));
  setValue("mouse_usb_pid", randomUsbId(0x0001, 0xfffe));
  setValue("mouse_usb_manufacturer", brand);
  setValue("mouse_usb_product", `${brand} USB Optical Mouse`);
  setValue("mouse_usb_serial", randomSerial(brand.slice(0, 2).toUpperCase()));
  setValidation("mouseHardwareValidation", [
    "mouse_usb_vid",
    "mouse_usb_pid",
    "mouse_usb_manufacturer",
    "mouse_usb_product",
    "mouse_usb_serial",
    "mouse_usb_configuration",
  ], []);
}

function hailoYesNo(value) {
  return value ? "正常" : "未就绪";
}

function hailoInstallRunning(install) {
  return install && install.status === "running";
}

function renderHailoStatus(payload) {
  const status = payload || {};
  state.hailo = status;
  const pcie = status.pcie || {};
  const driver = status.driver || {};
  const runtime = status.runtime || {};
  const device = status.device || {};
  const install = status.install || {};
  const scan = device.scan || {};
  const ready = !!status.ready;
  const pciePresent = !!pcie.present;
  const running = hailoInstallRunning(install);

  setText("hailoReadyPill", ready ? "已就绪" : pciePresent ? "已插卡" : "未插卡");
  setText("hailoPcieValue", pciePresent ? "已检测" : "未检测");
  setText("hailoDriverValue", hailoYesNo(driver.loaded));
  setText("hailoRuntimeValue", runtime.installed ? `已安装 ${runtime.expected_version || ""}`.trim() : "未安装");
  setText("hailoScanValue", scan.ok ? "正常" : "未通过");
  setText("hailoKernelValue", status.kernel_release || "--");
  setText("hailoInstallStage", install.status === "ready" ? "完成" : running ? "安装中" : install.status === "failed" ? "失败" : "空闲");

  const installButton = $("installHailoButton");
  if (installButton) {
    installButton.disabled = !pciePresent || running || ready;
    installButton.textContent = ready ? "Hailo-8 已就绪" : running ? "正在安装" : "安装 Hailo-8 依赖";
  }
  setText(
    "hailoInstallHint",
    !pciePresent
      ? "未检测到 Hailo-8 PCIe 设备，插卡后刷新状态。"
      : ready
        ? "驱动、HailoRT 和设备扫描均已就绪。"
        : running
          ? "正在下载并安装依赖，请保持设备联网。"
          : "检测到 Hailo-8，可从授权服务器安装驱动和 HailoRT。"
  );

  const progressPanel = $("hailoInstallProgress");
  const progressBar = $("hailoInstallProgressBar");
  const installStatus = install.status || "idle";
  if (progressPanel) {
    progressPanel.hidden = installStatus === "idle";
    progressPanel.classList.toggle("is-failed", installStatus === "failed");
    progressPanel.classList.toggle("is-success", installStatus === "ready");
  }
  const progress = Number(install.progress || 0);
  setText("hailoInstallMessage", install.error || install.message || "暂无安装任务");
  setText("hailoInstallPercent", `${Math.max(0, Math.min(100, Math.round(progress)))}%`);
  if (progressBar) {
    progressBar.style.width = `${Math.max(0, Math.min(100, progress))}%`;
  }
}

async function refreshHailoStatus({ silent = false } = {}) {
  if (state.hailoStatusInFlight) {
    return state.hailo;
  }
  state.hailoStatusInFlight = true;
  try {
    const payload = await api("/api/hailo/status");
    renderHailoStatus(payload);
    return payload;
  } catch (error) {
    if (!silent) {
      throw error;
    }
    return state.hailo;
  } finally {
    state.hailoStatusInFlight = false;
  }
}

function scheduleHailoStatusPolling() {
  if (state.hailoStatusTimer) {
    clearTimeout(state.hailoStatusTimer);
    state.hailoStatusTimer = null;
  }
  const install = state.hailo && state.hailo.install;
  if (!hailoInstallRunning(install)) {
    return;
  }
  state.hailoStatusTimer = setTimeout(async () => {
    await refreshHailoStatus({ silent: true });
    scheduleHailoStatusPolling();
  }, 1500);
}

async function installHailoDependencies() {
  const result = await api("/api/hailo/install", { method: "POST" });
  if (result && result.status) {
    renderHailoStatus({ ...(state.hailo || {}), install: result.status });
  }
  await refreshHailoStatus({ silent: true });
  scheduleHailoStatusPolling();
}

function bindHardwareInputFilters() {
  const filters = {
    display_name: (value) => printableAscii(value, 13),
    display_vendor: vendorCode,
    display_product_id: (value) => hexText(value, 4),
    display_serial: (value) => hexText(value, 8),
    mouse_usb_vid: (value) => hexText(value, 4),
    mouse_usb_pid: (value) => hexText(value, 4),
    mouse_usb_manufacturer: (value) => printableAscii(value, 48),
    mouse_usb_product: (value) => printableAscii(value, 64),
    mouse_usb_serial: (value) => printableAscii(value, 64),
    mouse_usb_configuration: (value) => printableAscii(value, 32),
    kmbox_ip: (value) => String(value || "").replace(/[^0-9.]/g, "").slice(0, 64),
    kmbox_uuid: normalizeKmboxUuid,
    catnet_ip: (value) => String(value || "").replace(/[^0-9.]/g, "").slice(0, 64),
    catnet_uuid: normalizeKmboxUuid,
  };
  Object.entries(filters).forEach(([id, filter]) => {
    const el = $(id);
    if (!el) {
      return;
    }
    el.addEventListener("input", () => {
      const filtered = filter(el.value);
      if (el.value !== filtered) {
        el.value = filtered;
      }
      if (id.startsWith("kmbox_")) {
        updateKmboxFormUi();
      } else if (id.startsWith("catnet_")) {
        updateCatnetFormUi();
      }
    });
  });
}

async function loadHardware() {
  const [display, mouse] = await Promise.all([
    api("/api/hardware/display"),
    api("/api/hardware/mouse"),
  ]);
  populateDisplayHardware(display);
  populateMouseHardware(mouse);
}

async function refreshAll() {
  let payload = await api("/api/state");
  payload = await maybeRunLicenseRecovery(payload);
  state.data = payload;
  applyFullState(payload);
  await loadHardware();
  await refreshMakcuDevices({ silent: true });
  await refreshFerrumDevices({ silent: true });
  await refreshKmboxbDevices({ silent: true });
  if (state.activePageId === "hailo-page") {
    await refreshHailoStatus({ silent: true });
    scheduleHailoStatusPolling();
  }
  setApplyStatus("ready", "已同步");
}

function applyFullState(payload) {
  state.data = payload;
  applyBrand(payload);
  populateForm(payload.config);
  applyLiveState(payload);
}

function applyLiveState(payload) {
  state.data = payload;
  applyBrand(payload);
  renderRuntime(payload);
  renderLicensePanel({
    license: payload.state && payload.state.license,
    core: payload.state && payload.state.core,
    version: payload.version,
    recovery: payload.recovery,
  });
  renderPresets(payload.presets || []);
  renderModels({ models: payload.models, selected_model_id: payload.config.model_id }, payload.config.model_id);
}

function hardenInputAutofillHints(root = document) {
  root.querySelectorAll("input[type='number']").forEach((input) => {
    input.setAttribute("autocomplete", "off");
  });
  root.querySelectorAll("input[type='password']").forEach((input) => {
    input.setAttribute("autocomplete", "new-password");
    input.setAttribute("autocapitalize", "off");
    input.setAttribute("spellcheck", "false");
    input.setAttribute("data-lpignore", "true");
    input.setAttribute("data-1p-ignore", "true");
    input.setAttribute("data-bwignore", "true");
  });
}

async function runUiAction(fn) {
  try {
    await fn();
  } catch (error) {
    const message = error.message || String(error);
    if (suppressMouseSwitchDaemonTimeout(message)) {
      return;
    }
    showToast(message, true);
  }
}

function enforceAutoBackFlickDirectionExclusion(changedId = "") {
  const randomDirection = $("autoBackFlickClassRandomDirection");
  const dodgeAway = $("autoBackFlickClassDodgeAway");
  if (!randomDirection || !dodgeAway) {
    return;
  }
  if (changedId === "autoBackFlickClassRandomDirection" && randomDirection.checked) {
    dodgeAway.checked = false;
  } else if (changedId === "autoBackFlickClassDodgeAway" && dodgeAway.checked) {
    randomDirection.checked = false;
  } else if (randomDirection.checked && dodgeAway.checked) {
    dodgeAway.checked = false;
  }
}

function bindConfigAutoApply() {
  document.querySelectorAll("[data-config]").forEach((el) => {
    if (el.type === "number") {
      el.setAttribute("autocomplete", "off");
      el.addEventListener("keydown", (event) => {
        if (event.key === "Enter") {
          event.preventDefault();
          el.blur();
        }
      });
      el.addEventListener("input", () => {
        if (el.id === "capture_crop_size" && el.value !== "" && Number.isFinite(Number(el.value))) {
          syncCropSizePresetRange(el.value);
          updateDynamicOffsetControlLimits();
        }
        if (AIM_OVERLAY_CONFIG_IDS.has(el.id)) {
          updateAimRangeOverlay();
        }
      });
      el.addEventListener("change", () => {
        clampNumberInputToLimits(el);
        enforceFanControlInputPairs(el.id);
        if (el.id === "capture_crop_size") {
          updateDynamicOffsetControlLimits({ clampValues: true });
        }
        if (AIM_OVERLAY_CONFIG_IDS.has(el.id)) {
          updateAimRangeOverlay();
        }
        requestApplyForNumberInput(el, 90);
      });
      return;
    }
    const isInstant = el.matches("select") || el.type === "checkbox" || el.type === "radio";
    const eventName = isInstant ? "change" : "input";
    const delay = isInstant ? 70 : 180;
    el.addEventListener(eventName, () => {
      enforceFanControlInputPairs(el.id);
      if (el.id === "capture_crop_size") {
        updateDynamicOffsetControlLimits({ clampValues: true });
      } else if (AIM_OVERLAY_CONFIG_IDS.has(el.id)) {
        updateAimRangeOverlay();
      }
      requestConfigApply(delay);
    });
  });
}

function updateAssistModuleCollapseStates() {
  document.querySelectorAll("#assist-page .assist-section").forEach((section) => {
    const active = section.id === (state.activeAssistSectionId || "assist-section-recoil");
    section.classList.toggle("is-active", active);
    section.hidden = !active;
  });
  document.querySelectorAll("[data-assist-section-target]").forEach((tab) => {
    const active = tab.dataset.assistSectionTarget === (state.activeAssistSectionId || "assist-section-recoil");
    tab.classList.toggle("is-active", active);
    tab.setAttribute("aria-selected", active ? "true" : "false");
  });
}

function bindAssistModuleCollapseState() {
  updateAssistModuleCollapseStates();
}

function initAssistSectionNavigation() {
  const tabs = Array.from(document.querySelectorAll("[data-assist-section-target]"));
  const sections = Array.from(document.querySelectorAll("[data-assist-section]"));
  if (tabs.length === 0 || sections.length === 0) {
    return;
  }

  function activate(sectionId, shouldScroll = false) {
    state.activeAssistSectionId = sectionId;
    updateAssistModuleCollapseStates();
    if (shouldScroll) {
      const assistPage = $("assist-page");
      if (assistPage) {
        assistPage.scrollIntoView({ block: "start", behavior: "smooth" });
      }
    }
  }

  tabs.forEach((tab) => {
    tab.addEventListener("click", () => activate(tab.dataset.assistSectionTarget, true));
  });
  activate(state.activeAssistSectionId, false);
}

function initPageNavigation() {
  const tabs = Array.from(document.querySelectorAll("[data-page-target]"));
  const pages = Array.from(document.querySelectorAll("[data-page]"));
  if (tabs.length === 0 || pages.length === 0) {
    return;
  }

  function activate(pageId) {
    if (state.navigationLockedToLicense && pageId !== LICENSE_GATE_PAGE_ID) {
      pageId = LICENSE_GATE_PAGE_ID;
    }
    state.activePageId = pageId;
    document.body.dataset.activePage = pageId;
    pages.forEach((page) => {
      page.classList.toggle("is-active", page.id === pageId);
    });
    tabs.forEach((tab) => {
      tab.classList.toggle("is-active", tab.dataset.pageTarget === pageId);
    });
    if (pageId === "hailo-page") {
      refreshHailoStatus({ silent: true }).then(() => scheduleHailoStatusPolling()).catch(() => {});
    }
    window.scrollTo({ top: 0, behavior: "smooth" });
  }

  tabs.forEach((tab) => {
    tab.addEventListener("click", () => activate(tab.dataset.pageTarget));
  });
  const initialPage = pages.find((page) => page.classList.contains("is-active"));
  document.body.dataset.activePage = (initialPage && initialPage.id) || "home-page";
  window.activatePage = activate;
}

function initControlSectionNavigation() {
  const tabs = Array.from(document.querySelectorAll("[data-control-section-target]"));
  const sections = Array.from(document.querySelectorAll("[data-control-section]"));
  if (tabs.length === 0 || sections.length === 0) {
    return;
  }

  function activate(sectionId, shouldScroll = false) {
    state.activeControlSectionId = sectionId;
    sections.forEach((section) => {
      const active = section.id === sectionId;
      section.classList.toggle("is-active", active);
      section.hidden = !active;
    });
    tabs.forEach((tab) => {
      const active = tab.dataset.controlSectionTarget === sectionId;
      tab.classList.toggle("is-active", active);
      tab.setAttribute("aria-selected", active ? "true" : "false");
    });
    const resetButton = $("resetControllerDefaultsButton");
    if (resetButton) {
      resetButton.hidden = sectionId === "control-section-auto-calibration";
    }
    updateAutoCalibrationPolling();
    if (shouldScroll) {
      const controlPage = $("control-page");
      if (controlPage) {
        controlPage.scrollIntoView({ block: "start", behavior: "smooth" });
      }
    }
  }

  tabs.forEach((tab) => {
    tab.addEventListener("click", () => activate(tab.dataset.controlSectionTarget, true));
  });
  activate(state.activeControlSectionId, false);
}

function setLicenseNavigationLock(locked) {
  const wasLocked = state.navigationLockedToLicense;
  state.navigationLockedToLicense = locked;
  document.body.classList.remove("license-loading");
  if (locked) {
    redirectToActivationPage();
    return;
  }
  document.body.classList.toggle("license-locked", locked);
  const overlay = $("licenseGateOverlay");
  if (overlay) {
    overlay.hidden = !locked;
  }
  document.querySelectorAll("[data-page-target]").forEach((tab) => {
    const allowed = tab.dataset.pageTarget === LICENSE_GATE_PAGE_ID;
    tab.disabled = locked && !allowed;
    tab.setAttribute("aria-disabled", locked && !allowed ? "true" : "false");
  });
  if (locked) {
    updateLicenseGateStatus();
    if (window.activatePage) {
      window.activatePage(LICENSE_GATE_PAGE_ID);
    }
    if (!wasLocked) {
      focusLicenseGateInput();
    }
  }
  updateDisclaimerActions();
}

async function applySelectedModel(model_id, selectedModel = null) {
  if (!model_id) {
    throw new Error("没有可用模型");
  }
  setApplyStatus("saving", "切换模型");
  const result = await api("/api/models/select", {
    method: "POST",
    headers: { "Content-Type": "application/json" },
    body: JSON.stringify({ model_id }),
  });
  if (result && result.config) {
    state.config = result.config;
    populateForm(result.config);
  }
  if (result && Array.isArray(result.models)) {
    state.data = { ...(state.data || {}), models: result.models };
  }
  if (result && Array.isArray(result.presets)) {
    renderPresets(result.presets);
  }
  syncSelectedPresetFromModelResult(result);
  if (state.data) {
    state.data.config = state.config || state.data.config;
  }
  const models = (state.data && state.data.models) || [];
  const model = (result && result.model) || selectedModel || models.find((item) => item.id === model_id) || null;
  const cropSize = modelInputCropSize(model);
  if (cropSize !== null) {
    const nextCapture = {
      ...((state.config && state.config.capture) || {}),
      crop_size: cropSize,
    };
    state.config = {
      ...(state.config || {}),
      capture: nextCapture,
    };
    if (state.data) {
      state.data.config = state.config;
    }
    setCropSizeValue(cropSize);
    updateAimRangeOverlay();
    await applyConfigNow();
  }
  renderModels({ models: (state.data && state.data.models) || [], selected_model_id: model_id }, model_id);
  setApplyStatus("ready", "已同步");
  return result;
}

async function copyModelDeviceCode() {
  const result = await api("/api/models/device-code");
  const code = String(result && result.code ? result.code : "").trim();
  if (!code) {
    throw new Error("设备码为空");
  }
  await copyTextToClipboard(code);
  showToast("模型设备码已复制");
}

async function copyLanAccessUrl(button) {
  const url = String(button && button.dataset ? button.dataset.copyLanUrl || "" : "").trim();
  if (!url) {
    throw new Error("没有可复制的地址");
  }
  await copyTextToClipboard(url);
  showToast("访问地址已复制");
}

async function bindModelPreset(model_id, preset_name) {
  const result = await api("/api/models/bind-preset", {
    method: "POST",
    headers: { "Content-Type": "application/json" },
    body: JSON.stringify({ model_id, preset_name }),
  });
  if (state.data && Array.isArray(result.models)) {
    state.data.models = result.models;
  }
  renderModels({
    models: (result && result.models) || (state.data && state.data.models) || [],
    selected_model_id: (result && result.selected_model_id) || (state.config && state.config.model_id) || "",
  });
  const nextName = String(result && result.model && result.model.preset_name || "");
  const selectedModelId = String((result && result.selected_model_id) || (state.config && state.config.model_id) || "");
  if (nextName && selectedModelId === model_id && state.presetNames.includes(nextName)) {
    setSelectedPresetName(nextName);
  } else if (!nextName && selectedModelId === model_id) {
    setSelectedPresetName("");
  }
  return result;
}

async function updateModelGameProfile(model_id, game_profile) {
  const nextGameProfile = String(game_profile || "").trim() || "generic";
  const result = await api("/api/models/game-profile", {
    method: "POST",
    headers: { "Content-Type": "application/json" },
    body: JSON.stringify({ model_id, game_profile: nextGameProfile }),
  });
  if (state.data && Array.isArray(result.models)) {
    state.data.models = result.models;
  }
  renderModels({
    models: (result && result.models) || (state.data && state.data.models) || [],
    selected_model_id: (result && result.selected_model_id) || (state.config && state.config.model_id) || "",
  });
  return result;
}

async function updateModelRemoteFrameFormat(model_id, remote_frame_format) {
  const nextFormat = normalizeRemoteFrameFormat(remote_frame_format);
  const result = await api("/api/models/remote-frame-format", {
    method: "POST",
    headers: { "Content-Type": "application/json" },
    body: JSON.stringify({ model_id, remote_frame_format: nextFormat }),
  });
  if (state.data && Array.isArray(result.models)) {
    state.data.models = result.models;
  }
  renderModels({
    models: (result && result.models) || (state.data && state.data.models) || [],
    selected_model_id: (result && result.selected_model_id) || (state.config && state.config.model_id) || "",
  });
  return result;
}

async function updateModelRknnConcurrency(model_id, rknn_concurrency) {
  const nextConcurrency = Math.round(clamp(Number(rknn_concurrency) || 1, 1, 3));
  const result = await api("/api/models/rknn-concurrency", {
    method: "POST",
    headers: { "Content-Type": "application/json" },
    body: JSON.stringify({ model_id, rknn_concurrency: nextConcurrency }),
  });
  if (state.data && Array.isArray(result.models)) {
    state.data.models = result.models;
  }
  if (result && result.config) {
    state.config = result.config;
    if (state.data) {
      state.data.config = result.config;
    }
    populateForm(result.config);
  }
  renderModels({
    models: (result && result.models) || (state.data && state.data.models) || [],
    selected_model_id: (result && result.selected_model_id) || (state.config && state.config.model_id) || "",
  });
  return result;
}

async function updateModelHailoPipelineDepth(model_id, hailo_pipeline_depth) {
  const nextDepth = Math.round(clamp(Number(hailo_pipeline_depth) || 3, 1, 4));
  const result = await api("/api/models/hailo-pipeline-depth", {
    method: "POST",
    headers: { "Content-Type": "application/json" },
    body: JSON.stringify({ model_id, hailo_pipeline_depth: nextDepth }),
  });
  if (state.data && Array.isArray(result.models)) {
    state.data.models = result.models;
  }
  if (result && result.config) {
    state.config = result.config;
    if (state.data) {
      state.data.config = result.config;
    }
    populateForm(result.config);
  }
  renderModels({
    models: (result && result.models) || (state.data && state.data.models) || [],
    selected_model_id: (result && result.selected_model_id) || (state.config && state.config.model_id) || "",
  });
  return result;
}

async function deleteModel(model_id) {
  const model = findModelById(model_id);
  const isRemote = modelBackend(model) === "remote";
  await api(isRemote ? "/api/remote/delete" : "/api/models/delete", {
    method: "POST",
    headers: { "Content-Type": "application/json" },
    body: JSON.stringify({
      model_id,
      backend: isRemote ? "remote" : (model && model.backend) || undefined,
      remote_model_id: isRemote ? (model && model.remote_model_id) || "" : undefined,
      remote_host: isRemote ? (model && model.remote_host) || "" : undefined,
      remote_available: isRemote ? model && model.remote_available !== false : undefined,
      remote_missing: isRemote ? model && model.remote_available === false : undefined,
    }),
  });
  const payload = await api("/api/models");
  if (state.data) {
    state.data.models = payload.models || [];
  }
  renderModels(payload, payload.selected_model_id);
}

function renderModelClassNamesEditor(model) {
  const editor = $("modelClassNamesEditor");
  const subtitle = $("modelClassNamesSubtitle");
  if (!editor || !model) {
    return;
  }
  const count = modelClassEditCount(model);
  const names = modelClassNames(model);
  if (subtitle) {
    subtitle.textContent = `${modelFileName(model)} · ${count} 类`;
  }
  editor.innerHTML = "";
  for (let classId = 0; classId < count; classId += 1) {
    const row = document.createElement("label");
    row.className = "class-name-row";
    row.innerHTML = `
      <span>${classId}</span>
      <input class="model-class-name-input" type="text"
        data-class-id="${classId}"
        value="${escapeAttr(names[classId] || "")}"
        placeholder="${escapeAttr(`类别 ${classId}`)}">
    `;
    editor.appendChild(row);
  }
}

function setModelClassNamesDialogOpen(open, modelOrId = null) {
  const dialog = $("modelClassNamesDialog");
  if (!dialog) {
    return;
  }
  if (!open) {
    state.modelClassNamesEditModelId = "";
    dialog.hidden = true;
    setAnyModalOpen();
    return;
  }
  const model = typeof modelOrId === "string" ? findModelById(modelOrId) : modelOrId;
  if (!model || !model.id) {
    return;
  }
  state.modelClassNamesEditModelId = model.id;
  renderModelClassNamesEditor(model);
  dialog.hidden = false;
  setAnyModalOpen();
  const firstInput = dialog.querySelector(".model-class-name-input");
  if (firstInput) {
    firstInput.focus();
    firstInput.select();
  }
}

function collectModelClassNamesEditor() {
  const editor = $("modelClassNamesEditor");
  if (!editor) {
    return [];
  }
  const names = [];
  Array.from(editor.querySelectorAll(".model-class-name-input")).forEach((input) => {
    const classId = Number(input.dataset.classId);
    if (!Number.isInteger(classId) || classId < 0 || classId >= AIM_CLASS_MAX_COUNT) {
      return;
    }
    names[classId] = String(input.value || "").trim();
  });
  while (names.length > 0 && !names[names.length - 1]) {
    names.pop();
  }
  return names.map((name) => name || "");
}

function refreshClassNameDependentViews() {
  if (!state.configReady) {
    return;
  }
  renderAimProfiles(collectAimProfiles());
  renderAutoTriggerProfiles({ profiles: collectAutoTriggerProfiles() });
  renderAutoBackFlickClassPicker(currentAutoBackFlickClassConfig());
}

async function saveModelClassNames(modelId, classNames) {
  const result = await api("/api/models/class-names", {
    method: "POST",
    headers: { "Content-Type": "application/json" },
    body: JSON.stringify({ model_id: modelId, class_names: classNames }),
  });
  if (state.data && Array.isArray(result.models)) {
    state.data.models = result.models;
  }
  renderModels({
    models: (result && result.models) || (state.data && state.data.models) || [],
    selected_model_id: (result && result.selected_model_id) || (state.config && state.config.model_id) || "",
  });
  refreshClassNameDependentViews();
  return result;
}

function setModelImportDialogOpen(open, options = {}) {
  const dialog = $("modelImportDialog");
  if (!dialog) {
    return;
  }
  const force = options && options.force === true;
  if (!open && state.modelImportState === "importing" && !force) {
    return;
  }
  const form = $("modelImportForm");
  if (!open) {
    setModelImportBusy(false);
    setModelGameSuggestionOpen(false);
  }
  dialog.hidden = !open;
  setAnyModalOpen();
  if (open) {
    setModelImportBusy(false);
    updateModelImportMode();
    renderModelImportGameSuggestions();
    const fileInput = $("modelImportFile") || dialog.querySelector('input[name="file"]');
    if (fileInput) {
      fileInput.focus();
    }
  } else if (form) {
    form.reset();
    renderModelImportGameSuggestions();
    updateModelImportMode();
  }
}

function setPresetImportDialogOpen(open) {
  const dialog = $("presetImportDialog");
  if (!dialog) {
    return;
  }
  const form = $("presetImportForm");
  dialog.hidden = !open;
  setAnyModalOpen();
  if (open) {
    const fileInput = $("presetImportFile") || dialog.querySelector('input[name="file"]');
    if (fileInput) {
      fileInput.focus();
    }
  } else if (form) {
    form.reset();
  }
}

function setPresetRenameDialogOpen(open, name = "") {
  const dialog = $("presetRenameDialog");
  const form = $("presetRenameForm");
  const input = $("presetRenameName");
  if (!dialog) {
    return;
  }
  dialog.hidden = !open;
  dialog.dataset.oldName = open ? String(name || "").trim() : "";
  setAnyModalOpen();
  if (open) {
    if (input) {
      input.value = String(name || "").trim();
      window.setTimeout(() => {
        input.focus();
        input.select();
      }, 0);
    }
  } else if (form) {
    form.reset();
  }
}

async function loadPresetByName(name) {
  const presetName = String(name || "").trim();
  if (!presetName) {
    throw new Error("没有可加载的预设参数");
  }
  setSelectedPresetName(presetName);
  await api("/api/presets/load", {
    method: "POST",
    headers: { "Content-Type": "application/json" },
    body: JSON.stringify({ name: presetName }),
  });
  showToast(`已加载 ${presetName}`);
  await refreshAll();
}

function clearQueuedPresetAutosave(names) {
  const deleteNames = new Set((Array.isArray(names) ? names : [names]).map((name) => String(name || "").trim()).filter(Boolean));
  if (deleteNames.size === 0) {
    return;
  }
  if (deleteNames.has(state.presetAutoSaveName)) {
    state.presetAutoSaveQueued = false;
    state.presetAutoSaveName = "";
    state.presetAutoSaveConfig = null;
  }
}

async function deletePresetRequest(presetName) {
  return api("/api/presets", {
    method: "POST",
    headers: { "Content-Type": "application/json" },
    body: JSON.stringify({ name: presetName, action: "delete" }),
  });
}

async function renamePresetByName(oldName, newName) {
  const previousName = String(oldName || "").trim();
  const nextName = String(newName || "").trim();
  if (!previousName || !nextName) {
    throw new Error("请输入预设参数名称");
  }
  if (previousName === nextName) {
    setPresetRenameDialogOpen(false);
    return { name: nextName, presets: state.presetNames };
  }
  const deadline = Date.now() + 5000;
  while (state.presetAutoSaveInFlight || (state.presetAutoSaveQueued && state.presetAutoSaveName === previousName)) {
    if (!state.presetAutoSaveInFlight && state.presetAutoSaveQueued) {
      flushCurrentPresetAutosave();
    }
    if (Date.now() >= deadline) {
      throw new Error("预设正在保存，请稍后重试");
    }
    await sleep(40);
  }
  const result = await api("/api/presets", {
    method: "POST",
    headers: { "Content-Type": "application/json" },
    body: JSON.stringify({ name: previousName, new_name: nextName, action: "rename" }),
  });
  if (state.presetSelectedName === previousName) {
    state.presetSelectedName = String(result.name || nextName);
  }
  if (state.presetAutoSaveName === previousName) {
    state.presetAutoSaveName = String(result.name || nextName);
  }
  if (state.data && Array.isArray(result.models)) {
    state.data.models = result.models;
  }
  const presets = Array.isArray(result.presets) ? result.presets : (await api("/api/presets")).presets || [];
  renderPresets(presets);
  setPresetRenameDialogOpen(false);
  showToast(`已重命名为 ${result.name || nextName}`);
  return result;
}

async function refreshPresetLibraryAfterDelete(results = []) {
  renderPresets((await api("/api/presets")).presets || []);
  const unboundModels = results.some((result) =>
    result && Array.isArray(result.unbound_models) && result.unbound_models.length > 0
  );
  if (unboundModels) {
    const payload = await api("/api/models");
    if (state.data) {
      state.data.models = payload.models || [];
    }
    renderModels(payload, payload.selected_model_id);
  }
}

async function deletePresetByName(name, options = {}) {
  const presetName = String(name || "").trim();
  if (!presetName) {
    throw new Error("没有可删除的预设参数");
  }
  if (options.confirm !== false) {
    const confirmed = window.confirm(`确认删除预设「${presetName}」？\n删除后不可恢复；如果有模型绑定到它，也会自动清空绑定。`);
    if (!confirmed) {
      return;
    }
  }
  clearQueuedPresetAutosave(presetName);
  const result = await deletePresetRequest(presetName);
  showToast(`已删除 ${presetName}`);
  await refreshPresetLibraryAfterDelete([result]);
}

async function cleanupUnusedPresets() {
  const modelsPayload = await api("/api/models");
  if (state.data) {
    state.data.models = modelsPayload.models || [];
  }
  renderModels(modelsPayload, modelsPayload.selected_model_id);
  const unusedNames = unusedPresetNames();
  if (unusedNames.length === 0) {
    showToast("没有未使用预设");
    return;
  }
  const previewNames = unusedNames.slice(0, 8).join("、");
  const suffix = unusedNames.length > 8 ? ` 等 ${unusedNames.length} 个` : "";
  const confirmed = window.confirm(`确认清理 ${unusedNames.length} 个未绑定模型的预设？\n${previewNames}${suffix}\n删除后不可恢复。`);
  if (!confirmed) {
    return;
  }
  clearQueuedPresetAutosave(unusedNames);
  const results = [];
  for (const presetName of unusedNames) {
    results.push(await deletePresetRequest(presetName));
  }
  showToast(`已清理 ${unusedNames.length} 个未使用预设`);
  await refreshPresetLibraryAfterDelete(results);
}

function bindEvents() {
  const preview = $("previewImage");
  if (preview) {
    preview.addEventListener("load", () => {
      preview.style.visibility = "visible";
      updateAimRangeOverlay();
    });
    preview.addEventListener("error", () => {
      preview.style.visibility = "hidden";
    });
  }
  window.addEventListener("resize", updateAimRangeOverlay);
  document.addEventListener("click", (event) => {
    closeClassOffsetPopovers();
    const copyButton = event.target && event.target.closest
      ? event.target.closest("[data-copy-lan-url]")
      : null;
    if (!copyButton || copyButton.disabled) {
      return;
    }
    runUiAction(() => copyLanAccessUrl(copyButton));
  });

  on("acceptDisclaimerButton", "click", () => hideDisclaimer(false));
  on("hideDisclaimerButton", "click", () => hideDisclaimer(true));
  on("ackAnnouncementButton", "click", acknowledgeAnnouncement);
  on("closeAnnouncementButton", "click", acknowledgeAnnouncement);

  on("startButton", "click", () => runUiAction(async () => {
    const runtime = (state.data && state.data.state) || {};
    const shouldStop = runtime.running || runtime.status === "starting" || runtime.status === "reconnecting";
    if (!shouldStop && needsLicenseRecovery(state.data)) {
      await refreshLicenseStatus();
      await refreshAll();
    }
    const path = shouldStop ? "/api/control/stop" : "/api/control/start";
    const nextRuntime = await api(path, { method: "POST" });
    if (state.data) {
      state.data.state = {
        ...runtime,
        ...nextRuntime,
        license: nextRuntime.license || runtime.license,
        core: nextRuntime.core || runtime.core,
      };
      if (shouldStop) {
        state.data.state.running = false;
        state.data.state.status = "stopped";
      }
      renderRuntime(state.data);
    }
  }));

  on("rebootSystemButton", "click", () => runUiAction(async () => {
    if (!window.confirm("确认重启设备？")) {
      return;
    }
    await api("/api/system/reboot", { method: "POST" });
    showToast("重启指令已发送");
  }));

  on("poweroffSystemButton", "click", () => runUiAction(async () => {
    if (!window.confirm("确认关机？")) {
      return;
    }
    await api("/api/system/poweroff", { method: "POST" });
    showToast("关机指令已发送");
  }));

  const lanHostnameInput = $("lanHostnameInput");
  if (lanHostnameInput) {
    lanHostnameInput.addEventListener("input", updateNetworkAccessButtonState);
    lanHostnameInput.addEventListener("keydown", (event) => {
      if (event.key === "Enter") {
        event.preventDefault();
        runUiAction(applyNetworkAccessSettings);
      }
    });
  }
  const webPortInput = $("webPortInput");
  if (webPortInput) {
    webPortInput.addEventListener("input", updateNetworkAccessButtonState);
    webPortInput.addEventListener("keydown", (event) => {
      if (event.key === "Enter") {
        event.preventDefault();
        runUiAction(applyNetworkAccessSettings);
      }
    });
  }
  on("applyNetworkAccessButton", "click", () => runUiAction(applyNetworkAccessSettings));

  const lanBlockSelect = $("lanBlockDeviceSelect");
  if (lanBlockSelect) {
    lanBlockSelect.addEventListener("change", () => {
      const input = $("lanBlockIpInput");
      if (input && lanBlockSelect.value) {
        input.value = lanBlockSelect.value;
      }
      updateLanBlockButtonState();
    });
  }
  const lanBlockIpInput = $("lanBlockIpInput");
  if (lanBlockIpInput) {
    lanBlockIpInput.addEventListener("input", updateLanBlockButtonState);
    lanBlockIpInput.addEventListener("keydown", (event) => {
      if (event.key === "Enter") {
        event.preventDefault();
        runUiAction(applyLanBlock);
      }
    });
  }
  on("scanLanDevicesButton", "click", () => runUiAction(scanLanDevices));
  on("applyLanBlockButton", "click", () => runUiAction(applyLanBlock));
  on("clearLanBlockButton", "click", () => runUiAction(clearLanBlock));

  const wifiSelect = $("wifiNetworkSelect");
  if (wifiSelect) {
    wifiSelect.addEventListener("change", () => {
      state.wifiSelectedSsid = wifiSelect.value;
      renderWifiPanels(state.wifi);
    });
  }

  const wifiPasswordInput = $("wifiPassword");
  if (wifiPasswordInput) {
    wifiPasswordInput.addEventListener("input", updateWifiConnectButtonState);
  }

  const wifiClientModeButton = $("wifiClientModeButton");
  if (wifiClientModeButton) {
    wifiClientModeButton.addEventListener("click", () => setWifiModePanel("client"));
  }

  const wifiApModeButton = $("wifiApModeButton");
  if (wifiApModeButton) {
    wifiApModeButton.addEventListener("click", () => setWifiModePanel("ap"));
  }

  ["wifiApSsid", "wifiApPassword"].forEach((id) => {
    const input = $(id);
    if (input) {
      input.addEventListener("input", () => {
        state.wifiApCredentialsInitialized = true;
        updateWifiApButtonState();
      });
    }
  });

  on("wifiScanButton", "click", () => runUiAction(async () => {
    await refreshWifi(true);
    showToast("Wi-Fi 列表已刷新");
  }));

  on("wifiConnectButton", "click", () => runUiAction(() => connectWifiFrom("wifiNetworkSelect", "wifiPassword")));
  on("wifiFallbackButton", "click", () => runUiAction(fallbackWifi));
  on("wifiApApplyButton", "click", () => runUiAction(applyWifiApHotspot));
  on("wifiClientActivateButton", "click", () => runUiAction(activateWifiClientMode));

  on("resetOverviewDefaultsButton", "click", resetOverviewDefaults);
  on("resetControllerDefaultsButton", "click", resetCurrentMovementSectionDefaults);
  on("resetAssistDefaultsButton", "click", resetCurrentAssistSectionDefaults);

  on("closeAutoBackFlickClassDialogButton", "click", () => setAutoBackFlickClassDialogOpen(false));
  on("cancelAutoBackFlickClassDialogButton", "click", () => setAutoBackFlickClassDialogOpen(false));
  on("autoBackFlickClassRandomDirection", "change", () => {
    enforceAutoBackFlickDirectionExclusion("autoBackFlickClassRandomDirection");
  });
  on("autoBackFlickClassDodgeAway", "change", () => {
    enforceAutoBackFlickDirectionExclusion("autoBackFlickClassDodgeAway");
  });
  const autoBackFlickClassForm = $("autoBackFlickClassForm");
  if (autoBackFlickClassForm) {
    autoBackFlickClassForm.addEventListener("submit", (event) => {
      event.preventDefault();
      saveAutoBackFlickClassDialog();
    });
  }
  const autoBackFlickClassDialog = $("autoBackFlickClassDialog");
  if (autoBackFlickClassDialog) {
    autoBackFlickClassDialog.addEventListener("click", (event) => {
      if (event.target === autoBackFlickClassDialog) setAutoBackFlickClassDialogOpen(false);
    });
  }

  on("downloadUsbDiagnosticsButton", "click", () => runUiAction(downloadUsbDiagnostics));

  on("recordAimTraceButton", "click", () => runUiAction(async () => {
    const button = $("recordAimTraceButton");
    try {
      if (button) {
        button.disabled = true;
      }
      setAimTraceStatus("记录中...");
      const result = await api("/api/diagnostics/aim-trace", {
        method: "POST",
        headers: { "Content-Type": "application/json" },
        body: JSON.stringify({ duration_sec: 10 }),
      });
      const durationMs = Math.max(1000, Math.round(((result && result.duration_sec) || 10) * 1000));
      showToast("移动日志已开始");
      window.setTimeout(() => {
        setAimTraceStatus(result && result.filename ? `已生成 ${result.filename}` : "记录完成");
        if (button) {
          button.disabled = false;
        }
      }, durationMs + 500);
    } catch (error) {
      if (button) {
        button.disabled = false;
      }
      setAimTraceStatus("记录失败");
      throw error;
    }
  }));

  on("openModelImportButton", "click", () => setModelImportDialogOpen(true));
  on("openPresetImportButton", "click", () => setPresetImportDialogOpen(true));
  on("cleanupUnusedPresetsButton", "click", () => runUiAction(cleanupUnusedPresets));
  on("openRemoteConnectButton", "click", () => setRemoteConnectDialogOpen(true));
  on("copyModelDeviceCodeButton", "click", () => runUiAction(copyModelDeviceCode));
  on("closeModelImportButton", "click", () => setModelImportDialogOpen(false));
  on("cancelModelImportButton", "click", () => setModelImportDialogOpen(false));
  on("closeRemoteConnectButton", "click", () => setRemoteConnectDialogOpen(false));
  on("cancelRemoteConnectButton", "click", () => setRemoteConnectDialogOpen(false));
  on("closeModelClassNamesButton", "click", () => setModelClassNamesDialogOpen(false));
  on("cancelModelClassNamesButton", "click", () => setModelClassNamesDialogOpen(false));
  on("closePresetImportButton", "click", () => setPresetImportDialogOpen(false));
  on("cancelPresetImportButton", "click", () => setPresetImportDialogOpen(false));
  on("closePresetRenameButton", "click", () => setPresetRenameDialogOpen(false));
  on("cancelPresetRenameButton", "click", () => setPresetRenameDialogOpen(false));
  on("addDisplayEdidModeButton", "click", () => setDisplayEdidModeDialogOpen(true));
  on("closeDisplayEdidModeButton", "click", () => setDisplayEdidModeDialogOpen(false));
  on("cancelDisplayEdidModeButton", "click", () => setDisplayEdidModeDialogOpen(false));
  const modelImportDialog = $("modelImportDialog");
  if (modelImportDialog) {
    modelImportDialog.addEventListener("click", (event) => {
      if (event.target === modelImportDialog) {
        setModelImportDialogOpen(false);
      }
    });
  }
  const remoteConnectDialog = $("remoteConnectDialog");
  if (remoteConnectDialog) {
    remoteConnectDialog.addEventListener("click", (event) => {
      if (event.target === remoteConnectDialog) {
        setRemoteConnectDialogOpen(false);
      }
    });
  }
  const modelClassNamesDialog = $("modelClassNamesDialog");
  if (modelClassNamesDialog) {
    modelClassNamesDialog.addEventListener("click", (event) => {
      if (event.target === modelClassNamesDialog) {
        setModelClassNamesDialogOpen(false);
      }
    });
  }
  const presetImportDialog = $("presetImportDialog");
  if (presetImportDialog) {
    presetImportDialog.addEventListener("click", (event) => {
      if (event.target === presetImportDialog) {
        setPresetImportDialogOpen(false);
      }
    });
  }
  const presetRenameDialog = $("presetRenameDialog");
  if (presetRenameDialog) {
    presetRenameDialog.addEventListener("click", (event) => {
      if (event.target === presetRenameDialog) {
        setPresetRenameDialogOpen(false);
      }
    });
  }
  const displayEdidModeDialog = $("displayEdidModeDialog");
  if (displayEdidModeDialog) {
    displayEdidModeDialog.addEventListener("click", (event) => {
      if (event.target === displayEdidModeDialog) {
        setDisplayEdidModeDialogOpen(false);
      }
    });
  }
  const displayEdidModeForm = $("displayEdidModeForm");
  if (displayEdidModeForm) {
    displayEdidModeForm.addEventListener("submit", (event) => {
      event.preventDefault();
      saveDisplayEdidModeFromDialog();
    });
  }
  ["displayEdidModeWidth", "displayEdidModeHeight"].forEach((id) => {
    const input = $(id);
    if (input) {
      input.addEventListener("input", updateDisplayEdidModeRefreshFromSize);
    }
  });
  const displayEdidModeRefresh = $("displayEdidModeRefresh");
  if (displayEdidModeRefresh) {
    displayEdidModeRefresh.addEventListener("input", () => validateDisplayEdidModeInputs());
  }
  document.addEventListener("keydown", (event) => {
    if (event.key === "Escape" && autoBackFlickClassDialog && !autoBackFlickClassDialog.hidden) {
      setAutoBackFlickClassDialogOpen(false);
    } else if (event.key === "Escape" && physicalButtonBlockDialog && !physicalButtonBlockDialog.hidden) {
      setPhysicalButtonBlockDialogOpen(false);
    } else if (event.key === "Escape" && modelImportDialog && !modelImportDialog.hidden) {
      setModelImportDialogOpen(false);
    } else if (event.key === "Escape" && remoteConnectDialog && !remoteConnectDialog.hidden) {
      setRemoteConnectDialogOpen(false);
    } else if (event.key === "Escape" && modelClassNamesDialog && !modelClassNamesDialog.hidden) {
      setModelClassNamesDialogOpen(false);
    } else if (event.key === "Escape" && presetImportDialog && !presetImportDialog.hidden) {
      setPresetImportDialogOpen(false);
    } else if (event.key === "Escape" && presetRenameDialog && !presetRenameDialog.hidden) {
      setPresetRenameDialogOpen(false);
    } else if (event.key === "Escape" && displayEdidModeDialog && !displayEdidModeDialog.hidden) {
      setDisplayEdidModeDialogOpen(false);
    }
  });
  document.addEventListener("pointerdown", (event) => {
    const modelCombo = $("modelGameCombobox");
    if (state.modelGameSuggestionOpen && modelCombo && !modelCombo.contains(event.target)) {
      setModelGameSuggestionOpen(false);
    }
    const presetCombo = $("presetCombobox");
    if (state.presetSuggestionOpen && presetCombo && !presetCombo.contains(event.target)) {
      setPresetSuggestionOpen(false);
    }
    const modelPresetCombo = event.target.closest(".model-preset-combobox");
    if (state.modelPresetBindingOpenId && !modelPresetCombo) {
      setModelPresetBindingOpen("");
    }
    const modelGameBinding = event.target.closest(".model-game-binding");
    if (state.modelGameBindingOpenId && !modelGameBinding) {
      setModelGameBindingOpen("");
    }
    const modelRemoteFrameBinding = event.target.closest(".model-remote-frame-binding");
    if (state.modelRemoteFrameBindingOpenId && !modelRemoteFrameBinding) {
      setModelRemoteFrameBindingOpen("");
    }
    const modelRknnConcurrencyBinding = event.target.closest(".model-rknn-concurrency-binding");
    if (state.modelRknnConcurrencyBindingOpenId && !modelRknnConcurrencyBinding) {
      setModelRknnConcurrencyBindingOpen("");
    }
    const modelHailoPipelineBinding = event.target.closest(".model-hailo-pipeline-binding");
    if (state.modelHailoPipelineBindingOpenId && !modelHailoPipelineBinding) {
      setModelHailoPipelineBindingOpen("");
    }
  });

  const modelGameInput = $("modelImportGameProfile");
  const modelGameToggle = $("modelGameSuggestionToggle");
  const modelGameList = $("modelGameSuggestionList");
  if (modelGameInput) {
    modelGameInput.addEventListener("focus", () => setModelGameSuggestionOpen(true));
    modelGameInput.addEventListener("click", () => setModelGameSuggestionOpen(true));
    modelGameInput.addEventListener("input", () => {
      renderModelImportGameSuggestions();
      setModelGameSuggestionOpen(true);
    });
    modelGameInput.addEventListener("keydown", (event) => {
      if (event.key === "ArrowDown") {
        event.preventDefault();
        setModelGameSuggestionOpen(true);
        const firstOption = modelGameList && modelGameList.querySelector("[data-model-game]");
        if (firstOption) {
          firstOption.focus();
        }
      } else if (event.key === "Escape") {
        event.stopPropagation();
        setModelGameSuggestionOpen(false);
      }
    });
  }
  if (modelGameToggle) {
    modelGameToggle.addEventListener("click", (event) => {
      event.preventDefault();
      event.stopPropagation();
      renderModelImportGameSuggestions();
      setModelGameSuggestionOpen(!state.modelGameSuggestionOpen);
      if (modelGameInput) {
        modelGameInput.focus();
      }
    });
  }
  if (modelGameList) {
    modelGameList.addEventListener("click", (event) => {
      const option = event.target.closest("[data-model-game]");
      if (!option) {
        return;
      }
      event.preventDefault();
      if (modelGameInput) {
        modelGameInput.value = option.dataset.modelGame || "";
        modelGameInput.dispatchEvent(new Event("input", { bubbles: true }));
        modelGameInput.focus();
      }
      setModelGameSuggestionOpen(false);
    });
    modelGameList.addEventListener("keydown", (event) => {
      if (event.key === "Escape") {
        event.stopPropagation();
        setModelGameSuggestionOpen(false);
        if (modelGameInput) {
          modelGameInput.focus();
        }
      }
    });
  }

  const presetButton = $("presetSelectButton");
  const presetList = $("presetSuggestionList");
  if (presetButton) {
    presetButton.addEventListener("click", (event) => {
      event.preventDefault();
      event.stopPropagation();
      setPresetSuggestionOpen(!state.presetSuggestionOpen);
    });
    presetButton.addEventListener("keydown", (event) => {
      if (event.key === "ArrowDown") {
        event.preventDefault();
        setPresetSuggestionOpen(true);
        const firstOption = presetList && presetList.querySelector("[data-preset-name]");
        if (firstOption) {
          firstOption.focus();
        }
      } else if (event.key === "Escape") {
        event.stopPropagation();
        setPresetSuggestionOpen(false);
      }
    });
  }
  if (presetList) {
    presetList.addEventListener("click", (event) => {
      const option = event.target.closest("[data-preset-name]");
      if (!option) {
        return;
      }
      event.preventDefault();
      setSelectedPresetName(option.dataset.presetName || "");
      setPresetSuggestionOpen(false);
      if (presetButton) {
        presetButton.focus();
      }
    });
    presetList.addEventListener("keydown", (event) => {
      if (event.key === "Escape") {
        event.stopPropagation();
        setPresetSuggestionOpen(false);
        if (presetButton) {
          presetButton.focus();
        }
      }
    });
  }

  const modelImportForm = $("modelImportForm");
  if (modelImportForm) {
    Array.from(modelImportForm.querySelectorAll('input[name="model_type"]')).forEach((radio) => {
      radio.addEventListener("change", updateModelImportMode);
    });
    updateModelImportMode();
    modelImportForm.addEventListener("submit", (event) => {
      event.preventDefault();
      const form = event.currentTarget;
      runUiAction(async () => {
        if (state.modelImportState === "importing") {
          return;
        }
        const gameProfileInput = $("modelImportGameProfile");
        if (gameProfileInput) {
          gameProfileInput.value = gameProfileInput.value.trim();
        }
        const formData = new FormData(form);
        const importType = currentModelImportType(form);
        setModelImportStatus("importing", modelImportProgressMessage(importType));
        try {
          await api("/api/models/import", {
            method: "POST",
            body: formData,
          });
          if (importType === "remote_onnx") {
            state.modelBackendFilter = "remote";
            await api("/api/remote/models");
          }
          const payload = await api("/api/state");
          applyFullState(payload);
          form.reset();
          setModelImportStatus("success", "导入成功");
          showToast(importType === "remote_onnx" ? "远端 ONNX 已转换并导入" : importType === "onnx" ? "ONNX 已转换并导入" : importType === "hef" ? "HEF 模型导入成功" : "模型导入成功");
          state.modelImportCloseTimer = window.setTimeout(() => {
            state.modelImportCloseTimer = null;
            setModelImportDialogOpen(false, { force: true });
          }, 700);
        } catch (error) {
          setModelImportStatus("error", error.message || String(error));
        }
      });
    });
  }

  const remoteConnectForm = $("remoteConnectForm");
  if (remoteConnectForm) {
    remoteConnectForm.addEventListener("submit", (event) => {
      event.preventDefault();
      runUiAction(async () => {
        const input = $("remoteHostInput");
        const host = input ? input.value.trim() : "";
        setRemoteConnectBusy(true);
        setRemoteConnectError("");
        try {
          await connectRemoteHost(host);
          state.modelBackendFilter = "remote";
          rerenderCurrentModels();
          setRemoteConnectDialogOpen(false);
          showToast("远端服务已连接");
        } catch (error) {
          const message = error.message || "连接失败，请重新输入局域网IP或检查Windows端程序是否启动";
          setRemoteConnectError(message);
          throw error;
        } finally {
          setRemoteConnectBusy(false);
        }
      });
    });
  }

  const modelClassNamesForm = $("modelClassNamesForm");
  if (modelClassNamesForm) {
    modelClassNamesForm.addEventListener("submit", (event) => {
      event.preventDefault();
      runUiAction(async () => {
        const modelId = state.modelClassNamesEditModelId;
        if (!modelId) {
          throw new Error("请选择模型");
        }
        await saveModelClassNames(modelId, collectModelClassNamesEditor());
        setModelClassNamesDialogOpen(false);
        showToast("类别名称已保存");
      });
    });
  }

  on("savePresetButton", "click", () => runUiAction(async () => {
    const name = $("presetName").value.trim();
    if (!name) {
      throw new Error("请输入预设参数名称");
    }
    await applyConfigNow();
    const result = await api("/api/presets", {
      method: "POST",
      headers: { "Content-Type": "application/json" },
      body: JSON.stringify({ name, config: collectConfig() }),
    });
    showToast("预设参数已保存");
    renderPresets((await api("/api/presets")).presets || []);
    setSelectedPresetName((result && result.name) || name);
  }));

  on("loadPresetButton", "click", () => runUiAction(async () => {
    const name = getSelectedPresetName();
    await loadPresetByName(name);
  }));

  on("deletePresetButton", "click", () => runUiAction(async () => {
    const name = getSelectedPresetName();
    await deletePresetByName(name);
  }));

  const presetImportForm = $("presetImportForm");
  if (presetImportForm) {
    presetImportForm.addEventListener("submit", (event) => {
      event.preventDefault();
      const form = event.currentTarget;
      runUiAction(async () => {
        const formData = new FormData(form);
        const file = formData.get("file");
        const requestedName = String(formData.get("name") || "").trim();
        if (!requestedName && file && typeof file.name === "string") {
          formData.set("name", file.name.replace(/\.json$/i, ""));
        }
        const result = await api("/api/presets/import", {
          method: "POST",
          body: formData,
        });
        form.reset();
        setPresetImportDialogOpen(false);
        showToast("预设参数导入成功");
        renderPresets((await api("/api/presets")).presets || []);
        if (result && result.name) {
          setSelectedPresetName(result.name);
        }
      });
    });
  }

  const presetRenameForm = $("presetRenameForm");
  if (presetRenameForm) {
    presetRenameForm.addEventListener("submit", (event) => {
      event.preventDefault();
      const dialog = $("presetRenameDialog");
      const oldName = String(dialog && dialog.dataset.oldName || "").trim();
      const newName = String($("presetRenameName") && $("presetRenameName").value || "").trim();
      runUiAction(() => renamePresetByName(oldName, newName));
    });
  }

  on("refreshHardwareButton", "click", () => runUiAction(async () => {
    await loadHardware();
    showToast("硬件信息已刷新");
  }));

  on("refreshHailoButton", "click", () => runUiAction(async () => {
    await refreshHailoStatus();
    scheduleHailoStatusPolling();
    showToast("Hailo-8 状态已刷新");
  }));

  on("installHailoButton", "click", () => runUiAction(async () => {
    await installHailoDependencies();
    showToast("Hailo-8 安装任务已启动");
  }));

  on("testMouseCircleButton", "click", () => runUiAction(testMouseOutputCircle));

  on("saveKmboxButton", "click", () => runUiAction(async () => {
    const config = validateKmboxConfig();
    if (config.enabled) {
      setCheckbox("catnet_enabled", false);
      setCheckbox("makcu_enabled", false);
      setCheckbox("ferrum_enabled", false);
      setCheckbox("kmboxb_enabled", false);
      updateCatnetFormUi();
      updateMakcuFormUi();
      updateFerrumFormUi();
      updateKmboxbFormUi();
    }
    setRadioValue("mouse_output_mode", config.enabled ? "kmboxnet" : "passthrough");
    await applyConfigNow();
    showToast(config.enabled ? "键鼠盒子配置已保存" : "键鼠盒子已关闭");
  }));

  on("saveCatnetButton", "click", () => runUiAction(async () => {
    const config = validateCatnetConfig();
    if (config.enabled) {
      setCheckbox("kmbox_enabled", false);
      setCheckbox("makcu_enabled", false);
      setCheckbox("ferrum_enabled", false);
      setCheckbox("kmboxb_enabled", false);
      updateKmboxFormUi();
      updateMakcuFormUi();
      updateFerrumFormUi();
      updateKmboxbFormUi();
    }
    setRadioValue("mouse_output_mode", config.enabled ? "catnet" : "passthrough");
    await applyConfigNow();
    showToast(config.enabled ? "CatNet 配置已保存" : "CatNet 已关闭");
  }));

  on("refreshMakcuDevicesButton", "click", () => runUiAction(async () => {
    await refreshMakcuDevices();
  }));

  on("saveMakcuButton", "click", () => runUiAction(async () => {
    const config = validateMakcuConfig();
    if (config.enabled) {
      setCheckbox("kmbox_enabled", false);
      setCheckbox("catnet_enabled", false);
      setCheckbox("ferrum_enabled", false);
      setCheckbox("kmboxb_enabled", false);
      updateKmboxFormUi();
      updateCatnetFormUi();
      updateFerrumFormUi();
      updateKmboxbFormUi();
    }
    setRadioValue("mouse_output_mode", config.enabled ? "makcu" : "passthrough");
    await applyConfigNow();
    showToast(config.enabled ? "MAKCU 配置已保存" : "MAKCU 已关闭");
  }));

  on("refreshFerrumDevicesButton", "click", () => runUiAction(async () => {
    await refreshFerrumDevices();
  }));

  on("saveFerrumButton", "click", () => runUiAction(async () => {
    const config = validateFerrumConfig();
    if (config.enabled) {
      setCheckbox("kmbox_enabled", false);
      setCheckbox("catnet_enabled", false);
      setCheckbox("makcu_enabled", false);
      setCheckbox("kmboxb_enabled", false);
      updateKmboxFormUi();
      updateCatnetFormUi();
      updateMakcuFormUi();
      updateKmboxbFormUi();
    }
    setRadioValue("mouse_output_mode", config.enabled ? "ferrum" : "passthrough");
    await applyConfigNow();
    showToast(config.enabled ? "Ferrum Legacy 配置已保存" : "Ferrum Legacy 已关闭");
  }));

  on("refreshKmboxbDevicesButton", "click", () => runUiAction(async () => {
    await refreshKmboxbDevices();
  }));

  on("saveKmboxbButton", "click", () => runUiAction(async () => {
    const config = validateKmboxbConfig();
    if (config.enabled) {
      setCheckbox("kmbox_enabled", false);
      setCheckbox("catnet_enabled", false);
      setCheckbox("makcu_enabled", false);
      setCheckbox("ferrum_enabled", false);
      updateKmboxFormUi();
      updateCatnetFormUi();
      updateMakcuFormUi();
      updateFerrumFormUi();
    }
    setRadioValue("mouse_output_mode", config.enabled ? "kmboxb" : "passthrough");
    await applyConfigNow();
    showToast(config.enabled ? "kmbox B+ 配置已保存" : "kmbox B+ 已关闭");
  }));

  [
    "kmbox_enabled",
    "kmbox_encrypted",
    "kmbox_port",
    "kmbox_monitor_port",
    "kmbox_timeout_ms",
  ].forEach((id) => {
    const input = $(id);
    if (!input) {
      return;
    }
    const eventName = input.type === "checkbox" || input.tagName === "SELECT" ? "change" : "input";
    input.addEventListener(eventName, () => {
      if (id === "kmbox_enabled") {
        if (input.checked) {
          setCheckbox("catnet_enabled", false);
          setCheckbox("makcu_enabled", false);
          setCheckbox("ferrum_enabled", false);
          setCheckbox("kmboxb_enabled", false);
          updateCatnetFormUi();
          updateMakcuFormUi();
          updateFerrumFormUi();
          updateKmboxbFormUi();
        } else {
          setCheckbox("catnet_enabled", false);
          setCheckbox("makcu_enabled", false);
          setCheckbox("ferrum_enabled", false);
          setCheckbox("kmboxb_enabled", false);
          setRadioValue("mouse_output_mode", "passthrough");
          updateCatnetFormUi();
          updateMakcuFormUi();
          updateFerrumFormUi();
          updateKmboxbFormUi();
          updateKmboxFormUi();
          runUiAction(async () => {
            await applyConfigNow();
            showToast("键鼠盒子已关闭，已切回默认监听与移动");
          });
          return;
        }
      }
      updateKmboxFormUi();
    });
  });

  [
    "catnet_enabled",
    "catnet_port",
    "catnet_monitor_port",
    "catnet_timeout_ms",
  ].forEach((id) => {
    const input = $(id);
    if (!input) {
      return;
    }
    const eventName = input.type === "checkbox" || input.tagName === "SELECT" ? "change" : "input";
    input.addEventListener(eventName, () => {
      if (id === "catnet_enabled") {
        if (input.checked) {
          setCheckbox("kmbox_enabled", false);
          setCheckbox("makcu_enabled", false);
          setCheckbox("ferrum_enabled", false);
          setCheckbox("kmboxb_enabled", false);
          updateKmboxFormUi();
          updateMakcuFormUi();
          updateFerrumFormUi();
          updateKmboxbFormUi();
        } else {
          setCheckbox("kmbox_enabled", false);
          setCheckbox("makcu_enabled", false);
          setCheckbox("ferrum_enabled", false);
          setCheckbox("kmboxb_enabled", false);
          setRadioValue("mouse_output_mode", "passthrough");
          updateKmboxFormUi();
          updateMakcuFormUi();
          updateFerrumFormUi();
          updateKmboxbFormUi();
          updateCatnetFormUi();
          runUiAction(async () => {
            await applyConfigNow();
            showToast("CatNet 已关闭，已切回默认监听与移动");
          });
          return;
        }
      }
      updateCatnetFormUi();
    });
  });

  [
    "makcu_enabled",
    "makcu_high_speed",
    "makcu_port",
  ].forEach((id) => {
    const input = $(id);
    if (!input) {
      return;
    }
    const eventName = input.type === "checkbox" || input.tagName === "SELECT" ? "change" : "input";
    input.addEventListener(eventName, () => {
      if (id === "makcu_enabled") {
        if (input.checked) {
          setCheckbox("kmbox_enabled", false);
          setCheckbox("catnet_enabled", false);
          setCheckbox("ferrum_enabled", false);
          setCheckbox("kmboxb_enabled", false);
          updateKmboxFormUi();
          updateCatnetFormUi();
          updateFerrumFormUi();
          updateKmboxbFormUi();
        } else {
          setCheckbox("kmbox_enabled", false);
          setCheckbox("catnet_enabled", false);
          setCheckbox("ferrum_enabled", false);
          setCheckbox("kmboxb_enabled", false);
          setRadioValue("mouse_output_mode", "passthrough");
          updateKmboxFormUi();
          updateCatnetFormUi();
          updateFerrumFormUi();
          updateKmboxbFormUi();
          updateMakcuFormUi();
          runUiAction(async () => {
            await applyConfigNow();
            showToast("MAKCU 已关闭，已切回默认监听与移动");
          });
          return;
        }
      }
      updateMakcuFormUi();
    });
  });

  [
    "ferrum_enabled",
    "ferrum_port",
  ].forEach((id) => {
    const input = $(id);
    if (!input) {
      return;
    }
    const eventName = input.type === "checkbox" || input.tagName === "SELECT" ? "change" : "input";
    input.addEventListener(eventName, () => {
      if (id === "ferrum_enabled") {
        if (input.checked) {
          setCheckbox("kmbox_enabled", false);
          setCheckbox("catnet_enabled", false);
          setCheckbox("makcu_enabled", false);
          setCheckbox("kmboxb_enabled", false);
          updateKmboxFormUi();
          updateCatnetFormUi();
          updateMakcuFormUi();
          updateKmboxbFormUi();
        } else {
          setCheckbox("kmbox_enabled", false);
          setCheckbox("catnet_enabled", false);
          setCheckbox("makcu_enabled", false);
          setCheckbox("kmboxb_enabled", false);
          setRadioValue("mouse_output_mode", "passthrough");
          updateKmboxFormUi();
          updateCatnetFormUi();
          updateMakcuFormUi();
          updateKmboxbFormUi();
          updateFerrumFormUi();
          runUiAction(async () => {
            await applyConfigNow();
            showToast("Ferrum Legacy 已关闭，已切回默认监听与移动");
          });
          return;
        }
      }
      updateFerrumFormUi();
    });
  });

  [
    "kmboxb_enabled",
    "kmboxb_port",
  ].forEach((id) => {
    const input = $(id);
    if (!input) {
      return;
    }
    const eventName = input.type === "checkbox" || input.tagName === "SELECT" ? "change" : "input";
    input.addEventListener(eventName, () => {
      if (id === "kmboxb_enabled") {
        if (input.checked) {
          setCheckbox("kmbox_enabled", false);
          setCheckbox("catnet_enabled", false);
          setCheckbox("makcu_enabled", false);
          setCheckbox("ferrum_enabled", false);
          updateKmboxFormUi();
          updateCatnetFormUi();
          updateMakcuFormUi();
          updateFerrumFormUi();
        } else {
          setCheckbox("kmbox_enabled", false);
          setCheckbox("catnet_enabled", false);
          setCheckbox("makcu_enabled", false);
          setCheckbox("ferrum_enabled", false);
          setRadioValue("mouse_output_mode", "passthrough");
          updateKmboxFormUi();
          updateCatnetFormUi();
          updateMakcuFormUi();
          updateFerrumFormUi();
          updateKmboxbFormUi();
          runUiAction(async () => {
            await applyConfigNow();
            showToast("kmbox B+ 已关闭，已切回默认监听与移动");
          });
          return;
        }
      }
      updateKmboxbFormUi();
    });
  });

  on("randomDisplayHardwareButton", "click", () => {
    if (getCheckbox("display_loopout_enabled")) {
      showToast("环出模式使用真实显示器身份，不能随机");
      return;
    }
    randomizeDisplayHardware();
    showToast("显示器身份已随机");
  });

  const displayLoopoutToggle = $("display_loopout_enabled");
  if (displayLoopoutToggle) {
    displayLoopoutToggle.addEventListener("change", () => {
      if (!getCheckbox("display_loopout_enabled")) {
        setCheckbox("display_loopout_overlay_enabled", false);
      }
      refreshDisplayNativeModeOptions({
        ...(state.displayHardwarePayload || {}),
        config: {
          ...((state.displayHardwarePayload && state.displayHardwarePayload.config) || {}),
          native_mode: getString("display_native_mode"),
          loopout_enabled: getCheckbox("display_loopout_enabled"),
        },
      });
      // door display removed
      setValidation("displayHardwareValidation", DISPLAY_IDENTITY_FIELD_IDS, []);
    });
  }

  on("displayLoopoutOverlaySettingsButton", "click", (event) => {
    event.stopPropagation();
    const popover = $("displayLoopoutOverlaySettingsPopover");
    setLoopoutOverlaySettingsOpen(!!popover && popover.hidden);
  });
  on("displayLoopoutOverlaySettingsClose", "click", () => setLoopoutOverlaySettingsOpen(false));
  on("displayLoopoutOverlayCancel", "click", () => setLoopoutOverlaySettingsOpen(false));
  on("displayLoopoutOverlaySelectAll", "click", () => {
    document.querySelectorAll("#displayLoopoutOverlayClasses input[data-overlay-class-id]").forEach((input) => {
      input.checked = true;
    });
  });
  on("displayLoopoutOverlaySelectNone", "click", () => {
    document.querySelectorAll("#displayLoopoutOverlayClasses input[data-overlay-class-id]").forEach((input) => {
      input.checked = false;
    });
  });
  on("display_loopout_overlay_use_class_colors", "change", updateLoopoutOverlayColorUi);
  on("displayLoopoutOverlayApply", "click", () => runUiAction(async () => {
    const loopoutOverlay = collectLoopoutOverlaySettings();
    const result = await api("/api/config", {
      method: "PUT",
      headers: { "Content-Type": "application/json" },
      body: JSON.stringify({ loopout_overlay: loopoutOverlay }),
    });
    if (result && result.config) {
      state.config = result.config;
      if (state.data) {
        state.data.config = result.config;
      }
      populateLoopoutOverlaySettings(result.config.loopout_overlay || loopoutOverlay);
    }
    if (result && Array.isArray(result.models)) {
      state.data = { ...(state.data || {}), models: result.models };
      renderModels({ models: result.models, selected_model_id: state.config && state.config.model_id }, state.config && state.config.model_id);
    }
    if (result && Array.isArray(result.presets)) {
      renderPresets(result.presets);
    }
    const boundPresetName = String(currentModel() && currentModel().preset_name || "");
    setLoopoutOverlaySettingsOpen(false);
    showToast(boundPresetName ? `检测框设置已保存到 ${boundPresetName}` : "检测框设置已保存");
  }));
  document.addEventListener("click", (event) => {
    const control = $("displayLoopoutOverlaySettingsPopover")?.closest(".loopout-overlay-control");
    if (control && !control.contains(event.target)) {
      setLoopoutOverlaySettingsOpen(false);
    }
  });

  on("randomMouseHardwareButton", "click", () => {
    const selected = document.querySelector('input[name="mouse_proxy_mode"]:checked');
    if (selected && selected.value !== "synthetic") {
      showToast("透传模式使用真实鼠标身份，不能随机");
      return;
    }
    randomizeMouseHardware();
    showToast("鼠标身份已随机");
  });

  on("saveDisplayHardwareButton", "click", () => runUiAction(async () => {
    const config = validateDisplayHardware();
    const edidChanged = displayHardwareEdidChanged(config);
    if (edidChanged) {
      const confirmed = window.confirm("保存并应用会写入开机 HDMI RX EDID；设备不会立即重启，环出开关会立即切换。Windows 端可能需要重新检测 HDMI，确认继续？");
      if (!confirmed) {
        return;
      }
    }
    setHardwareStatus("displayHardwareStatus", "应用中");
    const result = await api("/api/hardware/display", {
      method: "PUT",
      headers: { "Content-Type": "application/json" },
      body: JSON.stringify({ config, apply: edidChanged, patch_boot_image: edidChanged, reboot_after_apply: false }),
    });
    populateDisplayHardware({
      available: true,
      config: result.config,
      status: result.result,
      display_mode: result.display_mode,
      loopout: result.loopout,
    });
    showToast(edidChanged ? "显示器模式已保存并应用" : "环出设置已保存");
  }));

  on("saveMouseProxyTimingButton", "click", () => runUiAction(async () => {
    const timing = validateMouseProxyTiming();
    const confirmed = window.confirm("保存等待时间会重启 usb-proxy 服务，Windows 会重新枚举 USB 鼠标。确认现在应用？");
    if (!confirmed) {
      return;
    }
    const result = await api("/api/hardware/mouse/timing", {
      method: "PUT",
      headers: { "Content-Type": "application/json" },
      body: JSON.stringify(timing),
    });
    populateMouseProxyTiming(result);
    showToast("USB 鼠标等待时间已保存并应用");
    await sleep(500);
    await loadHardware().catch(() => {});
  }));

  on("saveMouseHardwareButton", "click", () => runUiAction(async () => {
    const selected = document.querySelector('input[name="mouse_proxy_mode"]:checked');
    if (selected && selected.value !== "synthetic") {
      throw new Error("透传模式使用真实鼠标身份，不能保存伪装信息");
    }
    const confirmed = window.confirm("应用鼠标硬件信息会重启 usb-proxy 服务，Windows 会重新枚举 USB 鼠标。确认现在保存并应用？");
    if (!confirmed) {
      return;
    }
    const config = validateMouseHardware();
    setHardwareStatus("mouseHardwareStatus", "应用中");
    const result = await api("/api/hardware/mouse", {
      method: "PUT",
      headers: { "Content-Type": "application/json" },
      body: JSON.stringify({ config, apply_now: true }),
    });
    populateMouseHardware(result);
    showToast("鼠标信息已保存并应用，Windows 会重新枚举");
  }));

  Array.from(document.querySelectorAll('input[name="mouse_proxy_mode"]')).forEach((radio) => {
    radio.addEventListener("change", () => runUiAction(async () => {
      if (!radio.checked) {
        return;
      }
      updateMouseHardwareModeUi({ mode: radio.value });
      const fullWarning = radio.value === "full_passthrough"
        ? "\n\n完整透传会暴露原始复合 HID 接口。若切换后鼠标异常，请切换到“合成模式”。"
        : "";
      const confirmed = window.confirm(`切换 USB 鼠标模式会重启 usb-proxy 服务，Windows 会重新枚举 USB 设备。确认切换？${fullWarning}`);
      if (!confirmed) {
        await loadHardware();
        return;
      }
      markMouseModeSwitching();
      setHardwareStatus("mouseHardwareStatus", "切换中");
      const switchingLabel = radio.value === "synthetic"
        ? "合成模式"
        : "完整透传";
      showToast(`${switchingLabel}切换中，Windows 正在重新枚举`);
      const result = await api("/api/hardware/mouse/mode", {
        method: "PUT",
        headers: { "Content-Type": "application/json" },
        body: JSON.stringify({ mode: radio.value, apply_now: true }),
      });
      markMouseModeSwitching(15000);
      showToast(`已切换为${switchingLabel}`);
      await sleep(1000);
      await loadHardware().catch(() => {
        populateMouseHardware(result);
      });
    }));
  });

  on("refreshLicenseButton", "click", () => runUiAction(refreshLicenseStatus));
  on("licenseGateRefreshButton", "click", () => runUiAction(refreshLicenseStatus));

  on("reactivateDeviceButton", "click", () => runUiAction(async () => {
    const confirmed = window.confirm("授权修复会尝试修复更新后出现的设备身份变动问题，不会清除模型和预设参数。此功能仅用于提示授权不属于当前设备时使用，正常情况请勿点击。确认现在继续？");
    if (!confirmed) {
      return;
    }
    const result = await api("/api/system/reactivate", { method: "POST" });
    showToast(result.message || "授权修复已完成，请刷新页面后继续使用");
  }));

  on("activateLicenseButton", "click", () => runUiAction(() => activateLicenseFromInput("licenseKeyInput")));
  on("licenseGateActivateButton", "click", () => runUiAction(() => activateLicenseFromInput("licenseGateKeyInput")));
  ["licenseKeyInput", "licenseGateKeyInput"].forEach((id) => {
    const input = $(id);
    if (!input) {
      return;
    }
    input.addEventListener("input", () => syncLicenseKeyInputs(input));
    input.addEventListener("keydown", (event) => {
      if (event.key === "Enter") {
        event.preventDefault();
        runUiAction(() => activateLicenseFromInput(id));
      }
    });
  });

  on("checkUpdateButton", "click", () => runUiAction(async () => {
    const result = await checkUpdate();
    renderUpdateResult(result);
    showToast(result.update_available ? "发现可用更新" : "当前已是最新");
  }));

  on("switchUpdateVersionButton", "click", () => runUiAction(openUpdateVersionDialog));
  on("cleanupUpdateStatusButton", "click", () => runUiAction(cleanupStuckUpdateStatus));
  on("cancelUpdateVersionButton", "click", () => setUpdateVersionDialogOpen(false));
  on("confirmUpdateVersionButton", "click", () => runUiAction(switchToSelectedUpdateVersion));
  on("updateVersionSelect", "change", updateVersionSelectionSummary);

  on("installUpdateButton", "click", () => runUiAction(async () => {
    if (!state.updatePlan) {
      throw new Error("没有可安装的更新");
    }
    const themeOnly = !state.updatePlan.package && !Object.values(state.updatePlan.components || {}).some((item) => item && item.update_available);
    const fallback = state.updatePlan.theme_fallback;
    const confirmed = window.confirm(fallback
      ? `${fallback.title || fallback.theme_id || "当前收费主题"} 与目标系统不兼容。继续升级会保留购买权益和主题文件，但升级完成后切回默认主题。确认现在安装？`
      : themeOnly
        ? "确认安装已购买主题的更新？"
        : "安装更新会重启本地服务，过程中页面会短暂断开。确认现在安装？");
    if (!confirmed) {
      return;
    }
    const button = $("installUpdateButton");
    if (button) {
      button.disabled = true;
    }
    const result = await installUpdatePlan(state.updatePlan);
    if (result.theme_only) {
      stopUpdateStatusPolling();
      renderUpdateStatus({ status: "success", stage: "complete", message: result.message || "主题更新安装成功", progress: 100, type: "themes" });
      await refreshThemeStore({ silent: true }).catch(() => {});
      showToast("主题更新安装成功");
      if ((document.documentElement.dataset.visualTheme || "default") !== "default") {
        window.setTimeout(() => window.location.reload(), 500);
      }
      return;
    }
    renderUpdateStatus({
      status: "running",
      stage: "scheduled",
      message: result.message || "更新任务已启动，正在安装",
      progress: 65,
      version: result.version || state.updatePlan.latest_version || "",
      type: result.type || "",
      unit: result.unit || "",
    });
    showToast("更新任务已启动");
  }));

  on("refreshStorageButton", "click", () => runUiAction(() => refreshStorageStatus({ toast: true })));
  on("expandStorageButton", "click", () => runUiAction(expandStorage));

  on("addAimProfileButton", "click", () => {
    addAimProfileCard({
      hotkey: "left",
      class_filter_mask: currentModelClassMask(),
      offset_x: defaultAimProfileOffsetX(),
      offset_y: defaultAimProfileOffsetY(),
      sensitivity: 1,
      fov_scale: 1,
    });
  });

  on("physicalButtonBlockButton", "click", () => setPhysicalButtonBlockDialogOpen(true));
  on("closePhysicalButtonBlockButton", "click", () => setPhysicalButtonBlockDialogOpen(false));
  on("cancelPhysicalButtonBlockButton", "click", () => setPhysicalButtonBlockDialogOpen(false));
  on("addPhysicalButtonBlockButton", "click", addPhysicalButtonBlock);
  on("savePhysicalButtonBlockButton", "click", savePhysicalButtonBlockList);
  const physicalButtonBlockList = $("physicalButtonBlockList");
  if (physicalButtonBlockList) {
    physicalButtonBlockList.addEventListener("click", (event) => {
      const removeButton = event.target.closest("[data-physical-button]");
      if (!removeButton) {
        return;
      }
      state.blockedPhysicalButtonsDraft = state.blockedPhysicalButtonsDraft.filter(
        (buttonName) => buttonName !== removeButton.dataset.physicalButton
      );
      renderPhysicalButtonBlockEditor();
    });
  }
  const physicalButtonBlockDialog = $("physicalButtonBlockDialog");
  if (physicalButtonBlockDialog) {
    physicalButtonBlockDialog.addEventListener("click", (event) => {
      if (event.target === physicalButtonBlockDialog) {
        setPhysicalButtonBlockDialogOpen(false);
      }
    });
  }

  on("addAutoTriggerProfileButton", "click", () => {
    addAutoTriggerProfileCard(defaultAutoTriggerProfile());
  });

  on("startAutoCalibrationButton", "click", () => setAutoCalibrationConfirmOpen(true));
  on("closeAutoCalibrationConfirmButton", "click", () => setAutoCalibrationConfirmOpen(false));
  on("cancelAutoCalibrationConfirmButton", "click", () => setAutoCalibrationConfirmOpen(false));
  on("confirmAutoCalibrationButton", "click", () => runUiAction(startAutoCalibration));
  on("cancelAutoCalibrationButton", "click", () => runUiAction(cancelAutoCalibration));
  on("clearAutoCalibrationButton", "click", () => runUiAction(clearAutoCalibration));
  on("saveAutoCalibrationValuesButton", "click", () => runUiAction(saveAutoCalibrationValues));
  bindAutoCalibrationManualInputs();
  const calibrationDialog = $("autoCalibrationConfirmDialog");
  if (calibrationDialog) {
    calibrationDialog.addEventListener("click", (event) => {
      if (event.target === calibrationDialog) {
        setAutoCalibrationConfirmOpen(false);
      }
    });
  }
}

const AUTO_CALIBRATION_REASONS = {
  ready: "目标检测已经稳定。请确认物理鼠标处于静止状态，然后点击“开始自动标定”。",
  not_running: "采集或推理尚未运行。请先启动程序并确认模型已经正常输出检测框。",
  mouse_not_connected: "鼠标输出尚未连接。请检查鼠标设备和输出后端状态。",
  mouse_button_pressed: "检测到物理鼠标按键仍处于按下状态。请松开所有按键并保持鼠标静止。",
  no_target: "画面中没有可用目标。请将准星对准一个静止、完整可见的目标，并尽量放在采集画面中央。",
  predicted_target: "当前只有跟踪器预测框。请继续保持目标和鼠标静止，等待真实检测框恢复。",
  target_size_invalid: "目标尺寸不合适。请调整视角，使目标完整可见，且不要过小或占满采集画面。",
  target_near_edge: "目标距离画面左右边缘太近。请将目标移到采集画面中央附近，为水平往返移动留出空间。",
  target_unstable: "请将准星对准一个静止、完整可见且位于画面中央附近的目标，然后保持游戏视角和物理鼠标不动；系统需要连续采集多帧稳定检测，满足条件后按钮会自动可用。",
  busy: "自动标定正在进行",
  completed: "自动标定完成",
  cancelled: "自动标定已取消",
  failed: "自动标定失败",
};

const AUTO_CALIBRATION_PHASES = {
  preparing: "正在准备",
  starting: "正在准备",
  stabilize: "正在稳定目标",
  stabilize_x: "正在稳定 X 轴目标",
  sampling_x: "正在采样 X 轴响应",
  measure_x_response: "正在测量 X 轴响应延迟",
  analyzing_x: "正在分析 X 轴响应",
  measure_x_settle: "正在拟合 X 轴响应",
  stabilize_y: "正在稳定 Y 轴目标",
  sampling_y: "正在采样 Y 轴响应",
  measure_y_response: "正在测量 Y 轴响应延迟",
  analyzing_y: "正在分析 Y 轴响应",
  measure_y_settle: "正在拟合 Y 轴响应",
  validating: "正在验证标定结果",
  applying: "正在应用标定结果",
  saving: "正在保存标定结果",
  cancelling: "正在取消",
  completed: "标定完成",
  done: "标定完成",
  failed: "标定失败",
  error: "标定失败",
  cancelled: "标定已取消",
};

function setAutoCalibrationConfirmOpen(open) {
  const dialog = $("autoCalibrationConfirmDialog");
  if (!dialog) {
    return;
  }
  dialog.hidden = !open;
  setAnyModalOpen();
}

const AUTO_CALIBRATION_MANUAL_FIELDS = [
  { id: "autoCalibrationGainX", key: "gain_x_px_per_count", fallback: 0.55 },
  { id: "autoCalibrationGainY", key: "gain_y_px_per_count", fallback: 0.55 },
  { id: "autoCalibrationDelay", key: "response_delay_ms", fallback: 8.333 },
];

function autoCalibrationValueKey(calibration = {}) {
  return AUTO_CALIBRATION_MANUAL_FIELDS
    .map(({ key }) => Number(calibration[key]).toString())
    .concat([
      calibration.valid ? "1" : "0",
      calibration.model_id || "",
      calibration.calibrated_at || "",
    ])
    .join("|");
}

function autoCalibrationManualInputsValid() {
  return AUTO_CALIBRATION_MANUAL_FIELDS.every(({ id }) => {
    const input = $(id);
    return input && input.value.trim() !== "" && Number.isFinite(Number(input.value)) && input.checkValidity();
  });
}

function updateAutoCalibrationManualControls(running = !!((state.autoCalibration || {}).runtime || {}).running) {
  const disabled = running || state.autoCalibrationManualSaving;
  AUTO_CALIBRATION_MANUAL_FIELDS.forEach(({ id }) => {
    const input = $(id);
    if (input) {
      input.disabled = disabled;
    }
  });
  const saveButton = $("saveAutoCalibrationValuesButton");
  if (saveButton) {
    saveButton.disabled = disabled || !state.autoCalibrationDraftDirty || !autoCalibrationManualInputsValid();
    saveButton.textContent = state.autoCalibrationManualSaving ? "保存中..." : "保存手动参数";
  }
}

function populateAutoCalibrationManualInputs(calibration = {}) {
  if (state.autoCalibrationDraftDirty) {
    return;
  }
  AUTO_CALIBRATION_MANUAL_FIELDS.forEach(({ id, key, fallback }) => {
    const input = $(id);
    const value = Number(calibration[key] ?? fallback);
    if (input && Number.isFinite(value)) {
      input.value = value.toFixed(3);
    }
  });
}

function bindAutoCalibrationManualInputs() {
  AUTO_CALIBRATION_MANUAL_FIELDS.forEach(({ id }) => {
    const input = $(id);
    if (!input) {
      return;
    }
    input.addEventListener("input", () => {
      state.autoCalibrationDraftDirty = true;
      updateAutoCalibrationManualControls();
    });
    input.addEventListener("change", () => {
      clampNumberInputToLimits(input);
      updateAutoCalibrationManualControls();
    });
    input.addEventListener("keydown", (event) => {
      if (event.key !== "Enter") {
        return;
      }
      event.preventDefault();
      input.blur();
      const saveButton = $("saveAutoCalibrationValuesButton");
      if (saveButton && !saveButton.disabled) {
        runUiAction(saveAutoCalibrationValues);
      }
    });
  });
}

function renderAutoCalibration(payload) {
  if (!$("startAutoCalibrationButton")) {
    return;
  }
  const previous = state.autoCalibration || {};
  const runtime = (payload && payload.runtime) || previous.runtime || {};
  const calibration = (payload && payload.calibration) || previous.calibration || {};
  if (previous.calibration && autoCalibrationValueKey(previous.calibration) !== autoCalibrationValueKey(calibration)) {
    state.autoCalibrationDraftDirty = false;
  }
  state.autoCalibration = { runtime, calibration };

  const running = !!runtime.running;
  const valid = !!calibration.valid;
  const ready = !!runtime.ready && !running;
  const completed = runtime.status === "success" || runtime.status === "manual";
  const runningPhase = running
    ? `${AUTO_CALIBRATION_PHASES[runtime.state || runtime.phase] || "标定中"}${Number(runtime.round) > 0 ? `（${runtime.current_axis ? `${runtime.current_axis.toUpperCase()} 轴，` : ""}第 ${runtime.round}/${runtime.total_rounds || 10} 轮，${Number(runtime.valid_sample_count) || 0} 个有效样本，${(Number(runtime.elapsed_ms) / 1000).toFixed(1)} 秒）` : ""}`
    : "";
  const reason = runtime.error ||
    runningPhase ||
    AUTO_CALIBRATION_REASONS[runtime.reason] || "正在检查目标状态";
  const startButton = $("startAutoCalibrationButton");
  startButton.disabled = !ready;
  startButton.textContent = valid ? "重新自动标定" : "开始自动标定";
  setText("autoCalibrationReason", reason);
  const reasonElement = $("autoCalibrationReason");
  if (reasonElement) {
    reasonElement.hidden = valid && !running && completed;
    reasonElement.dataset.state = runtime.status === "failed"
      ? "error"
      : running ? "running" : ready ? "ready" : "waiting";
  }

  const badge = $("autoCalibrationStatusBadge");
  if (badge) {
    badge.textContent = running
      ? (AUTO_CALIBRATION_PHASES[runtime.phase] || "标定中")
      : runtime.status === "success"
        ? "✓ 已完成"
        : runtime.status === "manual"
          ? "✓ 已保存"
        : runtime.status === "failed"
          ? "失败"
          : runtime.status === "cancelled" ? "已取消" : ready ? "目标稳定" : "等待目标";
    badge.className = `status-badge ${completed ? "success" : running ? "live" : runtime.status === "failed" ? "error" : "idle"}`;
  }

  const successBanner = $("autoCalibrationSuccess");
  const showSuccess = valid && !running && runtime.status !== "failed" && runtime.status !== "cancelled";
  if (successBanner) {
    successBanner.hidden = !showSuccess;
  }
  if (showSuccess) {
    setText("autoCalibrationSuccessTitle", runtime.status === "success" ? "自动标定成功" : runtime.status === "manual" ? "手动参数已保存" : "当前标定已生效");
    setText(
      "autoCalibrationSuccessSummary",
      `X ${Number(calibration.gain_x_px_per_count ?? 0.55).toFixed(3)} · Y ${Number(calibration.gain_y_px_per_count ?? 0.55).toFixed(3)} px/count · 延迟 ${Number(calibration.response_delay_ms ?? 8.333).toFixed(3)} ms · 置信度 ${(Number(calibration.confidence || 0) * 100).toFixed(0)}%`
    );
  }

  const progress = Math.max(0, Math.min(100, Number(runtime.progress) || 0));
  const progressBar = $("autoCalibrationProgressBar");
  if (progressBar) {
    progressBar.style.width = `${progress}%`;
  }
  const cancelButton = $("cancelAutoCalibrationButton");
  if (cancelButton) {
    cancelButton.disabled = !running;
  }
  const clearButton = $("clearAutoCalibrationButton");
  if (clearButton) {
    clearButton.disabled = running || !valid;
  }

  const candidateTrackId = Number(runtime.candidate_track_id);
  const candidateClassId = Number(runtime.candidate_class_id);
  const candidateCount = Number(runtime.candidate_count) || 0;
  setText("autoCalibrationCandidate", candidateTrackId >= 0 && candidateClassId >= 0
    ? `${classDisplayName(candidateClassId)} / 轨迹 ${candidateTrackId}${candidateCount > 1 ? `（${candidateCount} 个候选中最稳定）` : ""}`
    : "--");
  const stableFrames = Number(runtime.stable_frames) || 0;
  const stableMs = Number(runtime.stable_ms) || 0;
  const jitterPx = Number(runtime.center_jitter_px) || 0;
  setText("autoCalibrationStability", stableFrames > 0
    ? `${stableFrames} 帧 / ${stableMs.toFixed(0)} ms / 抖动 ${jitterPx.toFixed(2)} px`
    : "--");
  populateAutoCalibrationManualInputs(calibration);
  updateAutoCalibrationManualControls(running);
  setText("autoCalibrationConfidence", valid ? `${(Number(calibration.confidence || 0) * 100).toFixed(0)}%` : "未标定");
  setText("autoCalibrationModel", calibration.model_id || "--");
  const captureWidth = Number(calibration.capture_width) || 0;
  const captureHeight = Number(calibration.capture_height) || 0;
  const cropSize = Number(calibration.crop_size) || 0;
  setText("autoCalibrationContext", valid && captureWidth && captureHeight
    ? `${captureWidth}×${captureHeight}${cropSize ? ` / 裁剪 ${cropSize}` : ""}`
    : "--");
  setText("autoCalibrationTime", calibration.calibrated_at || "--");
  updateAutoCalibrationPolling();
}

async function pollAutoCalibration() {
  if (state.autoCalibrationPollInFlight) {
    return;
  }
  state.autoCalibrationPollInFlight = true;
  try {
    renderAutoCalibration(await api("/api/control/calibration"));
  } catch (error) {
    setText("autoCalibrationReason", error.message || "无法读取标定状态");
  } finally {
    state.autoCalibrationPollInFlight = false;
  }
}

function updateAutoCalibrationPolling() {
  const running = !!((state.autoCalibration || {}).runtime || {}).running;
  const shouldPoll = state.activeControlSectionId === "control-section-auto-calibration" || running;
  if (!shouldPoll) {
    if (state.autoCalibrationPollTimer) {
      window.clearInterval(state.autoCalibrationPollTimer);
      state.autoCalibrationPollTimer = null;
    }
    return;
  }
  if (!state.autoCalibrationPollTimer) {
    pollAutoCalibration();
    state.autoCalibrationPollTimer = window.setInterval(pollAutoCalibration, 250);
  }
}

async function startAutoCalibration() {
  const button = $("confirmAutoCalibrationButton");
  if (button) {
    button.disabled = true;
  }
  try {
    const result = await api("/api/control/calibration/start", { method: "POST" });
    setAutoCalibrationConfirmOpen(false);
    state.autoCalibrationDraftDirty = false;
    renderAutoCalibration({
      runtime: result.runtime || {},
      calibration: (state.autoCalibration || {}).calibration || {},
    });
    setApplyStatus("ready", "标定运行中");
  } finally {
    if (button) {
      button.disabled = false;
    }
  }
}

async function saveAutoCalibrationValues() {
  if (!autoCalibrationManualInputsValid()) {
    throw new Error("请输入允许范围内的 X/Y 响应和鼠标响应延迟");
  }
  const values = Object.fromEntries(
    AUTO_CALIBRATION_MANUAL_FIELDS.map(({ id, key }) => [key, Number($(id).value)])
  );
  state.autoCalibrationManualSaving = true;
  updateAutoCalibrationManualControls();
  try {
    const result = await api("/api/control/calibration", {
      method: "PUT",
      headers: { "Content-Type": "application/json" },
      body: JSON.stringify(values),
    });
    state.autoCalibrationDraftDirty = false;
    renderAutoCalibration(result);
    showToast("手动标定参数已保存");
  } finally {
    state.autoCalibrationManualSaving = false;
    updateAutoCalibrationManualControls();
  }
}

async function cancelAutoCalibration() {
  const result = await api("/api/control/calibration/cancel", { method: "POST" });
  renderAutoCalibration({
    runtime: result.runtime || {},
    calibration: (state.autoCalibration || {}).calibration || {},
  });
}

async function clearAutoCalibration() {
  if (!window.confirm("清除后将恢复默认鼠标响应参数，确认继续？")) {
    return;
  }
  const result = await api("/api/control/calibration", { method: "DELETE" });
  state.autoCalibrationDraftDirty = false;
  renderAutoCalibration(result);
  showToast("自动标定参数已清除");
}

function handleLivePollError(error) {
  const message = error && error.message ? error.message : String(error || "连接已断开");
  if (suppressMouseSwitchDaemonTimeout(message)) {
    return;
  }
  if (!state.livePollErrorNotified) {
    showToast(message, true);
    state.livePollErrorNotified = true;
  }
  const badge = $("statusBadge");
  if (badge) {
    badge.textContent = "未连接";
    badge.className = "status-badge error";
  }
}

async function pollLiveState() {
  if (state.livePollInFlight) {
    return;
  }
  state.livePollInFlight = true;
  try {
    let payload = await api("/api/state");
    payload = await maybeRunLicenseRecovery(payload);
    applyLiveState(payload);
    state.livePollErrorNotified = false;
  } catch (error) {
    handleLivePollError(error);
  } finally {
    state.livePollInFlight = false;
  }
}

function initLiveStatePolling() {
  if (state.livePollTimer) {
    window.clearInterval(state.livePollTimer);
  }
  state.livePollTimer = window.setInterval(() => {
    pollLiveState();
  }, 1500);
}

async function main() {
  applyBrand({ ui_brand: document.documentElement.dataset.uiBrand || UI_BRAND_TTBOX });
  integrateFanSettings();
  initThemeControls();
  initThemeStore();
  fillOptions($("recoil_hotkey"), POINTER_HOTKEYS);
  fillOptions($("recoil_hotkey2"), OPTIONAL_POINTER_HOTKEYS);
  fillOptions($("rapid_fire_hotkey"), RAPID_FIRE_HOTKEYS);
  fillOptions($("controller_y_axis_fire_hotkey"), POINTER_HOTKEYS);
  fillOptions($("crosshair_hotkey"), OPTIONAL_POINTER_HOTKEYS, CROSSHAIR_MAIN_HOTKEY_LABELS);
  fillOptions($("crosshair_hotkey2"), OPTIONAL_POINTER_HOTKEYS);
  fillOptions($("hotkey_guard_toggle_hotkey"), POINTER_HOTKEYS);
  initPageNavigation();
  initControlSectionNavigation();
  initAssistSectionNavigation();
  enhanceNumericRangeControls();
  hardenInputAutofillHints();
  enableThumbOnlyRangeInputs();
  initRangeBindings();
  bindHardwareInputFilters();
  bindConfigAutoApply();
  bindAutoBackFlickClassDialogRanges();
  bindAssistModuleCollapseState();
  bindAutoStartControls();
  bindEvents();
  initSystemPolling();
  initWifiPolling();
  refreshLanBlocklist();

  try {
    await refreshAll();
  } catch (error) {
    document.body.classList.remove("license-loading");
    setApplyStatus("error", "连接失败");
    const badge = $("statusBadge");
    if (badge) {
      badge.textContent = "未连接";
      badge.className = "status-badge error";
    }
    showToast(error.message || String(error), true);
  }

  maybeShowDisclaimer();
  refreshInitialUpdateStatus().catch(() => {});
  initLiveStatePolling();
}

window.addEventListener("DOMContentLoaded", () => {
  main().catch((error) => showToast(error.message || String(error), true));
});
