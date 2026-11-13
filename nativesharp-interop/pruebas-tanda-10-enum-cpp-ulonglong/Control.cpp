#include "api.h"
int main() { Mode a = Low; Mode b = High; Mode c = b; Carrier x; x.value = c; x.guard = 2; return (unsigned long long)a == 0 && (unsigned long long)c == 0xffffffffffffffffULL && (unsigned long long)x.value == 0xffffffffffffffffULL && x.guard == 2 ? 0 : 1; }
