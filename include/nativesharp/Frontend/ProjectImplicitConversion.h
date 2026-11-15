#pragma once

#include "nativesharp/Frontend/ProjectConversionKind.h"
#include "nativesharp/Frontend/ProjectIntegerConstant.h"

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <span>
#include <vector>

namespace nativesharp::frontend
{
struct ProjectImplicitConversion final
{
    ProjectConversionKind kind = ProjectConversionKind::None;
    int rank = 1000;
    std::string sourceTypeKey;
    std::string targetTypeKey;
    std::optional<std::size_t> userDefinedMemberOrdinal;
    // T12-003: for CppPointerUpcast, Clang's offset in bytes of the base subobject in the derived object.
    // T12-003: para CppPointerUpcast, el offset en bytes que da Clang al subobjeto base en el derivado.
    std::uint64_t cppPointerOffset = 0;

    bool exists() const noexcept;
};
}
