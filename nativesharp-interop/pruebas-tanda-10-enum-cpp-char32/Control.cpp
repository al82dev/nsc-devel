#include "api.h"
int main() { Mode a = Low; Mode b = High; Mode c = b; Carrier x; x.value = c; x.guard = 2; return (unsigned int)a == 0u && (unsigned int)c == 0xffffffffu && (unsigned int)x.value == 0xffffffffu && x.guard == 2 ? 0 : 1; }
