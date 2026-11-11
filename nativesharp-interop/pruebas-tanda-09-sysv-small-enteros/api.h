#pragma once
#ifdef API_DLL_IMPORT
#define API_STORAGE __declspec(dllimport)
#else
#define API_STORAGE
#endif
typedef struct T09SysvSmallInts { unsigned char x, y, z, w; } T09SysvSmallInts;
API_STORAGE __attribute__((sysv_abi)) T09SysvSmallInts sysv_small_ints(T09SysvSmallInts value, unsigned int a, long long b, unsigned long long c);
