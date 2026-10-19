#pragma once

#include "nativesharp/Frontend/NamespaceFunctionPart.h"
#include "nativesharp/Frontend/NamespaceFunctionSymbol.h"

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
struct PrecompiledNamespaceFunctionIdentity final
{
    std::string moduleName;
    std::string namespaceName;
    std::string name;
    std::string signatureKey;
    std::vector<language::EscapeClass> parameterEscapeSummary;
};
}
