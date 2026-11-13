#include "api.h"
extern "C" { __declspec(selectany) int _fltused = 0; }
int main() { T09Cpp::Mode v=T09Cpp::Mode::Chosen; auto callback=T09Cpp::fetchCallback(); return callback(1,v,8)==32 && v==T09Cpp::Mode::Chosen ? 0 : 1; }
