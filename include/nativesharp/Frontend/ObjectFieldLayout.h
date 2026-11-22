#pragma once

#include "nativesharp/Frontend/ProjectConstructionModel.h"
#include "nativesharp/Frontend/ProjectInheritanceModel.h"
#include "nativesharp/Frontend/ProjectMemberModel.h"
#include "nativesharp/Frontend/ProjectNameLookup.h"
#include "nativesharp/Language/AggregateSemantics.h"
#include "nativesharp/Target/TargetDataLayout.h"

#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <vector>

namespace nativesharp::diagnostics
{
class DiagnosticEngine;
}

namespace nativesharp::frontend
{
struct ProjectOopHirModelTestAccess;

struct ObjectFieldLayout final
{
    std::size_t memberOrdinal = 0;
    std::uint64_t offset = 0;
    std::uint64_t size = 0;
    std::uint64_t alignment = 1;
    std::string typeKey;
    std::optional<std::uint64_t> bitOffset{};
    std::optional<std::uint64_t> bitWidth{};
    // C3-05: the Microsoft storage unit a volatile access uses (absent when it is the field window). / La
    //     unidad de Microsoft que usa un acceso volatile (ausente si es la ventana del campo).
    std::optional<std::uint64_t> volatileUnitByteOffset{};
    std::optional<std::uint64_t> volatileUnitBitOffset{};
    std::optional<std::uint64_t> volatileUnitBytes{};
};
}
