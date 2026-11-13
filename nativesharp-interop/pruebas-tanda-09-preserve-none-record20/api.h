#pragma once
#ifdef API_DLL_IMPORT
#define API_STORAGE __declspec(dllimport)
#else
#define API_STORAGE
#endif
namespace T09Cpp {
struct Small { int x,y,z,w,u; };
static_assert(sizeof(Small) == 20 && alignof(Small) == 4);
using Callback = Small (__attribute__((preserve_none)) *)(Small);
API_STORAGE Small __attribute__((preserve_none)) most(Small);
API_STORAGE Callback fetchCallback();
}
