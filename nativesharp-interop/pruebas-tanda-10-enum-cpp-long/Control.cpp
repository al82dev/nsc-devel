#include "api.h"
int main() { Mode a = Low; Mode b = High; Mode c = b; Carrier x; x.value = c; x.guard = 2; return (int)a == -2147483647 && (int)c == 2147483647 && (int)x.value == 2147483647 && x.guard == 2 ? 0 : 1; }
