#pragma once

#include "nativesharp/Frontend/ProjectOopMirModel.h"

#include <string_view>

// NSC305-L03. Struct/class layout lookups moved out of ProjectOopLlvmLowerer (pure move).
namespace nativesharp::llvm_backend
{
inline const frontend::ProjectOopMirStructLayout* findStructLayout(
    const frontend::ProjectOopMirModel& mirModel, std::string_view typeKey) noexcept
{
    for (const frontend::ProjectOopMirStructLayout& layout : mirModel.structLayouts())
    {
        if (layout.typeKey == typeKey) return &layout;
    }
    return nullptr;
}

inline const frontend::ProjectOopMirClassLayout* findClassLayout(
    const frontend::ProjectOopMirModel& mirModel, std::string_view typeKey) noexcept
{
    for (const frontend::ProjectOopMirClassLayout& layout : mirModel.classLayouts())
    {
        if (layout.typeKey == typeKey) return &layout;
    }
    return nullptr;
}
} // namespace nativesharp::llvm_backend
