#include <cstdint>

extern "C" {
struct Pair { std::int32_t Left; std::int32_t Right; };
std::int32_t host_cpp_pair(Pair value) { return value.Left + value.Right; }
}
