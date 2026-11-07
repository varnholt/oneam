"""Serve a local WebAssembly preview with the required isolation headers."""
import argparse
import functools
import http.server


class Handler(http.server.SimpleHTTPRequestHandler):
    def end_headers(self):
        self.send_header('Cross-Origin-Opener-Policy', 'same-origin')
        self.send_header('Cross-Origin-Embedder-Policy', 'require-corp')
        self.send_header('Cross-Origin-Resource-Policy', 'same-origin')
        self.send_header('Cache-Control', 'no-cache')
        super().end_headers()


if __name__ == '__main__':
    parser = argparse.ArgumentParser()
    parser.add_argument('directory', nargs='?', default='build-web/web')
    parser.add_argument('--port', type=int, default=8080)
    args = parser.parse_args()
    server = http.server.ThreadingHTTPServer(('127.0.0.1', args.port), functools.partial(Handler, directory=args.directory))
    print(f'oneam preview: http://127.0.0.1:{args.port}', flush=True)
    server.serve_forever()
