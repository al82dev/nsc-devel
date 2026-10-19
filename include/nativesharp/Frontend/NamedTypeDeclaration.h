#pragma once

#include "nativesharp/Frontend/NamedTypeDeclarationKind.h"
#include "nativesharp/Frontend/NamedTypeAccessibility.h"
#include "nativesharp/Frontend/NamedTypeDeclarationPart.h"

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

struct NamedTypeDeclaration final
{
    compilation::TypeIdentity identity;
    NamedTypeDeclarationKind kind = NamedTypeDeclarationKind::Struct;
    NamedTypeAccessibility accessibility = NamedTypeAccessibility::Internal;
    // NSC-LIMIT-009. El tipo que lo contiene, si lo hay. Es un ORDINAL y no un nombre porque la
    // pregunta que se le hace --«estoy dentro de este contenedor?»-- se responde comparando
    // ordinales con la cadena que el entorno de lookup lleva, sin volver a resolver nada.
    std::optional<std::size_t> containingTypeOrdinal;
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
    std::vector<NamedTypeDeclarationPart> parts;
};
}
