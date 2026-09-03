import importlib.util
import json
import os
import tempfile
import unittest
from pathlib import Path


class WebFrameworkApiTests(unittest.TestCase):
    def load_app(self, root):
        os.environ["TTBOX_PLUGINS_ROOT"] = str(root / "plugins")
        os.environ["TTBOX_PLUGIN_REPOSITORY_ROOT"] = str(root / "repository")
        entry = Path(__file__).resolve().parents[2] / "plugins/web/bin/ttbox-web.py"
        spec = importlib.util.spec_from_file_location("web_framework_api_test", entry)
        module = importlib.util.module_from_spec(spec)
        spec.loader.exec_module(module)
        return module

    def test_framework_and_market_api_are_registered(self):
        with tempfile.TemporaryDirectory() as td:
            root = Path(td); module = self.load_app(root); client = module.app.test_client()
            rules = {rule.rule for rule in module.app.url_map.iter_rules()}
            for route in ("/api/plugins", "/api/plugins/market", "/api/system/status", "/api/core/status", "/api/model/list", "/api/model/active", "/api/network/status", "/api/wifi/status", "/api/fan/status", "/api/monitor/status", "/api/upgrade/status"):
                self.assertIn(route, rules)
            self.assertEqual(client.get("/api/plugins").status_code, 200)
            self.assertEqual(client.get("/api/plugins/market").status_code, 200)

    def test_market_api_only_reads_local_repository(self):
        with tempfile.TemporaryDirectory() as td:
            root = Path(td); module = self.load_app(root); client = module.app.test_client()
            response = client.get("/api/plugins/market", query_string={"q": "web"})
            payload = response.get_json()
            self.assertTrue(payload["ok"])
            self.assertEqual(payload["data"]["source"], "local")
            self.assertEqual(payload["data"]["online"], False)

    def test_legacy_web_routes_still_exist(self):
        with tempfile.TemporaryDirectory() as td:
            module = self.load_app(Path(td)); rules = {rule.rule for rule in module.app.url_map.iter_rules()}
            for route in ("/api/state", "/api/models", "/api/system", "/api/network/wifi", "/api/hwmon", "/api/update/status", "/api/preview.mjpg"):
                self.assertIn(route, rules)


if __name__ == "__main__": unittest.main()
