#pragma once
typedef struct T09IndirectLarge { int a, b, c, d, e; } T09IndirectLarge;
typedef int (__attribute__((sysv_abi)) *T09LargeCallback)(T09IndirectLarge);
T09LargeCallback t09_get_large_callback(void);
