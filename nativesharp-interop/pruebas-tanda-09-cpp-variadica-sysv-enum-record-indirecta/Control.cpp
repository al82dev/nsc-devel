#include "api.h"
extern "C" { __declspec(selectany) int _fltused = 0; }
int main() { auto e=T09Cpp::Mode::Chosen; T09VarValue v = {2.25,7}; auto callback=T09Cpp::fetchCallback(); return callback(1,e,v)==32 && v.x==2.25 && v.y==7 ? 0 : 1; }
