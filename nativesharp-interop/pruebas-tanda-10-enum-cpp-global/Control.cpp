#include "api.h"
int main() {
    Mode value = static_cast<Mode>(10);
    Mode other = value;
    Mode chosen = Chosen;
    Mode changed = Changed;
    Mode negative = Negative;
    Carrier carrier;
    carrier.field = negative;
    carrier.padding = 2;
    return static_cast<int>(other) == 10 && static_cast<int>(chosen) == 10 &&
        static_cast<int>(changed) == 11 && static_cast<int>(negative) == -7 &&
        static_cast<int>(carrier.field) == -7 && carrier.padding == 2 ? 0 : 1;
}
