#include "api.h"
namespace T09Cpp {
int calculate(int a, int b, int c) { return a * 100 + b * 10 + c; }
int calculate(long long a, int b, int c) { return (int)a * 1000 + b * 10 + c; }
}
