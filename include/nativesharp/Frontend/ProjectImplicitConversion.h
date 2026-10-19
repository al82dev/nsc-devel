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

    bool exists() const noexcept;
};
}
