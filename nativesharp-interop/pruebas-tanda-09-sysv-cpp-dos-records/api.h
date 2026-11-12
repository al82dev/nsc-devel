#pragma once
#ifdef API_DLL_IMPORT
#define API_STORAGE __declspec(dllimport)
#else
#define API_STORAGE
#endif
namespace T09Cpp {
struct Small { int x,y,z,w,v,u; };
using Callback = Small (__attribute__((sysv_abi)) *)(Small, int, Small);
API_STORAGE Small __attribute__((sysv_abi)) most(Small, int, Small);
API_STORAGE Callback fetchCallback();
}
