import io
import json
import tarfile
import tempfile
import unittest
from pathlib import Path

from framework.plugin_manager.standard import (
    PluginManifest,
    PluginManifestError,
    PluginPackage,
    PluginStore,
)


class PluginPackageTests(unittest.TestCase):
    def manifest(self, **overrides):
        data = {"id": "web", "name": "TTBOX Web", "version": "1.0.0", "description": "", "api_version": "1", "entry": "bin/ttbox-web", "type": "process", "autostart": True, "enabled": True}
        data.update(overrides)
        return data

    def make_tpk(self, directory, data=None, root="web"):
        package = Path(directory) / f"{root}.tpk"
        with tarfile.open(package, "w") as archive:
            payload = json.dumps(data or self.manifest()).encode()
            info = tarfile.TarInfo(f"{root}/plugin.json"); info.size = len(payload); archive.addfile(info, io.BytesIO(payload))
            info = tarfile.TarInfo(f"{root}/bin/ttbox-web"); info.size = 0; archive.addfile(info)
        return package

    def test_valid_manifest_and_defaults(self):
        manifest = PluginManifest.from_dict(self.manifest())
        self.assertEqual(manifest.plugin_id, "web")
        self.assertEqual(manifest.stop_timeout, 10)
        self.assertEqual(manifest.dependencies, ())
        self.assertEqual(manifest.permissions, ())

    def test_required_and_enum_validation(self):
        for key in ("id", "name", "version", "api_version", "entry", "type"):
            data = self.manifest(); data.pop(key)
            with self.assertRaises(PluginManifestError): PluginManifest.from_dict(data)
        with self.assertRaises(PluginManifestError): PluginManifest.from_dict(self.manifest(type="library"))
        with self.assertRaises(PluginManifestError): PluginManifest.from_dict(self.manifest(version="1"))
        with self.assertRaises(PluginManifestError): PluginManifest.from_dict(self.manifest(api_version="2"))
        with self.assertRaises(PluginManifestError): PluginManifest.from_dict(self.manifest(entry="../escape"))

    def test_permissions_and_dependencies_are_normalized(self):
        manifest = PluginManifest.from_dict(self.manifest(permissions=["read_config"], dependencies=["core>=1.0.0"]))
        self.assertEqual(manifest.permissions, ("read_config",))
        self.assertEqual(manifest.dependencies, ("core>=1.0.0",))
        with self.assertRaises(PluginManifestError): PluginManifest.from_dict(self.manifest(permissions=["root"]))

    def test_tpk_validation_rejects_traversal_and_requires_manifest(self):
        with tempfile.TemporaryDirectory() as directory:
            package = Path(directory) / "bad.tpk"
            with tarfile.open(package, "w") as archive:
                info = tarfile.TarInfo("../escape.txt"); info.size = 0; archive.addfile(info)
            with self.assertRaises(PluginManifestError): PluginPackage.inspect(package)

    def test_tpk_validation_reads_manifest(self):
        with tempfile.TemporaryDirectory() as directory:
            package = self.make_tpk(directory)
            inspected = PluginPackage.inspect(package)
            self.assertEqual(inspected.manifest.plugin_id, "web")
            self.assertEqual(inspected.root_name, "web")

    def test_install_upgrade_rollback_and_uninstall_are_private(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory) / "plugins"; store = PluginStore(root)
            first = self.make_tpk(directory)
            self.assertEqual(store.install(first).version, "1.0.0")
            self.assertEqual((root / "web" / "plugin.json").exists(), True)
            second = self.make_tpk(directory, self.manifest(version="2.0.0"))
            self.assertEqual(store.upgrade(second).version, "2.0.0")
            self.assertTrue(store.rollback("web"))
            self.assertEqual(store.inspect("web").manifest.version, "1.0.0")
            public = Path(directory) / "public"; public.mkdir(); (public / "keep").write_text("x")
            self.assertTrue(store.uninstall("web"))
            self.assertFalse((root / "web").exists()); self.assertTrue((public / "keep").exists())


if __name__ == "__main__": unittest.main()
