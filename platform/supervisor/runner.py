#!/usr/bin/env python3
import sys,time,json,signal
sys.path.insert(0,"/opt/ttbox/supervisor")
from supervisor.supervisor import Supervisor
from supervisor.systemd_adapter import SystemdServiceAdapter,SystemdProcessAdapter
from runtime.controller import RuntimeController
from health.monitor import HealthMonitor
def main():
 svc=SystemdServiceAdapter(["sudo","-n","systemctl"]); runtime=RuntimeController(SystemdProcessAdapter(svc,"ttbox-core")); health=HealthMonitor(runtime,svc,("ttbox-core",)); sup=Supervisor(runtime,svc,health,"ttbox-core",())
 def stop(sig,frame): sup.stop(); raise SystemExit(0)
 signal.signal(signal.SIGTERM,stop); signal.signal(signal.SIGINT,stop)
 sup.start(); st=sup.status(); print(json.dumps({"state":st.runtime.state,"pid":st.runtime.pid,"health":st.health.ok}),flush=True)
 while True:
  h=health.check(); st=sup.status()
  if not h.ok and st.runtime.state in ("FAILED","STOPPED","STOPPING"): sup.recover()
  time.sleep(5)
if __name__=="__main__": main()
