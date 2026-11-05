namespace Plugin {
class Imported {
public:
    __declspec(dllimport) static int value;
    __declspec(dllimport) static int scale(int);
    __declspec(dllimport) int add(int) const;
};
__declspec(dllimport) Imported* make() noexcept;
}
