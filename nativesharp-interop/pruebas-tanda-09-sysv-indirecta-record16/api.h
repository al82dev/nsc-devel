#pragma once
typedef struct T09IndirectRecord16 { int x, y, z, w; } T09IndirectRecord16;
typedef int (__attribute__((sysv_abi)) *T09RecordCallback16)(T09IndirectRecord16);
T09RecordCallback16 t09_get_record_callback16(void);
