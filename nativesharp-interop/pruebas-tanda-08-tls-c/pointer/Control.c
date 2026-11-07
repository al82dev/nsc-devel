#include "StaticApi.h"
__declspec(dllimport) void __stdcall ExitProcess(unsigned int);
void mainCRTStartup(void)
{
    if (tls_pointer) ExitProcess(1);
    tls_pointer = pointer_target();
    if (*tls_pointer != 37) ExitProcess(2);
    pointer_prepare();
    if (**tls_nested_pointer != 37 || *(int*)tls_void_pointer != 37) ExitProcess(7);
    if (tls_record.value != 3 || tls_record.extra != 4 || tls_records[1].value != 7 || tls_mode != TLS_START) ExitProcess(5);
    tls_record.value = 31; tls_record_pointer->extra = 32; tls_records[1].value = 71; *tls_mode_pointer = TLS_SET;
    if (tls_volatile_record.value != 10 || tls_const_volatile_record.extra != 13 || tls_volatile_records[1].value != 16 || tls_volatile_mode != TLS_START) ExitProcess(8);
    tls_volatile_record.value = 40; tls_volatile_records[1].value = 46; tls_volatile_mode = TLS_SET;
    tls_callback = pointer_callback();
    if (tls_callback(38) != 42) ExitProcess(6);
    tls_pointer_array[1] = pointer_target(); tls_callback_array[1] = pointer_callback();
    if (*tls_pointer_array[1] != 37 || tls_callback_array[1](38) != 42) ExitProcess(9);
    tls_double_pointer = pointer_double_target();
    if (*tls_double_pointer != 1.5 || *tls_readonly_pointer != 37 || *tls_const_slot != 37) ExitProcess(4);
    *tls_double_pointer = 2.5; tls_volatile_slot = pointer_target(); *tls_volatile_pointer = 41;
    *tls_pointer = 41;
    ExitProcess(pointer_check() || pointer_other_thread() || pointer_check() ? 3 : 0);
}
