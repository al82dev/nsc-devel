#include "api.h"
int main(void) {
    T09IndirectRecord record = {1, 2, 3};
    return t09_get_record_callback()(record) == 14 && record.x == 1 && record.y == 2 && record.z == 3 ? 0 : 1;
}
