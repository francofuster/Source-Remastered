"""Test release server for the in-app updater (SetupActivity, Updater).

Serves the releases staged by Build-Android.ps1 -Stage the way GitHub does:

  /releases                 like api.github.com/repos/.../releases: one entry
                            per folder of the release folder, newest first
  /files/<tag>/<name>       the assets, with Range requests (206), as GitHub

Run it on the PC, then point the app at it:
  python Android/tools/update_server.py ../Build-Android/release
  phone:    adb reverse tcp:8000 tcp:8000, then --es updateFeed http://localhost:8000/releases
  emulator: --es updateFeed http://10.0.2.2:8000/releases

Options to test the hard cases:
  --kbps N        slow every download down to N KB/s
  --cut-after N   close the connection after N bytes of each file response
                  (a cut download: the app must resume where it stopped)
  --only TAG      list only this release (the others stay on disk)
"""
import argparse
import json
import os
import re
import sys
import time
from datetime import datetime, timezone
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
from urllib.parse import unquote

ARGS = None


def releases(host):
    root = ARGS.root
    tags = [d for d in os.listdir(root) if os.path.isdir(os.path.join(root, d))]
    if ARGS.only:
        tags = [t for t in tags if t == ARGS.only]
    tags.sort(key=lambda t: os.path.getmtime(os.path.join(root, t)), reverse=True)
    out = []
    for tag in tags:
        folder = os.path.join(root, tag)
        assets = []
        for name in sorted(os.listdir(folder)):
            path = os.path.join(folder, name)
            if os.path.isfile(path):
                assets.append({
                    'name': name,
                    'size': os.path.getsize(path),
                    'browser_download_url': 'http://%s/files/%s/%s' % (host, tag, name),
                })
        created = datetime.fromtimestamp(os.path.getmtime(folder), timezone.utc).strftime('%Y-%m-%dT%H:%M:%SZ')
        out.append({'tag_name': tag, 'draft': False, 'prerelease': True, 'created_at': created, 'assets': assets})
    return out


class Handler(BaseHTTPRequestHandler):
    protocol_version = 'HTTP/1.1'

    def log_message(self, fmt, *a):
        sys.stderr.write('%s %s\n' % (time.strftime('%H:%M:%S'), fmt % a))

    def do_GET(self):
        host = self.headers.get('Host', 'localhost:%d' % ARGS.port)
        path = unquote(self.path.split('?')[0])
        if path == '/releases':
            body = json.dumps(releases(host), indent=1).encode('utf8')
            self.send_response(200)
            self.send_header('Content-Type', 'application/json')
            self.send_header('Content-Length', str(len(body)))
            self.end_headers()
            self.wfile.write(body)
            return
        m = re.match(r'^/files/([^/]+)/([^/]+)$', path)
        if not m:
            self.send_error(404)
            return
        file = os.path.join(ARGS.root, m.group(1), m.group(2))
        if not os.path.isfile(file):
            self.send_error(404)
            return
        size = os.path.getsize(file)
        first, last = 0, size - 1
        partial = False
        rng = self.headers.get('Range')
        if rng:
            r = re.match(r'^bytes=(\d*)-(\d*)$', rng.strip())
            if not r or (r.group(1) == '' and r.group(2) == ''):
                self.send_error(416)
                return
            if r.group(1) == '':
                first = max(0, size - int(r.group(2)))
            else:
                first = int(r.group(1))
                if r.group(2) != '':
                    last = min(size - 1, int(r.group(2)))
            if first > last:
                self.send_response(416)
                self.send_header('Content-Range', 'bytes */%d' % size)
                self.send_header('Content-Length', '0')
                self.end_headers()
                return
            partial = True
        length = last - first + 1
        self.send_response(206 if partial else 200)
        self.send_header('Content-Type', 'application/octet-stream')
        self.send_header('Accept-Ranges', 'bytes')
        self.send_header('Content-Length', str(length))
        if partial:
            self.send_header('Content-Range', 'bytes %d-%d/%d' % (first, last, size))
        self.end_headers()
        sent = 0
        with open(file, 'rb') as f:
            f.seek(first)
            while sent < length:
                chunk = f.read(min(65536, length - sent))
                if not chunk:
                    break
                if ARGS.cut_after and sent + len(chunk) > ARGS.cut_after:
                    chunk = chunk[:max(0, ARGS.cut_after - sent)]
                    self.wfile.write(chunk)
                    self.wfile.flush()
                    sys.stderr.write('  cut after %d bytes\n' % (sent + len(chunk)))
                    self.close_connection = True
                    return
                try:
                    self.wfile.write(chunk)
                except (BrokenPipeError, ConnectionResetError):
                    return
                sent += len(chunk)
                if ARGS.kbps:
                    time.sleep(len(chunk) / (ARGS.kbps * 1024.0))


def main():
    global ARGS
    p = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    p.add_argument('root', help='the release folder (Build-Android/release)')
    p.add_argument('--port', type=int, default=8000)
    p.add_argument('--kbps', type=float, default=0)
    p.add_argument('--cut-after', type=int, default=0)
    p.add_argument('--only', default='')
    ARGS = p.parse_args()
    ARGS.root = os.path.abspath(ARGS.root)
    server = ThreadingHTTPServer(('0.0.0.0', ARGS.port), Handler)
    sys.stderr.write('Serving %s on port %d\n' % (ARGS.root, ARGS.port))
    server.serve_forever()


if __name__ == '__main__':
    main()
