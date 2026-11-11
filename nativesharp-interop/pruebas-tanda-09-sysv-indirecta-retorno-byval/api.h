#pragma once
typedef struct T09LargeReturn { int a, b, c, d, e; } T09LargeReturn;
typedef T09LargeReturn (__attribute__((sysv_abi)) *T09LargeReturnCallback)(T09LargeReturn);
T09LargeReturnCallback t09_get_large_return_callback(void);
