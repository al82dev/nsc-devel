#include "api.h"
int main() {
    UMode a = UZero;
    UMode b = UHigh;
    UMode c = b;
    UCarrier carrier; carrier.value = UHigh; carrier.guard = 2;
    return static_cast<unsigned int>(a) == 0u && static_cast<unsigned int>(c) == 0xffffffffu &&
        static_cast<unsigned int>(carrier.value) == 0xffffffffu && carrier.guard == 2 ? 0 : 1;
}
