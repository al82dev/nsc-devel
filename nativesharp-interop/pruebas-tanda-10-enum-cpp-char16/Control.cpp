#include "api.h"
int main() { Mode a = Low; Mode b = High; Mode c = b; Carrier x; x.value = c; x.guard = 2; return (int)a == 0 && (int)c == 65535 && (int)x.value == 65535 && x.guard == 2 ? 0 : 1; }
