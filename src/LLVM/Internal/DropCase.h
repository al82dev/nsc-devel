#pragma once

#include <cstdint>

namespace nativesharp::frontend
{
struct ClassMemberSymbol;
struct ObjectFieldLayout;
}

namespace nativesharp::llvm_backend
{
struct DropCase final
{
    std::uint64_t index = 0;
    const frontend::ObjectFieldLayout* field = nullptr;
    const frontend::ClassMemberSymbol* symbol = nullptr;
};
}
