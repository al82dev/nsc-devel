#include "api.h"
extern "C" { __declspec(selectany) int _fltused = 0; }
int main() { T09Cpp::scalar(-3.5f,7); if(T09Cpp::read()!=70) return 1; auto callback=T09Cpp::fetchCallback(); callback(1.25f,-2); return T09Cpp::read()==-200 ? 0 : 2; }
