int volatile_access_read(volatile int *address)
{
    return *address;
}

void volatile_access_write(volatile int *address, int value)
{
    *address = value;
}
