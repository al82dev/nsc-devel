#pragma once

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
struct NamespaceFunctionPart final
{
    NamespaceFunctionAccessibility accessibility = NamespaceFunctionAccessibility::Internal;
    std::size_t sourceOrdinal = 0;
    FileId fileId;
    SourceRange range;
    std::string sourcePath;
    const ast::AstNode* syntax = nullptr;
    bool hasBody = false;
};
}
