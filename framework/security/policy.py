from enum import Enum
class Permission(str, Enum): READ_CONFIG="read_config"; WRITE_CONFIG="write_config"; CONTROL_SERVICE="control_service"; MANAGE_PLUGIN="manage_plugin"; SENSITIVE="sensitive"
class SecurityPolicy:
    def __init__(self, declarations=None): self._permissions = {k: set(v) for k, v in (declarations or {}).items()}
    def declare(self, plugin_id, permissions): self._permissions.setdefault(plugin_id, set()).update(permissions); return True
    def check(self, plugin_id, permission): return permission in self._permissions.get(plugin_id, set())
    def authorize_sensitive(self, plugin_id): return self.check(plugin_id, Permission.SENSITIVE)
