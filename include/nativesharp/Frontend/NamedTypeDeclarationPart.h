#pragma once

#include "nativesharp/Frontend/NamedTypeDeclarationKind.h"
#include "nativesharp/Frontend/NamedTypeAccessibility.h"

#include "nativesharp/AST/TypeSyntax.h"
#include "nativesharp/Basic/FileId.h"
#include "nativesharp/Basic/SourceRange.h"
#include "nativesharp/Compilation/ProjectGraph.h"
#include "nativesharp/Compilation/TypeIdentity.h"
#include "nativesharp/Frontend/NamespaceSymbol.h"
#include "nativesharp/Frontend/ProjectSyntaxUnit.h"

#include <cstddef>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <unordered_map>
#include <vector>

namespace nativesharp::diagnostics
{
class DiagnosticEngine;
}

namespace nativesharp::frontend
{
struct ProjectOopHirModelTestAccess;

struct NamedTypeDeclarationPart final
{
    NamedTypeDeclarationKind kind = NamedTypeDeclarationKind::Struct;
    NamedTypeAccessibility accessibility = NamedTypeAccessibility::Internal;
    bool hasExplicitAccessibility = false;
    // NSC-LIMIT-009. Declarado dentro de otro tipo. Se guarda aunque la identidad ya lo diga
    // porque el agregador de partes solo entiende internal/public: para un anidado hay que
    // saltarlo y conservar la accesibilidad que se calculo al predeclarar.
    bool isNested = false;
    bool isPartial = false;
    bool isAbstract = false;
    bool isSealed = false;
    bool isStatic = false;
    bool isPrecompiled = false;
    std::size_t projectOrdinal = 0;
    std::size_t sourceOrdinal = 0;
    FileId fileId;
    SourceRange range;
    const ast::AstNode* syntax = nullptr;
    std::string sourcePath;
};
}
