/* TTBOX 插件中心（插件管理 + 本地市场） */
(function () {
  "use strict";

  var STATE_TEXT = {
    running: "运行中",
    stopped: "已停止",
    installed: "已安装",
    disabled: "已禁用",
    enabled: "已启用",
    failed: "失败",
    invalid: "无效",
    uninstalled: "未安装",
  };
  var HEALTH_TEXT = { healthy: "健康", degraded: "降级", failed: "失败", unknown: "未知" };
  var TYPE_TEXT = { process: "进程插件", builtin: "内置插件", invalid: "无效" };

  function $(id) { return document.getElementById(id); }

  function esc(s) {
    return String(s == null ? "" : s)
      .replace(/&/g, "&amp;")
      .replace(/</g, "&lt;")
      .replace(/>/g, "&gt;")
      .replace(/"/g, "&quot;");
  }

  function stateClass(st) {
    if (st === "running") return "pill is-ok";
    if (st === "failed" || st === "invalid") return "pill is-bad";
    if (st === "disabled") return "pill is-muted";
    return "pill";
  }

  function healthClass(h) {
    if (h === "healthy") return "pill is-ok";
    if (h === "failed") return "pill is-bad";
    if (h === "degraded") return "pill is-warn";
    return "pill is-muted";
  }

  function pill(cls, text) {
    return '<span class="' + cls + '">' + esc(text) + "</span>";
  }

  function pluginCard(p) {
    var st = p.state || "installed";
    var hl = p.health || "unknown";
    var running = st === "running";
    var actions = "";
    if (p.plugin_type === "process" || p.plugin_type === "builtin") {
      actions =
        '<button class="ghost-button" data-act="start" data-id="' + esc(p.plugin_id) + '"' + (running ? " disabled" : "") + ">启动</button>" +
        '<button class="ghost-button" data-act="stop" data-id="' + esc(p.plugin_id) + '"' + (running ? "" : " disabled") + ">停止</button>" +
        '<button class="ghost-button" data-act="restart" data-id="' + esc(p.plugin_id) + '">重启</button>' +
        '<button class="ghost-button" data-act="disable" data-id="' + esc(p.plugin_id) + '"' + (p.enabled ? "" : " disabled") + ">禁用</button>" +
        '<button class="ghost-button" data-act="enable" data-id="' + esc(p.plugin_id) + '"' + (p.enabled ? " disabled" : "") + ">启用</button>";
    }
    var meta = [
      p.plugin_type ? TYPE_TEXT[p.plugin_type] || p.plugin_type : "",
      p.version ? "v" + p.version : "",
      p.autostart ? "开机自启" : "",
    ].filter(Boolean).join(" · ");
    var error = p.error ? '<div class="field-error">' + esc(p.error) + "</div>" : "";
    return (
      '<article class="model-card plugin-card">' +
      '<div class="module-title"><h3>' + esc(p.name || p.plugin_id) + "</h3></div>" +
      '<div class="model-meta">' + esc(p.plugin_id) + " · " + esc(meta) + "</div>" +
      "<div>" +
      pill(stateClass(st), STATE_TEXT[st] || st) +
      pill(healthClass(hl), HEALTH_TEXT[hl] || hl) +
      "</div>" +
      '<div class="model-actions">' + actions + "</div>" +
      error +
      "</article>"
    );
  }

  function renderPlugins(list) {
    var grid = $("pluginGrid");
    if (!grid) return;
    var valid = list.filter(function (p) {
      return p.plugin_type !== "invalid" && p.plugin_id !== "__pycache__" && p.plugin_id !== "repository";
    });
    var running = valid.filter(function (p) { return p.state === "running"; }).length;
    var bad = valid.filter(function (p) {
      return p.state === "failed" || p.state === "invalid" || p.health === "failed";
    }).length;
    var summary = $("pluginsSummary");
    if (summary) summary.textContent = valid.length + " 个插件 · " + running + " 运行中 · " + bad + " 异常";
    grid.innerHTML = valid.length
      ? valid.map(pluginCard).join("")
      : '<div class="model-empty-state">暂无插件</div>';
  }

  function renderMarket(list) {
    var grid = $("marketGrid");
    if (!grid) return;
    var summary = $("marketSummary");
    if (summary) summary.textContent = (list || []).length + " 个可用";
    grid.innerHTML = list && list.length
      ? list.map(function (e) {
          return (
            '<article class="model-card plugin-card">' +
            '<div class="module-title"><h3>' + esc(e.name || e.plugin_id) + "</h3></div>" +
            '<div class="model-meta">' + esc(e.plugin_id) + " · v" + esc(e.version || "?") + (e.author ? " · " + esc(e.author) : "") + "</div>" +
            "<div>" + pill("pill is-muted", "本地仓库") + "</div>" +
            (e.description ? '<p class="model-desc">' + esc(e.description) + "</p>" : "") +
            "</article>"
          );
        }).join("")
      : '<div class="model-empty-state">本地仓库暂无插件包（.tpk）</div>';
  }

  function fetchJSON(url, opts) {
    return fetch(url, opts || {}).then(function (r) { return r.json(); });
  }

  function act(id, action) {
    return fetchJSON("/api/plugins/" + encodeURIComponent(id) + "/" + action, { method: "POST" })
      .then(function () { return refresh(); })
      .catch(function () { setTimeout(refresh, 800); });
  }

  function refresh() {
    return fetchJSON("/api/plugins")
      .then(function (d) { renderPlugins((d.data && d.data.plugins) || []); })
      .catch(function () {
        var grid = $("pluginGrid");
        if (grid) grid.innerHTML = '<div class="model-empty-state">插件状态暂不可用</div>';
      })
      .then(function () {
        return fetchJSON("/api/plugins/market");
      })
      .then(function (d) { renderMarket((d.data && d.data.plugins) || []); })
      .catch(function () { /* 市场不可用时保持空态 */ });
  }

  document.addEventListener("click", function (ev) {
    var btn = ev.target.closest ? ev.target.closest("[data-act]") : null;
    if (!btn) return;
    act(btn.dataset.id, btn.dataset.act);
  });

  document.addEventListener("DOMContentLoaded", function () {
    refresh();
    setInterval(refresh, 8000);
  });
})();
