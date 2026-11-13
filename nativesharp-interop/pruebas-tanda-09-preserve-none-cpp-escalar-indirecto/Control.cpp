#include "api.h"
extern "C" { __declspec(selectany) int _fltused = 0; }
int main() { auto callback=T09Cpp::fetchCallback(); return callback==&T09Cpp::scalar && callback(-3.5f)==-71 && callback(8.0f)==42 ? 0 : 1; }
