extern "C" void cpp_throw_integer() { throw 42; }
extern "C" int cpp_catch_integer() {
    try { cpp_throw_integer(); }
    catch (int value) { return value; }
    return 0;
}
