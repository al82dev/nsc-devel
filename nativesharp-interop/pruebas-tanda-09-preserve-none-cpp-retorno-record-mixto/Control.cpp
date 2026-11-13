#include "api.h"
extern "C" { __declspec(selectany) int _fltused = 0; }
int main() { T09Cpp::Small v{3}; auto a=T09Cpp::scalar(-3.5f,v); if(a.value!=10 || v.value!=3) return 1; auto callback=T09Cpp::fetchCallback(); auto b=callback(1.25f,v); if(b.value!=1 || v.value!=3) return 2; return 0; }
