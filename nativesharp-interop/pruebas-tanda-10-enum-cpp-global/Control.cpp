#include "api.h"
int main() {
    Mode value = static_cast<Mode>(10);
    Mode other = value;
    return static_cast<int>(other) == 10 ? 0 : 1;
}
