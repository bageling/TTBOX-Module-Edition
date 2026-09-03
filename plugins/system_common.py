"""System/Hardware Plugin 通用适配器：复用系统接口，不触碰 Core 算法。"""
from __future__ import annotations
import json, os, shutil, socket, subprocess, time
from pathlib import Path

class CommandAdapter:
    def __init__(self, command_runner=None): self.command_runner=command_runner or self._run
    @staticmethod
    def _run(argv):
        try:
            result=subprocess.run(argv,capture_output=True,text=True,encoding="utf-8",errors="replace",timeout=10,check=False)
            return result.returncode,result.stdout.strip()
        except (OSError,subprocess.SubprocessError) as exc: return 1,str(exc)
    def command(self, argv): return self.command_runner(list(argv))

class FanService:
    def __init__(self, sys_root="/sys"): self.sys_root=Path(sys_root)
    def status(self):
        thermal=self.sys_root/"class/thermal"; zones=list(thermal.glob("thermal_zone*/temp")) if thermal.is_dir() else []
        temps=[]
        for p in zones:
            try: temps.append(int(p.read_text().strip())/1000)
            except (OSError,ValueError): pass
        pwm=list((self.sys_root/"class/hwmon").glob("hwmon*/pwm*")) if (self.sys_root/"class/hwmon").is_dir() else []
        return {"available":bool(pwm),"temperature_c":max(temps) if temps else None,"pwm":pwm[0].name if pwm else None,"mode":"auto"}
    def set_mode(self, mode):
        if mode not in ("auto","manual"): raise ValueError("fan mode must be auto or manual")
        return {"ok":False,"mode":mode,"reason":"hardware adapter not configured"}

class WifiService(CommandAdapter):
    def status(self):
        code,out=self.command(["nmcli","-t","-f","DEVICE,TYPE,STATE,CONNECTION","device"])
        connected=[]
        if code==0:
            for line in out.splitlines():
                parts=line.split(":")
                if len(parts)>=4 and parts[1]=="wifi" and parts[2]=="connected": connected.append({"device":parts[0],"ssid":parts[3]})
        return {"nmcli":code==0,"connected":connected,"wifi_connected":bool(connected)}
    def scan(self): return self.command(["nmcli","-t","-f","SSID,SIGNAL,SECURITY","device","wifi","list"])
    def connect(self, ssid, password=None):
        argv=["nmcli","device","wifi","connect",ssid]
        if password: argv += ["password",password]
        return self.command(argv)

class NetworkService(CommandAdapter):
    def status(self):
        code,hostname=self.command(["hostname"])
        code2,addresses=self.command(["hostname","-I"])
        return {"hostname":hostname,"addresses":addresses.split() if code2==0 else [],"available":code==0 and code2==0}

class MonitorService:
    def __init__(self, proc_root="/proc", sys_root="/sys", disk_path=None): self.proc_root=Path(proc_root); self.sys_root=Path(sys_root); self.disk_path=Path(disk_path or self.proc_root)
    def status(self): return {"cpu":self._cpu(),"memory":self._memory(),"disk":self._disk(),"temperature":self._temperature()}
    def _cpu(self):
        try: return {"raw":self.proc_root.joinpath("stat").read_text(encoding="utf-8").splitlines()[0]}
        except (OSError,IndexError): return {"available":False}
    def _memory(self):
        try: return {"raw":self.proc_root.joinpath("meminfo").read_text(encoding="utf-8").splitlines()[0]}
        except OSError: return {"available":False}
    def _disk(self):
        try: usage=shutil.disk_usage(self.disk_path); return {"total":usage.total,"free":usage.free,"used":usage.used}
        except OSError: return {"available":False}
    def _temperature(self): return {"available":False,"source":"sysfs"}

class SystemService(CommandAdapter):
    def status(self):
        code,kernel=self.command(["uname","-a"])
        return {"kernel":kernel.strip(),"hostname":socket.gethostname(),"uptime_seconds":self._uptime(),"available":code==0}
    def _uptime(self):
        try: return float(Path("/proc/uptime").read_text().split()[0])
        except (OSError,ValueError,IndexError): return None
    def reboot(self): return self.command(["systemctl","reboot"])
    def poweroff(self): return self.command(["systemctl","poweroff"])

class LogService:
    def __init__(self, path): self.path=Path(path)
    def read(self, limit=500):
        try: lines=self.path.read_text(encoding="utf-8",errors="replace").splitlines()
        except OSError: lines=[]
        return {"path":str(self.path),"lines":lines[-limit:]}
    def status(self):
        return {"available": self.path.is_file(), "path": str(self.path)}

class UpgradeService(CommandAdapter):
    def status(self): return {"state":"unavailable","source":"update_engine"}
    def check(self): return {"ok":True,"state":"deferred","reason":"repository not configured"}
    def install(self, package): return {"ok":False,"state":"deferred","reason":"offline upgrade adapter"}
