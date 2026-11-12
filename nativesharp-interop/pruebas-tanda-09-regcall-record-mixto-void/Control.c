#include "api.h"
int _fltused=0;
int main(void) {
    PairMixed value={10.25f,20.5}; int direct=0, indirect=0;
    write_pair(value,&direct); get_write()(value,&indirect);
    return !(direct==51 && indirect==51 && value.x==10.25f);
}
