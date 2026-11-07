#include "web_bridge.hpp"
#include <emscripten.h>
#include <emscripten/heap.h>
#include <sys/sysinfo.h>
#include <cerrno>

// The archive backend asks for a memory limit through this POSIX API, which the
// browser runtime declares but does not implement. Report the configured Wasm
// heap ceiling, rather than pretending to know the device's physical RAM.
extern "C" int sysinfo(struct sysinfo* info)
{
   if (!info)
   {
      errno = EFAULT;
      return -1;
   }
   *info = {};
   info->mem_unit = 1;
   info->totalram = emscripten_get_heap_max();
   info->freeram = info->totalram - emscripten_get_heap_size();
   info->procs = 1;
   return 0;
}

namespace oneam
{
void web_choose_book()
{
   MAIN_THREAD_EM_ASM({
      if (globalThis.oneamChooseFile)
         globalThis.oneamChooseFile();
   });
}
void web_storage_changed()
{
   MAIN_THREAD_EM_ASM({
      if (Module.oneamScheduleSync)
         Module.oneamScheduleSync();
   });
}
void web_ready()
{
   MAIN_THREAD_EM_ASM({
      if (globalThis.oneamReady)
         globalThis.oneamReady();
   });
}
}  // namespace oneam
