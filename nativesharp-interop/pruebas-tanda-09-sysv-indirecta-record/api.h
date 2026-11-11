#pragma once
typedef struct T09IndirectRecord { int x, y, z; } T09IndirectRecord;
typedef int (__attribute__((sysv_abi)) *T09RecordCallback)(T09IndirectRecord);
T09RecordCallback t09_get_record_callback(void);
