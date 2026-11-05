int main() {
    try { throw 42; }
    catch (int value) { return value == 42 ? 0 : 1; }
    return 2;
}
