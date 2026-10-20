#pragma once

#include "nativesharp/Frontend/BuiltinIntegerTypes.h"
#include "nativesharp/Frontend/EnumUnderlyingType.h"
#include "nativesharp/Frontend/ProjectNameLookup.h"
#include "nativesharp/Frontend/ProjectOopMirModel.h"

#include <string_view>

// NSC305-L03. Struct/class layout + enum underlying-type lookups moved out of ProjectOopLlvmLowerer
// (pure move).
namespace nativesharp::llvm_backend
{
const frontend::ProjectOopMirStructLayout* findStructLayout(
    const frontend::ProjectOopMirModel& mirModel, std::string_view typeKey) noexcept;

const frontend::ProjectOopMirClassLayout* findClassLayout(
    const frontend::ProjectOopMirModel& mirModel, std::string_view typeKey) noexcept;

const frontend::builtins::IntegerTypeInfo* findEnumUnderlyingInfo(
    const frontend::ProjectNameLookup& nameLookup, std::string_view typeKey);

bool isEnumTypeKey(
    const frontend::ProjectNameLookup& nameLookup, std::string_view typeKey);
} // namespace nativesharp::llvm_backend
