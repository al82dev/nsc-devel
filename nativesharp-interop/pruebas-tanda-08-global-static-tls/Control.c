#include "Api.h"
int main(void) {
    if (local_value != 7 || read_local() != 7) return 1;
    local_value = 12;
    if (read_local() != 12) return 2;
    write_local(19);
    return local_value == 19 ? 0 : 3;
}
