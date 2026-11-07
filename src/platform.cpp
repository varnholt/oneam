#include "reader.hpp"
#ifdef SDL_PLATFORM_ANDROID
#include <jni.h>
#endif

namespace oneam
{
std::string display_name(const std::string& source)
{
#ifdef SDL_PLATFORM_ANDROID
   if (source.starts_with("content://"))
   {
      auto* env = static_cast<JNIEnv*>(SDL_GetAndroidJNIEnv());
      auto activity = static_cast<jobject>(SDL_GetAndroidActivity());
      if (env && activity)
      {
         auto cls = env->GetObjectClass(activity);
         auto method = env->GetMethodID(cls, "documentName", "(Ljava/lang/String;)[B");
         auto uri = env->NewStringUTF(source.c_str());
         jbyteArray bytes = nullptr;
         if (method && uri)
            bytes = static_cast<jbyteArray>(env->CallObjectMethod(activity, method, uri));
         if (env->ExceptionCheck())
         {
            env->ExceptionClear();
            bytes = nullptr;
         }
         std::string name;
         if (bytes)
         {
            const auto count = env->GetArrayLength(bytes);
            if (count > 0 && count <= 4096)
            {
               name.resize(static_cast<std::size_t>(count));
               env->GetByteArrayRegion(bytes, 0, count, reinterpret_cast<jbyte*>(name.data()));
            }
            env->DeleteLocalRef(bytes);
         }
         if (uri)
            env->DeleteLocalRef(uri);
         env->DeleteLocalRef(cls);
         env->DeleteLocalRef(activity);
         if (!name.empty())
            return name;
      }
   }
#endif
   const auto slash = source.find_last_of("/\\");
   return source.substr(slash == source.npos ? 0 : slash + 1);
}
}  // namespace oneam
