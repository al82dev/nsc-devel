#include "api.h"
extern "C" { int _fltused=0; }
int main(){ T09HfaCpp::Nested a{{10.0f,20.0f},30.0f}; T09HfaCpp::OtherNested n{{1.0f,2.0f}}; T09HfaCpp::Array b{{10.0f,20.0f,30.0f}}; T09HfaCpp::OtherArray q{{1.0f,2.0f}}; return !(T09HfaCpp::nested(a,7,n)==166 && T09HfaCpp::fetchNested()(a,7,n)==166 && T09HfaCpp::array(b,7,q)==166 && T09HfaCpp::fetchArray()(b,7,q)==166); }
