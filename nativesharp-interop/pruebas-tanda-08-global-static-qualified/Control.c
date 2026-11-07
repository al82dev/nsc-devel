#include "Api.h"
int main(void) {
    if (limit != 8 || small != 4 || *readptr != 9 || *fixedptr != 9 || *volptr != 13) return 1;
    small = 5; *fixedptr = 10; *volptr = 14;
    return inspect() ? 0 : 2;
}
