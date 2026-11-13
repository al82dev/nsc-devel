#include "api.h"
extern "C" { __declspec(selectany) int _fltused = 0; }
int main() { T09Cpp::Small v{3,5,7,1.5f}; auto a=T09Cpp::scalar(-3.5f,v); if(a.value!=10 || a.other!=5 || a.third!=8 || a.fourth!=3.5f || v.value!=3 || v.third!=7 || v.fourth!=1.5f) return 1; auto callback=T09Cpp::fetchCallback(); auto b=callback(1.25f,v); if(b.value!=1 || b.other!=5 || b.third!=8 || b.fourth!=3.5f || v.value!=3 || v.third!=7 || v.fourth!=1.5f) return 2; return 0; }
