#include "Restricted.h"

int var_volatile_read(FlexVolatile* value)
{
    volatile int* data = value->data;
    return data[0];
}

void var_volatile_write(FlexVolatile* value, int item)
{
    volatile int* data = value->data;
    data[0] = item;
}
