#include "nativesharp/MIR/MirValue.h"

namespace nativesharp::mir
{
MirValue::MirValue(std::uint32_t id, sema::Type* type) : id_(id), type_(type) {}
std::uint32_t MirValue::id() const noexcept { return id_; }
sema::Type* MirValue::type() const noexcept { return type_; }
}
