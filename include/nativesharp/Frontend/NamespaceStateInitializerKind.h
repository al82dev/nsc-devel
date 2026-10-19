#pragma once

#include "nativesharp/Frontend/NamespaceFunctionPart.h"
#include "nativesharp/Frontend/NamespaceFunctionSymbol.h"
#include "nativesharp/Frontend/PrecompiledNamespaceFunctionIdentity.h"

#include "nativesharp/Frontend/ProjectMemberModel.h"
#include "nativesharp/Frontend/ProjectNameLookup.h"

#include <cstddef>
#include <cstdint>
#include <span>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>
#include <vector>

namespace nativesharp::diagnostics
{
class DiagnosticEngine;
}

namespace nativesharp::frontend
{
class ProjectGenericModel;
enum class NamespaceStateInitializerKind
{
    Zero,
    Integer,
    Floating,
    Boolean,
    String,
    Null
};
}
