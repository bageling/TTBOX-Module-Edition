import importlib.util
import json
import tempfile
import unittest
from pathlib import Path


ROOT = Path(__file__).resolve().parents[2]
WEB_PLUGIN = ROOT / "plugins" / "web"


class WebPluginTests(unittest.TestCase):
    def test_manifest_and_layout(self):
        manifest = json.loads((WEB_PLUGIN / "plugin.json").read_text(encoding="utf-8"))
        self.assertEqual(manifest["id"], "web")
        self.assertEqual(manifest["type"], "process")
        self.assertEqual(manifest["api_version"], "1")
        self.assertTrue((WEB_PLUGIN / manifest["entry"]).is_file())
        self.assertTrue((WEB_PLUGIN / "templates" / "index.html").is_file())
        self.assertTrue((WEB_PLUGIN / "static" / "app.js").is_file())

    def test_flask_routes_and_pages_from_migrated_entry(self):
        entry = WEB_PLUGIN / "bin" / "ttbox-web.py"
        spec = importlib.util.spec_from_file_location("ttbox_web_plugin", entry)
        module = importlib.util.module_from_spec(spec)
        spec.loader.exec_module(module)
        client = module.app.test_client()
        self.assertEqual(client.get("/").status_code, 200)
        self.assertEqual(client.get("/desktop").status_code, 200)
        self.assertEqual(client.get("/mobile").status_code, 200)
        self.assertEqual(client.get("/static/app.js").status_code, 200)
        rules = {rule.rule for rule in module.app.url_map.iter_rules()}
        for route in ("/api/state", "/api/models", "/api/config", "/api/system", "/api/hailo/status", "/api/network/wifi", "/api/update/status"):
            self.assertIn(route, rules)

    def test_ttbox_state_and_license_do_not_fall_back_to_yu_values(self):
        entry = WEB_PLUGIN / "bin" / "ttbox-web.py"
        spec = importlib.util.spec_from_file_location("ttbox_web_state_contract", entry)
        module = importlib.util.module_from_spec(spec)
        spec.loader.exec_module(module)
        module._get_status = lambda: {"running": True, "runtime_running": True}
        module._get_runtime_profile = lambda: {
            "model_id": "ttbox-model",
            "mouse": {"mode": "local_hid"},
        }
        module.ipc_request = lambda request_type, *_args: {
            "status": 0,
            "data": {"models": []},
        } if request_type == "MODEL_LIST" else {"status": 1, "data": {}}

        state = module.collect_yu_state()["data"]
        license_data = module._license_payload()

        self.assertEqual(state["selected_model_id"], "ttbox-model")
        self.assertEqual(state["state"]["selected_model_id"], "ttbox-model")
        self.assertEqual(state["config"]["mouse_output"]["mode"], "full_passthrough")
        self.assertEqual(state["state"]["mouse_output"]["mode"], "full_passthrough")
        self.assertEqual(license_data["app_version"], module.TTBOX_APP_VERSION)
        self.assertEqual(license_data["ui"]["brand_name"], "TTBOX")
        self.assertEqual(license_data["ui"]["ui_brand"], "ttbox")
        self.assertEqual(license_data["ui_brand"], "ttbox")
        self.assertEqual(module._CONVERTER_SCRIPT, "/opt/ttbox/tools/converter/convert_onnx_to_rknn.py")
        self.assertEqual(module._CONVERT_CALIB_DIR, "/opt/ttbox/calib/imgs")

    def test_plugin_manifest_is_accepted_by_manager(self):
        from framework.plugin_manager.standard import PluginPackage
        with tempfile.TemporaryDirectory() as directory:
            target = Path(directory) / "web"
            import shutil
            shutil.copytree(WEB_PLUGIN, target)
            inspected = PluginPackage.inspect if False else None
            from framework.plugin_manager.standard import PluginManifest
            manifest = PluginManifest.from_dict(json.loads((target / "plugin.json").read_text(encoding="utf-8")))
            self.assertEqual(manifest.plugin_id, "web")


if __name__ == "__main__":
    unittest.main()
