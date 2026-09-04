"""Preview process plugin：latest-frame 快照、JPEG/MJPEG HTTP 输出。"""
from __future__ import annotations
import base64
import json
import os
import socket
import sys
import threading
import time
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from preview_contract import PreviewConfig, PreviewFrame, PreviewStatus, PixelFormat, now_us

class MemoryFrameSource:
    """测试和进程边界使用的 latest-frame 源；覆盖旧帧，不形成队列。"""
    def __init__(self): self._lock=threading.Lock(); self._frame=None
    def publish(self, data, frame_number, width, height, pixel_format="jpeg", timestamp_us=None):
        frame=PreviewFrame(bytes(data), frame_number, timestamp_us or now_us(), width, height, pixel_format)
        with self._lock: self._frame=frame
    def latest(self):
        with self._lock: return self._frame

class CoreIpcFrameSource:
    """通过既有 GET_PREVIEW IPC 获取 JPEG；不暴露 DMA-BUF 或 Core buffer。"""
    def __init__(self, socket_path=None): self.socket_path=socket_path or os.environ.get("TTBOX_IPC_SOCKET", "/tmp/ttbox_core.sock")
    def latest(self):
        if self.socket_path.startswith("tcp:"): return None
        try:
            sock=socket.socket(socket.AF_UNIX, socket.SOCK_STREAM); sock.settimeout(2); sock.connect(self.socket_path)
            sock.sendall(b'{"type":"GET_PREVIEW"}\n'); chunks=[]
            while True:
                chunk=sock.recv(65536)
                if not chunk: break
                chunks.append(chunk)
                if b"\n" in chunk: break
            sock.close(); response=json.loads(b"".join(chunks).split(b"\n",1)[0]); data=response.get("data", {})
            raw=base64.b64decode(data["jpeg_base64"]) if data.get("jpeg_base64") else b""
            if not raw: return None
            return PreviewFrame(raw, int(data.get("seq",0)), now_us(), 0, 0, PixelFormat.JPEG.value)
        except Exception: return None

class PreviewService:
    def __init__(self, source, config=None):
        self.source=source; self.config=config or PreviewConfig(); self.config.validate(); self.enabled=True; self.running=False; self._last=None; self._lock=threading.Lock(); self._dropped=0; self._no_frame_count=0; self._last_pub_seq=-1
    def start(self): self.enabled=True; self.running=True
    def stop(self): self.running=False
    def snapshot(self):
        frame=self.source.latest()
        if frame is None:
            # core 停止/无帧：立即清空缓存断流，防浏览器显示陈旧画框
            self._no_frame_count += 1
            with self._lock:
                self._last=None
                self._last_pub_seq=-1
            return None
        self._no_frame_count = 0
        with self._lock:
            # core 重启 seq 归零：重置已推送序号，强制重新推帧
            if frame.frame_number < self._last_pub_seq:
                self._last_pub_seq=-1
            self._last=frame
            self._last_pub_seq=frame.frame_number
        return frame
    def status(self):
        frame=self.snapshot(); return {"enabled":self.enabled,"running":self.running,"health":"healthy" if frame else "unknown","frame_number":frame.frame_number if frame else 0,"timestamp_us":frame.timestamp_us if frame else 0,"width":frame.width if frame else 0,"height":frame.height if frame else 0,"pixel_format":frame.pixel_format if frame else "jpeg","source":frame.source if frame else "core_ipc","fps":self.config.fps,"dropped":self._dropped}

class PreviewHandler(BaseHTTPRequestHandler):
    service=None
    def log_message(self, *_): pass
    def do_GET(self):
        if self.path == "/api/preview.jpg":
            frame=self.service.snapshot()
            if not frame: self.send_error(404,"暂无预览帧"); return
            self.send_response(200); self.send_header("Content-Type","image/jpeg"); self.send_header("Content-Length",str(len(frame.data))); self.end_headers(); self.wfile.write(frame.data); return
        if self.path == "/api/preview/status":
            body=json.dumps(self.service.status(),ensure_ascii=False).encode(); self.send_response(200); self.send_header("Content-Type","application/json"); self.send_header("Content-Length",str(len(body))); self.end_headers(); self.wfile.write(body); return
        if self.path == "/api/preview.mjpg":
            # MJPEG 流：持续推送新帧（不设 Content-Length，保持连接不断开）。
            self.send_response(200)
            self.send_header("Content-Type", "multipart/x-mixed-replace; boundary=ttboxframe")
            self.send_header("Cache-Control", "no-store")
            self.send_header("Connection", "keep-alive")
            self.end_headers()
            last_seq = -1
            try:
                while True:
                    frame = self.service.snapshot()
                    if frame and frame.frame_number != last_seq:
                        last_seq = frame.frame_number
                        self.wfile.write(b"--ttboxframe\r\n")
                        self.wfile.write(b"Content-Type: image/jpeg\r\n")
                        self.wfile.write(b"Content-Length: " + str(len(frame.data)).encode() + b"\r\n\r\n")
                        self.wfile.write(frame.data)
                        self.wfile.write(b"\r\n")
                        self.wfile.flush()
                    else:
                        time.sleep(0.05)
            except (BrokenPipeError, ConnectionResetError, OSError):
                pass
            return
        self.send_error(404)

def create_server(host="127.0.0.1", port=8082, service=None):
    service=service or PreviewService(CoreIpcFrameSource()); PreviewHandler.service=service
    return ThreadingHTTPServer((host, port), PreviewHandler)

def main():
    service=PreviewService(CoreIpcFrameSource()); service.start(); server=create_server(os.environ.get("TTBOX_PREVIEW_HOST","0.0.0.0"),int(os.environ.get("TTBOX_PREVIEW_PORT","8082")),service); server.serve_forever()
if __name__ == "__main__": main()
