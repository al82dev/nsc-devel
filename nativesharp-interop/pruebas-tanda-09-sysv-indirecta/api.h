#pragma once
typedef int (__attribute__((sysv_abi)) *T09SysvCallback)(int, int, int, int, int, int);
T09SysvCallback t09_get_sysv_callback(void);
