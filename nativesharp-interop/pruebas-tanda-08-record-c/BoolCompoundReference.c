volatile _Bool values[2];
int add_one(void) { return (values[0] += 1); }
int subtract_one(void) { return (values[0] -= 1); }
int xor_two(void) { return (values[0] ^= 2); }
int multiply_three(void) { return (values[0] *= 3); }
int divide_two(void) { return (values[0] /= 2); }
int remainder_two(void) { return (values[0] %= 2); }
int shift_left_one(void) { return (values[0] <<= 1); }
int shift_right_one(void) { return (values[0] >>= 1); }
