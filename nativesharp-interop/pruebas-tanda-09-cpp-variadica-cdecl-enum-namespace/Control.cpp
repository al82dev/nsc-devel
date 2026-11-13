#include "api.h"
extern "C" { __declspec(selectany) int _fltused = 0; }
int main() { T09Cpp::Mode v=T09Cpp::Chosen; return T09Cpp::sum(1,v,8)==32 && v==T09Cpp::Chosen ? 0 : 1; }
