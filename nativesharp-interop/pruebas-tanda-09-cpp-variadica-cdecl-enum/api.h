#pragma once
#ifdef API_DLL_IMPORT
#define API_STORAGE __declspec(dllimport)
#else
#define API_STORAGE
#endif
enum Mode : unsigned short { Chosen=65535, Changed=11 };
namespace T09Cpp { API_STORAGE int sum(int count,...); API_STORAGE int sum(int a,int b); }
