import io
import json
import tarfile
import tempfile
import unittest
from pathlib import Path

from framework.plugin_manager import (
    CatalogEntry, InstallRequest, InstallSource, LocalRepository, PluginManager,
    PluginSpec, PluginState, PluginHealth,
)
from framework.plugin_manager.discovery import PluginDiscovery
from framework.plugin_manager.models import DependencyCycleError, DependencyMissingError
from framework.plugin_manager.standard import PluginManifestError
from framework.security.policy import Permission, SecurityPolicy


class ManagerTests(unittest.TestCase):
    def manifest(self, plugin_id="demo", version="1.0.0", **extra):
        value = {"id": plugin_id, "name": plugin_id, "version": version, "api_version": "1", "entry": "bin/run", "type": "builtin", "enabled": True, "autostart": True}
        value.update(extra)
        return value

    def package(self, directory, plugin_id="demo", version="1.0.0", **extra):
        path = Path(directory) / f"{plugin_id}-{version}.tpk"
        with tarfile.open(path, "w") as archive:
            payload = json.dumps(self.manifest(plugin_id, version, **extra)).encode()
            info = tarfile.TarInfo(f"{plugin_id}/plugin.json"); info.size=len(payload); archive.addfile(info, io.BytesIO(payload))
            info = tarfile.TarInfo(f"{plugin_id}/bin/run"); info.size=0; archive.addfile(info)
        return path

    def test_discovery_valid_invalid_and_persistence(self):
        with tempfile.TemporaryDirectory() as td:
            root=Path(td)/"plugins"; root.mkdir(); valid=root/"demo"; valid.mkdir(); (valid/"plugin.json").write_text(json.dumps(self.manifest()), encoding="utf-8")
            bad=root/"bad"; bad.mkdir(); (bad/"plugin.json").write_text("{}", encoding="utf-8")
            records=PluginDiscovery(root).scan(); self.assertEqual({x.state for x in records}, {PluginState.INSTALLED, PluginState.INVALID})
            manager=PluginManager(root, root/"state.json"); manager.discover(); self.assertTrue(manager.registry.path.exists())
            restored=PluginManager(root, root/"state.json"); self.assertIsNotNone(restored.get_plugin("demo"))

    def test_install_sources_permissions_and_uninstall(self):
        with tempfile.TemporaryDirectory() as td:
            root=Path(td)/"plugins"; package=self.package(td, permissions=["read_config", "manage_plugin"])
            policy=SecurityPolicy(); manager=PluginManager(root, security=policy)
            record=manager.install(InstallRequest(InstallSource.LOCAL_FILE, path=str(package)))
            self.assertEqual(record.plugin_id, "demo"); self.assertTrue(policy.check("demo", Permission.READ_CONFIG.value))
            with self.assertRaises(Exception): manager.install(InstallRequest(InstallSource.LOCAL_FILE, path=str(package)))
            public=Path(td)/"public"; public.mkdir(); (public/"keep").write_text("x")
            self.assertTrue(manager.uninstall("demo")); self.assertTrue((public/"keep").exists())

    def test_builtin_lifecycle_and_autostart(self):
        with tempfile.TemporaryDirectory() as td:
            events=[]; root=Path(td)/"plugins"; package=self.package(td)
            manager=PluginManager(root, runtime_factory=lambda record, spec: __import__("framework.plugin_manager.lifecycle", fromlist=["BuiltinPluginRuntime"]).BuiltinPluginRuntime(lambda: events.append("start"), lambda: events.append("stop")))
            manager.install(InstallRequest(InstallSource.LOCAL_FILE, path=str(package)))
            manager._items["demo"]=PluginSpec("demo"); self.assertTrue(manager.start_all()); self.assertEqual(manager.status("demo").state, PluginState.RUNNING)
            self.assertTrue(manager.stop("demo")); self.assertEqual(events, ["start", "stop"])

    def test_dependencies_and_cycle(self):
        with tempfile.TemporaryDirectory() as td:
            root=Path(td)/"plugins"; manager=PluginManager(root)
            manager.install(InstallRequest(InstallSource.LOCAL_FILE, path=str(self.package(td, "base"))))
            manager.install(InstallRequest(InstallSource.LOCAL_FILE, path=str(self.package(td, "app", dependencies=["base"]))))
            self.assertTrue(manager.start("app"))
            with self.assertRaises(DependencyMissingError): manager.install(InstallRequest(InstallSource.LOCAL_FILE, path=str(self.package(td, "bad", dependencies=["missing"]))))
            manager.registry.require("base").dependencies=("app",)
            with self.assertRaises(DependencyCycleError): manager.start("app")

    def test_repository_market_api(self):
        with tempfile.TemporaryDirectory() as td:
            package=self.package(td); repo=LocalRepository(td); manager=PluginManager(Path(td)/"plugins", repository=repo)
            self.assertEqual(repo.get_versions("demo"), ["1.0.0"]); self.assertEqual(manager.get_market_plugin("demo").plugin_id, "demo")
            self.assertEqual(manager.get_market_versions("demo"), ["1.0.0"]); self.assertEqual(manager.search_plugins("demo")[0].plugin_id, "demo")

    def test_upgrade_rollback_and_transaction(self):
        with tempfile.TemporaryDirectory() as td:
            root=Path(td)/"plugins"; manager=PluginManager(root)
            manager.install(InstallRequest(InstallSource.LOCAL_FILE, path=str(self.package(td, version="1.0.0"))))
            upgraded=self.package(td, version="2.0.0"); result=manager.upgrade(InstallRequest(InstallSource.LOCAL_FILE, path=str(upgraded)))
            self.assertEqual(result.version, "2.0.0"); self.assertTrue(manager.rollback("demo")); self.assertEqual(manager.get_plugin("demo").version, "1.0.0")
            self.assertTrue(manager.transactions.path.exists())

    def test_integrity_and_path_safety(self):
        with tempfile.TemporaryDirectory() as td:
            package=self.package(td); import hashlib
            expected=hashlib.sha256(package.read_bytes()).hexdigest(); manager=PluginManager(Path(td)/"plugins")
            self.assertTrue(manager.verify_integrity(package, expected))
            with self.assertRaises(Exception): manager.verify_integrity(package, "0"*64)


if __name__ == "__main__": unittest.main()
