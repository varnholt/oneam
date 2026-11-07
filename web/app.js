(() => {
  'use strict';
  const canvas = document.getElementById('canvas');
  const status = document.getElementById('status');
  const fileInput = document.getElementById('file');
  let ready = false;
  let retainedBytes = 0;
  const fail = message => {
    status.textContent = message;
    document.getElementById('loading').classList.remove('hidden');
  };
  globalThis.Module = {
    canvas,
    print: message => console.info(message),
    printErr: message => console.warn(message),
    setStatus: () => {},
    onAbort: () => fail('The reader could not start. Reload this page or download the app.'),
    onExit: code => { if (code) fail('This browser could not start the reader. Try the desktop app.'); },
  };
  const resize = () => {
    if (!ready) return;
    const box = document.getElementById('reader').getBoundingClientRect();
    Module.ccall('oneam_web_resize', null, ['number', 'number'], [Math.floor(box.width), Math.floor(box.height)]);
  };
  globalThis.oneamReady = () => {
    ready = true;
    document.getElementById('loading').classList.add('hidden');
    document.getElementById('open').disabled = false;
    document.getElementById('sample').disabled = false;
    requestAnimationFrame(resize);
    canvas.focus();
  };
  globalThis.oneamStorageError = () => {
    document.getElementById('privacy').textContent = 'Local files. No uploads. Browser storage is unavailable.';
  };
  globalThis.oneamChooseFile = () => fileInput.click();
  globalThis.oneamOpenFile = async file => {
    if (!ready || !file) return;
    if (file.size > 256 * 1024 * 1024) {
      alert('The browser preview supports comics up to 256 MiB. Use the desktop app for larger books.');
      return;
    }
    const button = document.getElementById('open');
    button.disabled = true;
    try {
      const bytes = new Uint8Array(await file.arrayBuffer());
      const digest = new Uint8Array(await crypto.subtle.digest('SHA-256', bytes));
      const hash = Array.from(digest, value => value.toString(16).padStart(2, '0')).join('');
      const name = file.name.replace(/[\\/\x00-\x1f]/g, '_').slice(-240) || 'comic.cbz';
      const directory = `/books/${hash}`;
      const path = `${directory}/${name}`;
      Module.FS.mkdirTree(directory);
      if (!Module.FS.analyzePath(path).exists) {
        if (retainedBytes + bytes.length > 512 * 1024 * 1024) {
          alert('This tab is holding 512 MiB of comics. Reload it to open more; your reading preferences will be kept.');
          return;
        }
        Module.FS.writeFile(path, bytes, { canOwn: true });
        retainedBytes += bytes.length;
      }
      // The same content receives a stable fingerprint even after reselecting it.
      Module.FS.utime(path, new Date(1000), new Date(1000));
      Module.ccall('oneam_web_open', null, ['string'], [path]);
      canvas.focus();
    } catch (error) {
      console.error(error);
      alert('This file could not be opened in the browser. Try the desktop app.');
    } finally { button.disabled = false; fileInput.value = ''; }
  };
  document.getElementById('open').addEventListener('click', oneamChooseFile);
  fileInput.addEventListener('change', () => oneamOpenFile(fileInput.files[0]));
  document.getElementById('sample').addEventListener('click', async () => {
    await document.fonts.ready;
    await oneamOpenFile(await oneamSample());
  });
  canvas.addEventListener('contextmenu', event => event.preventDefault());
  document.body.addEventListener('dragover', event => event.preventDefault());
  document.body.addEventListener('drop', event => {
    event.preventDefault();
    if (event.dataTransfer.files.length) oneamOpenFile(event.dataTransfer.files[0]);
  });
  new ResizeObserver(resize).observe(document.getElementById('reader'));
  async function start() {
    if (!crossOriginIsolated) {
      if (!('serviceWorker' in navigator)) {
        fail('This browser cannot run the preview. You can still download the app.'); return;
      }
      try {
        await navigator.serviceWorker.register('isolation-worker.js', { scope: './' });
        await navigator.serviceWorker.ready;
        if (sessionStorage.getItem('oneam-isolation-reload') === '1' && navigator.serviceWorker.controller) {
          fail('The preview could not start in this browser. Try a private window or download the app.'); return;
        }
        const reload = () => { sessionStorage.setItem('oneam-isolation-reload', '1'); location.reload(); };
        if (navigator.serviceWorker.controller) reload();
        else navigator.serviceWorker.addEventListener('controllerchange', reload, { once: true });
        return;
      } catch (error) {
        console.error(error); fail('The preview could not start. Reload this page or download the app.'); return;
      }
    }
    sessionStorage.removeItem('oneam-isolation-reload');
    const script = document.createElement('script');
    script.src = 'oneam.js'; script.onerror = () => fail('The reader could not be downloaded. Please reload.');
    document.body.appendChild(script);
  }
  start();
})();
