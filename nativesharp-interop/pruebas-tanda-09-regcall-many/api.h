#pragma once
#ifdef API_DLL_IMPORT
#define API __declspec(dllimport)
#else
#define API
#endif
typedef struct T09Inner { float x, y; } T09Inner;
typedef struct T09Nested { T09Inner pair; float z; } T09Nested;
typedef struct T09Array { float values[3]; } T09Array;
typedef T09Nested (__attribute__((regcall)) *T09NestedFn)(T09Nested, int,int,int,int,int,int,int,int);
typedef T09Array (__attribute__((regcall)) *T09ArrayFn)(T09Array, int,int,int,int,int,int,int,int);
API T09Nested __attribute__((regcall)) t09_nested(T09Nested value, int b, int c, int d, int e, int f, int g, int h, int i);
API T09Array __attribute__((regcall)) t09_array(T09Array value, int b, int c, int d, int e, int f, int g, int h, int i);
API T09NestedFn t09_get_nested(void);
API T09ArrayFn t09_get_array(void);
