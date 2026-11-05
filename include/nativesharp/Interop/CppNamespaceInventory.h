#pragma once

#include "nativesharp/Compilation/ProjectGraph.h"
#include "nativesharp/Interop/CppProjectSessions.h"

#include <filesystem>
#include <span>
#include <string>
#include <utility>
#include <variant>
#include <vector>

namespace nativesharp::interop
{
struct CppNamespaceInventoryEntry
{
    std::filesystem::path projectFile;
    std::size_t referenceIndex;
    std::size_t projectOrdinal;
    std::string moduleName;
    CppNamespaceDeclaration declaration;
};

struct CppNamespaceInventoryFailure
{
    std::filesystem::path projectFile;
    std::size_t referenceIndex;
    std::string message;
};

using CppNamespaceInventoryResult =
    std::variant<std::vector<CppNamespaceInventoryEntry>, CppNamespaceInventoryFailure>;

CppNamespaceInventoryResult collectCppNamespaces(
    const compilation::ProjectGraph& finalGraph, std::span<const CppProjectSessions> projects);

using CppVisibleNamespacesResult =
    std::variant<std::vector<std::pair<std::size_t, std::string>>, CppNamespaceInventoryFailure>;
CppVisibleNamespacesResult visibleCppNamespaces(std::span<const CppNamespaceInventoryEntry> entries);
}
