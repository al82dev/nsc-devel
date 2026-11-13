#pragma once
#ifdef API_DLL_IMPORT
#define API_STORAGE __declspec(dllimport)
#else
#define API_STORAGE
#endif
namespace T09Cpp { API_STORAGE double __attribute__((preserve_none)) scalar(int x,float y,unsigned long long z,short w,double v); }
