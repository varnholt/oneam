// Persist preferences only. Imported comic archives remain in this tab's memory.
if (!ENVIRONMENT_IS_PTHREAD) {
  Module.preRun = Module.preRun || [];
  Module.preRun.push(() => {
    FS.mkdirTree('/libsdl');
    FS.mount(IDBFS, {}, '/libsdl');
    addRunDependency('oneam-preferences');
    FS.syncfs(true, error => {
      if (error) globalThis.oneamStorageError?.();
      removeRunDependency('oneam-preferences');
    });
  });
  let timer, syncing = false, queued = false;
  const sync = () => {
    if (syncing) { queued = true; return; }
    syncing = true;
    FS.syncfs(false, error => {
      syncing = false;
      if (error) globalThis.oneamStorageError?.();
      if (queued) { queued = false; sync(); }
    });
  };
  Module.oneamScheduleSync = () => { clearTimeout(timer); timer = setTimeout(sync, 200); };
  if (typeof document !== 'undefined') {
    document.addEventListener('visibilitychange', () => {
      if (document.hidden) { clearTimeout(timer); sync(); }
    });
  }
}
