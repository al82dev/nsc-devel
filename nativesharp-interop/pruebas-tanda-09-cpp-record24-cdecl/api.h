#pragma once
#ifdef API_DLL_IMPORT
#define API_STORAGE __declspec(dllimport)
#else
#define API_STORAGE
#endif
namespace T09Cpp {
struct Small { int x,y,z,w,u,v; };
static_assert(sizeof(Small) == 24 && alignof(Small) == 4);
using Callback = Small (*)(Small);
API_STORAGE Small most(Small);
API_STORAGE Callback fetchCallback();
}
