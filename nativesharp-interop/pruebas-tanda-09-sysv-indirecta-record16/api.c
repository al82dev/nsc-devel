#include "api.h"
static int __attribute__((sysv_abi)) record_value(T09IndirectRecord16 value) {
    int result = value.x + 2 * value.y + 3 * value.z + 4 * value.w;
    value.x = 99;
    return result;
}
T09RecordCallback16 t09_get_record_callback16(void) { return record_value; }
