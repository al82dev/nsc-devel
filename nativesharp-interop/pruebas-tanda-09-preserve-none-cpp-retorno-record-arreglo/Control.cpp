#include "api.h"
extern "C" { __declspec(selectany) int _fltused = 0; }
int main() { T09Cpp::Small v{{3,4,7},1.5}; auto a=T09Cpp::scalar(-3.5f,v); if(a.values[0]!=10 || a.values[1]!=4 || a.values[2]!=8 || a.wide!=3.5 || v.values[0]!=3 || v.values[2]!=7 || v.wide!=1.5) return 1; auto callback=T09Cpp::fetchCallback(); auto b=callback(1.25f,v); if(b.values[0]!=1 || b.values[1]!=4 || b.values[2]!=8 || b.wide!=3.5 || v.values[0]!=3 || v.values[2]!=7 || v.wide!=1.5) return 2; return 0; }
