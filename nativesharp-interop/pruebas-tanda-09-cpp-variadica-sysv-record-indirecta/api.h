#pragma once
#ifdef API_DLL_IMPORT
#define API_STORAGE __declspec(dllimport)
#else
#define API_STORAGE
#endif
struct T09VarValue { double x; int y; };
namespace T09Cpp { using Callback = int (__attribute__((sysv_abi)) *)(int,...); API_STORAGE int __attribute__((sysv_abi)) sum(int count,...); API_STORAGE int sum(int a,int b); API_STORAGE Callback fetchCallback(); }
