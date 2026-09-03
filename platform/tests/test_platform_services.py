import tempfile,unittest
from pathlib import Path
from platform.supervisor.service_catalog import ServiceCatalog
from platform.health.checks import PlatformHealth
from platform.runtime.controller import RuntimeController
from platform.runtime.process_adapter import MockProcessAdapter
class PlatformServiceTests(unittest.TestCase):
 def test_catalog_dependencies(self):
  c=ServiceCatalog(); self.assertEqual(c.get('ttbox-core').restart,'always'); self.assertIn('ttbox-core',c.get('ttbox-supervisor').after)
 def test_health_reports_runtime_and_storage(self):
  r=RuntimeController(MockProcessAdapter());
  with tempfile.TemporaryDirectory() as td:
   h=PlatformHealth(r,td).check(); self.assertFalse(h.ok); self.assertFalse(h.checks['runtime']['ok'])
   r.start(); self.assertTrue(PlatformHealth(r,td).check().ok)
if __name__=='__main__':unittest.main()
