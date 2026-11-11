#include "api.h"
int main(void) {
    T09IndirectRecord16 record = {1, 2, 3, 4};
    T09RecordCallback16 callback = t09_get_record_callback16();
    return callback(record) != 30 || record.x != 1 || record.y != 2 || record.z != 3 || record.w != 4;
}
