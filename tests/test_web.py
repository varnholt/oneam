"""Browser integration check. Playwright is a development-only test dependency."""
import argparse
import functools
import importlib.util
import json
import pathlib
import threading
import http.server
from playwright.sync_api import sync_playwright


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('directory', type=pathlib.Path)
    parser.add_argument('--browser')
    parser.add_argument('--comic', type=pathlib.Path)
    parser.add_argument('--screenshot', type=pathlib.Path)
    parser.add_argument('--without-headers', action='store_true')
    args = parser.parse_args()
    spec = importlib.util.spec_from_file_location('serve_web', pathlib.Path(__file__).parents[1] / 'scripts/serve_web.py')
    serve = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(serve)
    handler = http.server.SimpleHTTPRequestHandler if args.without_headers else serve.Handler
    server = http.server.ThreadingHTTPServer(('127.0.0.1', 0), functools.partial(handler, directory=str(args.directory)))
    threading.Thread(target=server.serve_forever, daemon=True).start()
    errors, network = [], []
    try:
        with sync_playwright() as playwright:
            browser = playwright.chromium.launch(headless=True, executable_path=args.browser,
                                                args=['--enable-unsafe-swiftshader'])
            context = browser.new_context(viewport={'width': 1200, 'height': 900})
            page = context.new_page()
            page.on('pageerror', lambda error: errors.append(str(error)))
            page.on('console', lambda message: print(f'browser {message.type}: {message.text}', flush=True)
                    if message.type in ('warning','error') else None)
            page.on('request', lambda request: network.append((request.method, request.url)))
            page.goto(f'http://127.0.0.1:{server.server_port}/', wait_until='domcontentloaded')
            page.wait_for_function("document.getElementById('loading').classList.contains('hidden')", timeout=90000)
            assert page.evaluate('crossOriginIsolated'), 'browser is not isolated for worker threads'
            page.locator('#sample').click()
            page.wait_for_function("Module.ccall('oneam_web_page','number',[],[]) === 1", timeout=30000)
            page.locator('#canvas').focus()
            page.keyboard.press('ArrowRight')
            page.wait_for_function("Module.ccall('oneam_web_page','number',[],[]) === 2")
            page.keyboard.press('r')
            page.keyboard.press('Equal')
            page.wait_for_function("Module.ccall('oneam_web_rotation','number',[],[]) === 1 && Module.ccall('oneam_web_zoom','number',[],[]) > 1.1")
            page.wait_for_timeout(1000)
            page.reload(wait_until='domcontentloaded')
            page.wait_for_function("document.getElementById('loading').classList.contains('hidden')", timeout=90000)
            page.locator('#sample').click()
            page.wait_for_function("Module.ccall('oneam_web_page','number',[],[]) === 2", timeout=30000)
            assert page.evaluate("Module.ccall('oneam_web_rotation','number',[],[])") == 1, 'rotation did not persist'
            assert page.evaluate("Module.ccall('oneam_web_zoom','number',[],[])") > 1.1, 'zoom did not persist'
            page.keyboard.press('r')
            page.keyboard.press('r')
            page.keyboard.press('r')
            page.keyboard.press('0')
            if args.comic:
                page.set_input_files('#file', args.comic)
                page.wait_for_function("Module.ccall('oneam_web_page','number',[],[]) === 1", timeout=30000)
            if args.screenshot:
                page.wait_for_timeout(500)
                page.screenshot(path=str(args.screenshot))
            assert not errors, f'Browser errors: {errors}'
            assert all(method == 'GET' for method, _ in network), 'the preview sent a network upload'
            print(json.dumps({'result': 'passed', 'requests': len(network), 'uploads': 0,
                              'isolation_service_worker': args.without_headers}))
            browser.close()
    finally:
        server.shutdown()


if __name__ == '__main__':
    main()
