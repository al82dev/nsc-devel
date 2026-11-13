#pragma once
#ifdef API_DLL_IMPORT
#define API_STORAGE __declspec(dllimport)
#else
#define API_STORAGE
#endif
namespace T09Cpp { enum class Mode : unsigned short { Chosen=65535, Changed=11 }; using Callback = int (*)(int,...); API_STORAGE int sum(int count,...); API_STORAGE int sum(int a,int b); API_STORAGE Callback fetchCallback(); }
