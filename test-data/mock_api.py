"""Local-only fixed mock; Python standard library, no dependencies."""
import json
from functools import partial
from http.server import SimpleHTTPRequestHandler, ThreadingHTTPServer
from pathlib import Path
from urllib.parse import urlsplit

ROOT = Path(__file__).resolve().parents[1]

class Handler(SimpleHTTPRequestHandler):
    def do_GET(self):
        route = urlsplit(self.path).path
        if route == "/api/devices":
            data = [{"device_id": "motor_01", "online": False, "last_seen": None}]
        elif route == "/api/devices/motor_01/latest":
            data = json.loads((ROOT / "test-data/features.sample.json").read_text(encoding="utf-8"))
        elif route.startswith("/api/"):
            self.reply({"detail": "Mock route not implemented"}, 404)
            return
        elif route == "/" or route.startswith("/dashboard/"):
            if route == "/":
                self.path = "/dashboard/"
            # Limit static files to dashboard; do not serve local credentials.
            target = (ROOT / urlsplit(self.path).path.lstrip("/")).resolve()
            if not target.is_relative_to(ROOT / "dashboard"):
                self.reply({"detail": "Not found"}, 404)
                return
            super().do_GET()
            return
        else:
            self.reply({"detail": "Not found"}, 404)
            return
        self.reply(data)

    def reply(self, data, status=200):
        content = json.dumps(data).encode("utf-8")
        self.send_response(status)
        self.send_header("Content-Type", "application/json; charset=utf-8")
        self.send_header("Content-Length", str(len(content)))
        self.end_headers()
        self.wfile.write(content)

if __name__ == "__main__":
    server = ThreadingHTTPServer(("127.0.0.1", 8000), partial(Handler, directory=str(ROOT)))
    print("Fixed MOCK: https://edge-iot-project-1.onrender.com (Ctrl+C to stop)", flush=True)
    try:
        server.serve_forever()
    except KeyboardInterrupt:
        pass
    finally:
        server.server_close()
