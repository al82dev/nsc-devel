#include "Api.h"
int main(void) {
    if (!check_initial()) return 1;
    record_value.value = 10; items[0] = 11; changing = 12;
    return check_modified() ? 0 : 2;
}
