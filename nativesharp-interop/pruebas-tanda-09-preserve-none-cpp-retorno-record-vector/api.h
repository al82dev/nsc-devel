#pragma once
#ifdef API_DLL_IMPORT
#define API_STORAGE __declspec(dllimport)
#else
#define API_STORAGE
#endif
using Float4 = float __attribute__((ext_vector_type(4)));
namespace T09Cpp { struct Small { Float4 values; double wide; }; using Callback = Small (__attribute__((preserve_none)) *)(float,Small); API_STORAGE Small __attribute__((preserve_none)) scalar(float x,Small p); API_STORAGE Callback fetchCallback(); }
extern "C" { API_STORAGE void t09_init(T09Cpp::Small* p); API_STORAGE int t09_check(T09Cpp::Small* p,int n); }
