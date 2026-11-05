namespace Plugin {
__declspec(dllexport) int shared = 3;
__declspec(dllexport) int increment(int value) noexcept { return value + 1; }
class Imported { public: __declspec(dllexport) static int value; __declspec(dllexport) static int scale(int); __declspec(dllexport) int add(int) const; };
int Imported::value = 4;
int Imported::scale(int value) { return value * 2; }
int Imported::add(int value) const { return value + 2; }
static Imported instance;
__declspec(dllexport) Imported* make() noexcept { return &instance; }
}
