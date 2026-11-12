#pragma once
typedef struct T09ReturnRecord { int x, y, z, w; } T09ReturnRecord;
typedef T09ReturnRecord (__attribute__((sysv_abi)) *T09ReturnCallback)(T09ReturnRecord);
T09ReturnCallback t09_get_return_callback(void);
