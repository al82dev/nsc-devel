#include "api.h"
extern "C" { __declspec(selectany) int _fltused = 0; }
namespace T09Cpp { Small __attribute__((preserve_none)) scalar(float x,Small p) { p.values[0] += x == -3.5f ? 7 : -2; p.values[2] += 1; p.wide += 2.0; return p; } Callback fetchCallback() { return scalar; } }
extern "C" void t09_init(T09Cpp::Small* p) { p->values[0]=0; p->values[1]=0; p->values[2]=0; p->values[3]=0; p->wide=1.5; }
extern "C" int t09_check(T09Cpp::Small* p,int n) { return p->values[0]==n && p->values[1]==4 && p->values[2]==(n==3 ? 7 : 8) && p->values[3]==6 && p->wide==(n==3 ? 1.5 : 3.5); }
