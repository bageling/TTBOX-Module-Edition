"""Model Plugin 进程入口占位：生产模型推理仍由 Core ModelAdapter/RKNNEngine 承担。"""
from pathlib import Path
import os
from model_service import ModelPluginService

def main():
    root = Path(os.environ.get("TTBOX_MODELS_ROOT", "/opt/ttbox/models"))
    service = ModelPluginService(root)
    print(f"TTBOX Model Plugin ready: {root}", flush=True)
    import signal
    signal.pause()

if __name__ == "__main__": main()
