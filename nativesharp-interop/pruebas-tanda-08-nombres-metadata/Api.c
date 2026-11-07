#include "Api.h"
static NRecord record_value = { 1, 2, 3.0 };
NRecord* names_record(void) { return &record_value; }
void* names_raw(void) { return &record_value; }
int names_check(int value) { return record_value.value == value; }
long long names_layout(int id)
{
    switch (id)
    {
    case 1: return (long long)sizeof(NRecord);
    case 2: return (long long)_Alignof(NRecord);
    case 3: return (long long)sizeof(NAlias);
    case 4: return (long long)sizeof(NMood);
    default: return -1;
    }
}
