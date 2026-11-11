#pragma once
typedef float (__attribute__((sysv_abi)) *T09UnsupportedCallback)(float);
T09UnsupportedCallback t09_get_unsupported_callback(void);
