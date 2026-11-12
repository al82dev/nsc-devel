#pragma once
#ifdef API_DLL_IMPORT
#define API_STORAGE __declspec(dllimport)
#else
#define API_STORAGE
#endif
namespace T09Cpp {
struct Small { int x,y; };
using Callback = Small (__attribute__((preserve_most)) *)(Small);
API_STORAGE Small __attribute__((preserve_most)) most(Small);
API_STORAGE Callback fetchCallback();
}
