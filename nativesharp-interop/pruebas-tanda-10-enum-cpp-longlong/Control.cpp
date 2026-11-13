#include "api.h"
int main() { Mode a = Low; Mode b = High; Mode c = b; Carrier x; x.value = a; x.guard = 2; return (long long)a == -1099511627776LL && (long long)c == 1099511627783LL && (long long)x.value == -1099511627776LL && x.guard == 2 ? 0 : 1; }
