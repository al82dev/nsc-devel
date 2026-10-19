#pragma once

#include "nativesharp/Frontend/NamedTypeDeclarationKind.h"
#include "nativesharp/Frontend/NamedTypeAccessibility.h"
#include "nativesharp/Frontend/NamedTypeDeclarationPart.h"
#include "nativesharp/Frontend/NamedTypeDeclaration.h"
#include "nativesharp/Frontend/NamespaceFunctionAccessibility.h"
#include "nativesharp/Frontend/NamespaceFunctionDeclaration.h"
#include "nativesharp/Frontend/NamespaceStateDeclaration.h"
#include "nativesharp/Frontend/TypeNameBindingKind.h"

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

struct TypeNameBinding final
{
    const ast::TypeSyntax* syntax = nullptr;
    TypeNameBindingKind kind = TypeNameBindingKind::Unresolved;
    std::size_t projectOrdinal = 0;
    FileId fileId;
    std::string namespaceName;
    std::string writtenName;
    std::optional<std::size_t> targetTypeOrdinal;
    std::vector<std::size_t> candidateTypeOrdinals;
    // NSC-LIMIT-009. Los tipos dentro de los que se escribio ESTE nombre, del mas exterior al mas
    // interior. La resolucion de nombres los usa para ver los anidados `private` del contenedor;
    // se guardan aqui porque `protected` no se puede responder todavia --hace falta saber si el
    // sitio deriva del contenedor, y la herencia se construye una fase mas tarde-- y quien la
    // responda necesita saber desde donde se escribio.
    std::vector<std::size_t> enclosingTypeOrdinals;
};
}
