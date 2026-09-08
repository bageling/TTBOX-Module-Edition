# Phase 2 Service / Supervisor / Health

TTBOX unit facts are represented by `ServiceCatalog` and controlled through `SystemdServiceAdapter`:

- Core: `Restart=always`, `RestartSec=5`, `User=ttbox`, `/var/lib/ttbox`, `/etc/ttbox`.
- HID: `ExecStartPre=modprobe libcomposite`, root/ttboxkm, vendor versions use `Restart=on-failure` or `no`.
- Web: nginx dedicated unit, network-online dependency, preflight/setup, restart limit.
- Control plane: root backend, network-online dependency, `Restart=always`, `RestartSec=1`, `/opt/autobl`.

TTBOX `Supervisor` owns ordering and calls the existing `RuntimeController`; it does not implement Core. `HealthMonitor` aggregates Runtime and service status. Systemd calls are real when executed on Linux/RK3588; Windows tests use only `MockServiceAdapter` and are not device evidence.
