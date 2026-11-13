#include "api.h"
extern "C" { __declspec(selectany) int _fltused = 0; }
int main() { auto callback=T09Cpp::fetchCallback(); return callback==&T09Cpp::scalar && callback(-7,2.5f,9,-3,4.25)==-71.5 && callback(7,2.5f,9,-3,4.25)==42.5 ? 0 : 1; }
