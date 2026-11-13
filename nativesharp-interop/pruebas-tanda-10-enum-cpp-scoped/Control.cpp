#include "api.h"
int main() {
    Space::Mode a = static_cast<Space::Mode>(10);
    Space::Mode b = a;
    return static_cast<int>(b) == 10 ? 0 : 1;
}
