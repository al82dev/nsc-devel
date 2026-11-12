#include "api.h"
extern "C" { int _fltused = 0; }
int main() {
    int a=7, b=7;
    double direct=T09DoubleCpp::transform(2.25, &a, 3.5);
    auto callback=T09DoubleCpp::fetch(); double indirect=callback(2.25, &b, 3.5);
    return !(direct==5.75 && indirect==5.75 && a==19 && b==19);
}
