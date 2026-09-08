import json
import tempfile
import unittest
from pathlib import Path

from plugins.model.model_service import ModelPluginService, ModelState
from plugins.model.model_contract import ModelManifest, ModelManifestError


class ModelPluginTests(unittest.TestCase):
    def manifest(self, **overrides):
        data = {
            "id": "demo",
            "name": "Demo Model",
            "version": "1.0.0",
            "format": "rknn",
            "framework": "yolo",
            "description": "test model",
        }
        data.update(overrides)
        return data

    def test_manifest_preserves_unknown_hardware_metadata_without_inventing(self):
        manifest = ModelManifest.from_dict(self.manifest(input_width=320, input_height=320, quantization="INT8"))
        self.assertEqual(manifest.model_id, "demo")
        self.assertEqual(manifest.input_width, 320)
        self.assertIsNone(ModelManifest.from_dict(self.manifest()).input_width)

    def test_invalid_manifest_and_model_path(self):
        with self.assertRaises(ModelManifestError): ModelManifest.from_dict(self.manifest(id="../bad"))
        with self.assertRaises(ModelManifestError): ModelManifest.from_dict(self.manifest(format="onnx"))

    def test_install_activate_switch_and_rollback(self):
        with tempfile.TemporaryDirectory() as td:
            root = Path(td) / "models"; source = Path(td) / "source.rknn"; source.write_bytes(b"rknn" * 300)
            service = ModelPluginService(root)
            service.upload("one", source, self.manifest(id="one")); service.validate("one"); service.install("one"); service.activate("one")
            service.upload("two", source, self.manifest(id="two", version="2.0.0")); service.validate("two"); service.install("two")
            self.assertEqual(service.activate("two").state, ModelState.ACTIVE)
            self.assertEqual(service.active().model_id, "two")
            self.assertEqual(service.rollback().model_id, "one")

    def test_failed_activation_keeps_old_active(self):
        with tempfile.TemporaryDirectory() as td:
            root = Path(td) / "models"; source = Path(td) / "source.rknn"; source.write_bytes(b"rknn" * 300)
            service = ModelPluginService(root, loader=lambda path, manifest: manifest.model_id != "bad")
            service.upload("one", source, self.manifest(id="one")); service.validate("one"); service.install("one"); service.activate("one")
            bad = Path(td) / "bad.rknn"; bad.write_bytes(b"bad" * 300)
            service.upload("bad", bad, self.manifest(id="bad"))
            with self.assertRaises(Exception): service.validate("bad")
            self.assertEqual(service.active().model_id, "one")

    def test_list_delete_and_metadata(self):
        with tempfile.TemporaryDirectory() as td:
            root = Path(td) / "models"; source = Path(td) / "source.rknn"; source.write_bytes(b"rknn" * 300)
            service = ModelPluginService(root)
            service.upload("one", source, self.manifest(id="one", classes=1)); service.validate("one"); service.install("one")
            self.assertEqual(service.list()[0].metadata["classes"], 1)
            with self.assertRaises(Exception): service.delete("one") if service.active().model_id == "one" else None
            self.assertTrue(service.delete("one"))

    def test_active_json_is_single_source_of_truth(self):
        with tempfile.TemporaryDirectory() as td:
            root = Path(td) / "models"; source = Path(td) / "source.rknn"; source.write_bytes(b"rknn" * 300)
            service = ModelPluginService(root)
            service.upload("one", source, self.manifest(id="one")); service.validate("one"); service.install("one"); service.activate("one")
            active = json.loads((root / "active" / "active.json").read_text(encoding="utf-8"))
            self.assertEqual(active["model_id"], "one")


if __name__ == "__main__": unittest.main()
