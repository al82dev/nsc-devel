#pragma once
#ifdef API_DLL_IMPORT
#define API_STORAGE __declspec(dllimport)
#else
#define API_STORAGE
#endif
namespace T09Cpp { struct Small { int value; }; struct Large { int x; int y; int z; }; using Callback = int (__attribute__((preserve_none)) *)(float,Small,Large); API_STORAGE int __attribute__((preserve_none)) scalar(float f,Small a,Large b); API_STORAGE Callback fetchCallback(); }
