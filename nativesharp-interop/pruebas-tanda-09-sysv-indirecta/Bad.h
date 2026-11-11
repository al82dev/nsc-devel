#pragma once
typedef struct T09UnsupportedRecord { int x, y, z; } T09UnsupportedRecord;
typedef int (__attribute__((sysv_abi)) *T09UnsupportedCallback)(T09UnsupportedRecord);
T09UnsupportedCallback t09_get_unsupported_callback(void);
