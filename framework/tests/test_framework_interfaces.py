import tempfile
import unittest
from pathlib import Path

from framework.config.service import ConfigService
from framework.plugin_manager.manager import PluginManager, PluginSpec, PluginState
from framework.security.policy import Permission, SecurityPolicy
from framework.service.manager import ServiceManager, ServiceSpec, ServiceState
from framework.runtime import FrameworkRuntime


class FrameworkInterfaceTests(unittest.TestCase):
    def test_plugin_lifecycle_and_core_independence(self):
        events = []
        manager = PluginManager()
        manager.register(PluginSpec("demo", start=lambda: events.append("plugin-start"), stop=lambda: events.append("plugin-stop")))
        self.assertTrue(manager.enable("demo"))
        self.assertTrue(manager.start("demo"))
        self.assertEqual(manager.status("demo").state, PluginState.RUNNING)
        self.assertTrue(manager.restart("demo"))
        self.assertTrue(manager.stop("demo"))
        self.assertEqual(events, ["plugin-start", "plugin-stop", "plugin-start", "plugin-stop"])

    def test_config_load_set_validate_and_notify(self):
        with tempfile.TemporaryDirectory() as td:
            path = Path(td) / "config.json"
            path.write_text('{"core": {"enabled": true}}', encoding="utf-8")
            changed = []
            config = ConfigService({"core.enabled": lambda value: isinstance(value, bool)})
            config.subscribe(lambda key, value: changed.append((key, value)))
            self.assertTrue(config.load(path))
            self.assertEqual(config.get("core.enabled"), True)
            self.assertTrue(config.set("core.enabled", False))
            self.assertTrue(config.save())
            self.assertEqual(changed, [("core.enabled", False)])
            self.assertEqual(config.get("core.enabled"), False)

    def test_service_manager_lifecycle(self):
        calls = []
        services = ServiceManager({"core": ServiceSpec("core", start=lambda: calls.append("start"), stop=lambda: calls.append("stop"))})
        self.assertTrue(services.start("core"))
        self.assertEqual(services.status("core").state, ServiceState.RUNNING)
        self.assertTrue(services.restart("core"))
        self.assertTrue(services.stop("core"))
        self.assertEqual(calls, ["start", "stop", "start", "stop"])

    def test_security_permission(self):
        policy = SecurityPolicy({"demo": {Permission.READ_CONFIG}})
        self.assertTrue(policy.check("demo", Permission.READ_CONFIG))
        self.assertFalse(policy.check("demo", Permission.WRITE_CONFIG))
        self.assertTrue(policy.declare("demo", {Permission.WRITE_CONFIG}))
        self.assertTrue(policy.check("demo", Permission.WRITE_CONFIG))

    def test_framework_start_stop_order(self):
        events = []
        runtime = FrameworkRuntime(
            config=ConfigService(),
            services=ServiceManager({"core": ServiceSpec("core", start=lambda: events.append("service-start"), stop=lambda: events.append("service-stop"))}),
            plugins=PluginManager(),
            security=SecurityPolicy(),
            core_service="core",
        )
        self.assertTrue(runtime.start())
        self.assertTrue(runtime.stop())
        self.assertEqual(events, ["service-start", "service-stop"])


if __name__ == "__main__":
    unittest.main()
