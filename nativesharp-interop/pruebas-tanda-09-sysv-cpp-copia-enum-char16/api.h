#pragma once
#ifdef API_DLL_IMPORT
#define API_STORAGE __declspec(dllimport)
#else
#define API_STORAGE
#endif
enum Mode : char16_t { Chosen=65535, Changed=11 };
namespace T09Cpp {
struct Small { float x,y; int z; };
struct Other { Mode x; double y; int z; };
using Callback = Small (__attribute__((sysv_abi)) *)(Other);
API_STORAGE Small __attribute__((sysv_abi)) most(Other);
API_STORAGE Callback fetchCallback();
}
