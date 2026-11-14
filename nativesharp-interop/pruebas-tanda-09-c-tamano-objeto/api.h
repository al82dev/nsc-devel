#pragma once
#if defined(T09_EXPORT)
#define T09_STORAGE __declspec(dllexport)
#elif defined(T09_IMPORT)
#define T09_STORAGE __declspec(dllimport)
#else
#define T09_STORAGE
#endif
#define T09_SIZE(n) __attribute__((pass_object_size(n)))
#define T09_DYNAMIC(n) __attribute__((pass_dynamic_object_size(n)))
struct T09CArray { int values[2]; int tail[3]; };
T09_STORAGE int t09_s0(int *p T09_SIZE(0));
T09_STORAGE int t09_s1(int *p T09_SIZE(1));
T09_STORAGE int t09_s2(int *p T09_SIZE(2));
T09_STORAGE int t09_s3(int *p T09_SIZE(3));
T09_STORAGE int t09_d0(int *p T09_DYNAMIC(0));
T09_STORAGE int t09_d1(int *p T09_DYNAMIC(1));
T09_STORAGE int t09_d2(int *p T09_DYNAMIC(2));
T09_STORAGE int t09_d3(int *p T09_DYNAMIC(3));
T09_STORAGE int t09_narrow(int *p T09_SIZE(0), signed char a, unsigned short b, _Bool c);
