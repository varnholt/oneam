#pragma once
#ifdef __EMSCRIPTEN__
namespace oneam
{
extern "C" void web_choose_book();
extern "C" void web_storage_changed();
extern "C" void web_ready();
}  // namespace oneam
#endif
