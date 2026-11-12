#pragma once
#ifdef __cplusplus
typedef bool T09Bool;
#else
typedef _Bool T09Bool;
#endif
#ifdef API_DLL_IMPORT
#define API __declspec(dllimport)
#else
#define API
#endif
typedef struct T09Inner { float x, y; } T09Inner;
typedef struct T09Nested { T09Inner pair; float z; } T09Nested;
typedef struct T09Array { float values[3]; } T09Array;
typedef T09Nested (__attribute__((regcall)) *T09NestedFn)(T09Nested, signed char, unsigned char, short, unsigned short, T09Bool);
typedef T09Array (__attribute__((regcall)) *T09ArrayFn)(T09Array, signed char, unsigned char, short, unsigned short, T09Bool);
API T09Nested __attribute__((regcall)) t09_nested(T09Nested value, signed char delta, unsigned char factor, short extra, unsigned short fourth, T09Bool last);
API T09Array __attribute__((regcall)) t09_array(T09Array value, signed char delta, unsigned char factor, short extra, unsigned short fourth, T09Bool last);
API T09NestedFn t09_get_nested(void);
API T09ArrayFn t09_get_array(void);
