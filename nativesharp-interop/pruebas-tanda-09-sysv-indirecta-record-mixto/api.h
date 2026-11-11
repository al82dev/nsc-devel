#pragma once
typedef struct T09IndirectMixed { double x; int y; } T09IndirectMixed;
typedef int (__attribute__((sysv_abi)) *T09MixedCallback)(T09IndirectMixed);
T09MixedCallback t09_get_mixed_callback(void);
