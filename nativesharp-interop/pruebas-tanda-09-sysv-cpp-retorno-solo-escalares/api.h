#pragma once
#ifdef API_DLL_IMPORT
#define API_STORAGE __declspec(dllimport)
#else
#define API_STORAGE
#endif
namespace T09Cpp {
struct Small { float x,y; int z; };
struct Other { float x,y; int z; };
using Callback = Small (__attribute__((sysv_abi)) *)(int, float);
API_STORAGE Small __attribute__((sysv_abi)) most(int, float);
API_STORAGE Callback fetchCallback();
}
