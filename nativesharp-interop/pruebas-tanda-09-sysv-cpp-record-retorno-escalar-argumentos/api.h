#pragma once
#ifdef API_DLL_IMPORT
#define API_STORAGE __declspec(dllimport)
#else
#define API_STORAGE
#endif
namespace T09Cpp {
struct Small { int x,y,z,w,v,u; };
struct Other { int x,y,z,w,v; };
using Callback = int (__attribute__((sysv_abi)) *)(Small, signed char, unsigned char, short, unsigned short, bool, unsigned int, long long, unsigned long long, float, double, Other);
API_STORAGE int __attribute__((sysv_abi)) most(Small, signed char, unsigned char, short, unsigned short, bool, unsigned int, long long, unsigned long long, float, double, Other);
API_STORAGE Callback fetchCallback();
}
