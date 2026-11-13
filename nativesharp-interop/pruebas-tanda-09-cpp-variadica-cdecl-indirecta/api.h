#pragma once
#ifdef API_DLL_IMPORT
#define API_STORAGE __declspec(dllimport)
#else
#define API_STORAGE
#endif
namespace T09Cpp { using Callback = int (*)(int,...); API_STORAGE int sum(int count,...); API_STORAGE int sum(int a,int b); API_STORAGE Callback fetchCallback(); }
