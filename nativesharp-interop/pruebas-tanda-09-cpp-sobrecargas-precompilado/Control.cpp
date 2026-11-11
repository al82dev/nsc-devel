#include "api.h"
int main() { return T09Cpp::calculate(3,4,7) == 347 && T09Cpp::calculate(3LL,4,7) == 3047 ? 0 : 1; }
