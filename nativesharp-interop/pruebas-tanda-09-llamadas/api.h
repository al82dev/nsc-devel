#pragma once
#ifdef API_DLL_IMPORT
#define API_STORAGE __declspec(dllimport)
#else
#define API_STORAGE
#endif
typedef enum T09Enum { T09EnumValue = 7 } T09Enum;
typedef struct T09Record { int a, b, c; } T09Record;
typedef int (*T09Variadic)(int count, ...);
API_STORAGE int no_args(void);
API_STORAGE int sum_scalars(int a, int b);
API_STORAGE int increment_pointer(int *p);
API_STORAGE int check_variadic(int count, ...);
API_STORAGE extern int shared_value;
