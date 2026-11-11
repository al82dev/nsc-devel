#include "api.h"
static int __attribute__((sysv_abi)) record_value(T09IndirectRecord value) {
    int result = value.x + 2 * value.y + 3 * value.z;
    value.x = 99;
    return result;
}
T09RecordCallback t09_get_record_callback(void) { return record_value; }
