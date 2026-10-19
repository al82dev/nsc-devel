#pragma once

#include "nativesharp/Frontend/NamespaceFunctionPart.h"
#include "nativesharp/Frontend/NamespaceFunctionSymbol.h"
#include "nativesharp/Frontend/PrecompiledNamespaceFunctionIdentity.h"
#include "nativesharp/Frontend/NamespaceStateInitializerKind.h"
#include "nativesharp/Frontend/NamespaceStateSymbol.h"

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
struct NamespaceFunctionReferenceBinding final
{
    const ast::AstNode* callSyntax = nullptr;
    const ast::AstNode* calleeSyntax = nullptr;
    std::size_t projectOrdinal = 0;
    FileId fileId;
    std::string lexicalNamespace;
    std::string writtenName;
    bool qualified = false;
    std::vector<std::size_t> candidateFunctionOrdinals;
    bool extension = false;
    const ast::AstNode* receiverSyntax = nullptr;
};
}
