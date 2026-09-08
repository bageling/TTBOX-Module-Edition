"""TTBOX 安装到 RK3588 时使用的最小 systemd unit 模板；不包含 Core 实现。"""
CORE_UNIT="""[Unit]
Description=TTBOX Core Runtime
After=network.target
Wants=network.target

[Service]
Type=simple
User=ttbox
Group=ttbox
WorkingDirectory=/opt/ttbox/core/current
ExecStart=/opt/ttbox/core/current/ttbox-core
ExecReload=/bin/kill -HUP $MAINPID
Restart=always
RestartSec=5
StandardOutput=journal
StandardError=journal
Environment=TTBOX_CONFIG_PATH=/opt/ttbox/config/current
NoNewPrivileges=true
PrivateTmp=true
ProtectSystem=strict
ProtectHome=true
ReadWritePaths=/opt/ttbox/core /opt/ttbox/models /opt/ttbox/config /opt/ttbox/data /opt/ttbox/logs /run/ttbox
RuntimeDirectory=ttbox
RuntimeDirectoryMode=0755

[Install]
WantedBy=multi-user.target
"""
SUPERVISOR_UNIT="""[Unit]
Description=TTBOX Platform Supervisor
After=network-online.target ttbox-core.service
Wants=network-online.target

[Service]
Type=simple
User=root
Group=root
WorkingDirectory=/opt/ttbox/supervisor
ExecStart=/usr/bin/python3 /opt/ttbox/supervisor/runner.py
Restart=always
RestartSec=2
StandardOutput=journal
StandardError=journal

[Install]
WantedBy=multi-user.target
"""
