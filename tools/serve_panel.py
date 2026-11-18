from http.server import SimpleHTTPRequestHandler, ThreadingHTTPServer
from urllib.parse import urlsplit


class DisplayHandler(SimpleHTTPRequestHandler):
    def __init__(self, *args, **kwargs):
        super().__init__(*args, directory="/frames", **kwargs)

    def send_head(self):
        path = urlsplit(self.path).path
        if path not in {"/", "/index.html", "/panel.svg"}:
            self.send_error(404)
            return None
        self.path = "/index.html" if path == "/" else path
        return super().send_head()

    def end_headers(self):
        self.send_header("Cache-Control", "no-store")
        super().end_headers()


if __name__ == "__main__":
    with ThreadingHTTPServer(("0.0.0.0", 8080), DisplayHandler) as server:
        server.serve_forever()
