#pragma once

#include "nativesharp/Frontend/NamedTypeDeclarationKind.h"
#include "nativesharp/Frontend/NamedTypeAccessibility.h"
#include "nativesharp/Frontend/NamedTypeDeclarationPart.h"
#include "nativesharp/Frontend/NamedTypeDeclaration.h"
#include "nativesharp/Frontend/NamespaceFunctionAccessibility.h"
#include "nativesharp/Frontend/NamespaceFunctionDeclaration.h"
#include "nativesharp/Frontend/NamespaceStateDeclaration.h"
#include "nativesharp/Frontend/TypeNameBindingKind.h"
#include "nativesharp/Frontend/TypeNameBinding.h"

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

struct ExpressionTypeNameBinding final
{
    const ast::AstNode* expression = nullptr;
    std::string writtenName;
    std::vector<std::size_t> candidateTypeOrdinals;
    // NSC-LIMIT-009, segunda vuelta. LA MISMA CADENA QUE GUARDA TypeNameBinding, y por la misma
    // razon: `Base.Reservada.Codigo()` nombra un tipo SIN escribir un TypeSyntax --el receptor es
    // una expresion-- de modo que un anidado `protected` se alcanzaba por este camino sin que
    // nadie comprobara nada. Ahora los dos tipos de binding llevan el contexto lexico del sitio y
    // la comprobacion diferida recorre los dos.
    std::vector<std::size_t> enclosingTypeOrdinals;
};
}
