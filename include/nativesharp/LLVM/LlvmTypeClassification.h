#pragma once

#include "nativesharp/Frontend/ProjectNameLookup.h"
#include "nativesharp/Frontend/ProjectTypeConversion.h"
#include "nativesharp/Language/CallableSemantics.h"

#include <optional>
#include <string_view>

// NSC305-L03. Type classification moved out of ProjectOopLlvmLowerer (pure move).
namespace nativesharp::llvm_backend
{
bool isPointerLikeType(std::string_view typeKey) noexcept;

bool isDelegateReferenceType(
    const frontend::ProjectNameLookup& nameLookup, std::string_view typeKey) noexcept;

bool isClassReferenceType(
    const frontend::ProjectNameLookup& nameLookup, std::string_view typeKey) noexcept;
} // namespace nativesharp::llvm_backend
