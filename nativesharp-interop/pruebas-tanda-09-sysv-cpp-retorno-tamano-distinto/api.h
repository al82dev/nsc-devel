#pragma once
#ifdef API_DLL_IMPORT
#define API_STORAGE __declspec(dllimport)
#else
#define API_STORAGE
#endif
namespace T09Cpp {
struct Small { float x,y; int z; };
struct Other { double x; int y; };
using Callback = Small (__attribute__((sysv_abi)) *)(Other);
API_STORAGE Small __attribute__((sysv_abi)) most(Other);
API_STORAGE Callback fetchCallback();
}
