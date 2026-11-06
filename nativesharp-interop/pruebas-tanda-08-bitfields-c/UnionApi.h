typedef union UnionBits {
    unsigned int raw;
    unsigned char bytes[4];
    unsigned int first : 3;
    signed int delta : 5;
} UnionBits;
UnionBits* get_union_bits(void);
void init_union_bits(UnionBits* value);
int check_union_bits(UnionBits* value);
