#pragma once
#if defined(T09_EXPORT)
#define T09_STORAGE __declspec(dllexport)
#elif defined(T09_IMPORT)
#define T09_STORAGE __declspec(dllimport)
#else
#define T09_STORAGE
#endif
T09_STORAGE __attribute__((sysv_abi)) int t09_csum(
    int *p __attribute__((pass_object_size(0))), signed char marker, _Bool flag, ...);
