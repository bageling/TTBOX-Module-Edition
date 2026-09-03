"""Web Plugin 的 Framework 兼容 API。

只做路由适配：所有插件安装/升级仍进入 PluginManager；系统能力由既有适配器提供。
"""
from __future__ import annotations
import os
from dataclasses import asdict, is_dataclass
from pathlib import Path
from flask import jsonify, request, render_template

from framework.plugin_manager import InstallRequest, InstallSource, LocalRepository, PluginManager
from plugins.system_host import SystemPluginHost


def _jsonable(value):
    if hasattr(value, "value"): return value.value
    if is_dataclass(value): return {k: _jsonable(v) for k, v in asdict(value).items()}
    if isinstance(value, dict): return {k: _jsonable(v) for k, v in value.items()}
    if isinstance(value, (list, tuple)): return [_jsonable(v) for v in value]
    return value


def install_framework_api(app):
    plugins_root = Path(os.environ.get("TTBOX_PLUGINS_ROOT", "/opt/ttbox/plugins"))
    repository_root = Path(os.environ.get("TTBOX_PLUGIN_REPOSITORY_ROOT", str(plugins_root / "repository")))
    manager = PluginManager(plugins_root=plugins_root)
    repository = LocalRepository(repository_root)
    system = SystemPluginHost.create()
    app.extensions["ttbox_plugin_manager"] = manager
    app.extensions["ttbox_plugin_repository"] = repository
    app.extensions["ttbox_system_plugins"] = system

    @app.get("/plugins")
    def plugin_page():
        return render_template("plugins.html")

    @app.get("/api/plugins")
    def plugin_list():
        manager.discover()
        return jsonify({"ok": True, "data": {"plugins": _jsonable(manager.get_installed_plugins())}})

    @app.get("/api/plugins/<plugin_id>")
    def plugin_detail(plugin_id):
        record = manager.get_plugin(plugin_id)
        if record is None: return jsonify({"ok": False, "error": "插件不存在"}), 404
        return jsonify({"ok": True, "data": _jsonable(record)})

    @app.get("/api/plugins/<plugin_id>/status")
    def plugin_status(plugin_id):
        try: return jsonify({"ok": True, "data": _jsonable(manager.get_plugin_status(plugin_id))})
        except KeyError: return jsonify({"ok": False, "error": "插件不存在"}), 404

    def _plugin_action(plugin_id, action):
        try: result = getattr(manager, action)(plugin_id)
        except Exception as exc: return jsonify({"ok": False, "error": str(exc)}), 400
        return jsonify({"ok": bool(result is not False), "data": _jsonable(result)})

    for action in ("enable", "disable", "start", "stop", "restart", "rollback", "uninstall"):
        endpoint = f"plugin_{action}"
        app.add_url_rule(f"/api/plugins/<plugin_id>/{action}", endpoint, lambda plugin_id, _action=action: _plugin_action(plugin_id, _action), methods=["POST"])

    @app.post("/api/plugins/install")
    def plugin_install():
        body = request.get_json(silent=True) or {}
        source = body.get("source", "local_file")
        try:
            request_model = InstallRequest(InstallSource(source), path=body.get("path"), url=body.get("url"), plugin_id=body.get("plugin_id"), version=body.get("version"), expected_sha256=body.get("sha256"))
            record = manager.install(request_model)
            return jsonify({"ok": True, "data": _jsonable(record)}), 201
        except Exception as exc: return jsonify({"ok": False, "error": str(exc)}), 400

    @app.post("/api/plugins/upgrade")
    def plugin_upgrade():
        body = request.get_json(silent=True) or {}
        try:
            source = InstallSource(body.get("source", "local_file"))
            request_model = InstallRequest(source, path=body.get("path"), url=body.get("url"), plugin_id=body.get("plugin_id"), version=body.get("version"), expected_sha256=body.get("sha256"))
            return jsonify({"ok": True, "data": _jsonable(manager.upgrade(request_model))})
        except Exception as exc: return jsonify({"ok": False, "error": str(exc)}), 400

    @app.get("/api/plugins/market")
    def plugin_market():
        query = request.args.get("q", "")
        entries = repository.search_plugins(query) if query else repository.list_plugins()
        return jsonify({"ok": True, "data": {"source": "local", "online": False, "plugins": _jsonable(entries)}})

    @app.get("/api/plugins/market/<plugin_id>")
    def market_plugin(plugin_id):
        entry = repository.get_plugin(plugin_id)
        if entry is None: return jsonify({"ok": False, "error": "市场插件不存在"}), 404
        return jsonify({"ok": True, "data": _jsonable(entry)})

    @app.get("/api/plugins/market/<plugin_id>/versions")
    def market_versions(plugin_id):
        return jsonify({"ok": True, "data": {"plugin_id": plugin_id, "versions": repository.get_versions(plugin_id)}})

    def _system_status(plugin_id):
        try: return jsonify({"ok": True, "data": _jsonable(system.service(plugin_id).status())})
        except KeyError: return jsonify({"ok": False, "error": "系统插件不存在"}), 404

    for plugin_id in ("system", "core", "model", "network", "wifi", "fan", "monitor", "upgrade"):
        endpoint = f"system_api_{plugin_id}"
        if plugin_id == "core":
            app.add_url_rule("/api/core/status", endpoint, lambda: jsonify({"ok": True, "data": {"source": "core_ipc", "available": True}}), methods=["GET"])
        elif plugin_id == "model":
            app.add_url_rule("/api/model/list", endpoint, lambda: jsonify({"ok": True, "data": {"source": "model_plugin", "available": True}}), methods=["GET"])
        else:
            app.add_url_rule(f"/api/{plugin_id}/status", endpoint, lambda _plugin_id=plugin_id: _system_status(_plugin_id), methods=["GET"])
    app.add_url_rule("/api/model/active", "system_api_model_active", lambda: jsonify({"ok": True, "data": {"source": "model_plugin", "available": True}}), methods=["GET"])
    app.add_url_rule("/api/hwmon", "system_api_hwmon", lambda: _system_status("monitor"), methods=["GET"])
    return manager
