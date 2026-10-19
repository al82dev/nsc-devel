#pragma once

#include "nativesharp/Frontend/NamedTypeDeclarationKind.h"
#include "nativesharp/Frontend/NamedTypeAccessibility.h"
#include "nativesharp/Frontend/NamedTypeDeclarationPart.h"
#include "nativesharp/Frontend/NamedTypeDeclaration.h"
#include "nativesharp/Frontend/NamespaceFunctionAccessibility.h"
#include "nativesharp/Frontend/NamespaceFunctionDeclaration.h"
#include "nativesharp/Frontend/NamespaceStateDeclaration.h"

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

enum class TypeNameBindingKind
{
    Builtin,
    Resolved,
    Unresolved,
    Ambiguous
};
}
