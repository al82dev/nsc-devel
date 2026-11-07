volatile _Bool values[2];
int add_one(void) { return (values[0] += 1); }
int subtract_one(void) { return (values[0] -= 1); }
int xor_two(void) { return (values[0] ^= 2); }
int multiply_three(void) { return (values[0] *= 3); }
