#include "Restricted.h"

int direct_volatile_read(FlexVolatile* value) { return value->data[0]; }
void direct_volatile_write(FlexVolatile* value, int item) { value->data[0] = item; }

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

int reassign_volatile_read(FlexVolatile* first, FlexVolatile* second)
{
    volatile int* data = first->data;
    data = second->data;
    return data[0];
}

void reassign_volatile_write(FlexVolatile* first, FlexVolatile* second, int item)
{
    volatile int* data = first->data;
    data = second->data;
    data[0] = item;
}
