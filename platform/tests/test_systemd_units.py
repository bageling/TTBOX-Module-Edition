import unittest
from platform.supervisor.systemd_units import CORE_UNIT,SUPERVISOR_UNIT
class UnitTests(unittest.TestCase):
 def test_core_unit_matches_boundary(self):
  self.assertIn('ExecStart=/opt/ttbox/core/current/ttbox-core',CORE_UNIT); self.assertIn('Restart=always',CORE_UNIT); self.assertIn('TTBOX_CONFIG_PATH=/opt/ttbox/config/current',CORE_UNIT); self.assertIn('RuntimeDirectory=ttbox',CORE_UNIT)
 def test_supervisor_depends_on_core(self):
  self.assertIn('After=network-online.target ttbox-core.service',SUPERVISOR_UNIT); self.assertIn('Restart=always',SUPERVISOR_UNIT)
if __name__=='__main__': unittest.main()
