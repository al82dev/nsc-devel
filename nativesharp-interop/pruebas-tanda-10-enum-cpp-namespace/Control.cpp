#include "api.h"
int main() {
    First::Mode a = static_cast<First::Mode>(10);
    Second::Mode b = static_cast<Second::Mode>(20);
    return static_cast<int>(a) == 10 && static_cast<int>(b) == 20 ? 0 : 1;
}
