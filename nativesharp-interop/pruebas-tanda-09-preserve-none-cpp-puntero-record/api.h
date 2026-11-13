#pragma once
#ifdef API_DLL_IMPORT
#define API_STORAGE __declspec(dllimport)
#else
#define API_STORAGE
#endif
namespace T09Cpp { struct Small { int value; }; using Callback = int (__attribute__((preserve_none)) *)(float,Small*); API_STORAGE int __attribute__((preserve_none)) scalar(float x,Small* p); API_STORAGE Callback fetchCallback(); }
