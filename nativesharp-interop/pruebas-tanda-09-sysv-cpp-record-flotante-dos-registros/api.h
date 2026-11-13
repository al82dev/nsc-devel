#pragma once
#ifdef API_DLL_IMPORT
#define API_STORAGE __declspec(dllimport)
#else
#define API_STORAGE
#endif
namespace T09Cpp {
struct Small { float x,y,z; };
struct Other { int x,y,z,w,v; };
using Callback = int (__attribute__((sysv_abi)) *)(Small, int, Other);
API_STORAGE int __attribute__((sysv_abi)) most(Small, int, Other);
API_STORAGE Callback fetchCallback();
}
