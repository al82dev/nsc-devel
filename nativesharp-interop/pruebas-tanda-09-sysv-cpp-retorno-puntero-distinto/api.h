#pragma once
#ifdef API_DLL_IMPORT
#define API_STORAGE __declspec(dllimport)
#else
#define API_STORAGE
#endif
namespace T09Cpp {
struct Small { int x,y,z,w,v,u; };
struct Extra { int x,y,z,w,v,u; };
struct Other { int x,y,z,w,v; };
using Callback = Extra* (__attribute__((sysv_abi)) *)(Small, Extra*, Other);
API_STORAGE Extra* __attribute__((sysv_abi)) most(Small, Extra*, Other);
API_STORAGE Callback fetchCallback();
}
