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
#include "nativesharp/Frontend/ExpressionTypeNameBinding.h"

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

class ModuleDeclarationSpace final
{
public:
    ModuleDeclarationSpace(std::size_t projectOrdinal, std::string moduleName);

    std::size_t projectOrdinal() const noexcept;
    const std::string& moduleName() const noexcept;
    GlobalNamespace& globalNamespace() noexcept;
    const GlobalNamespace& globalNamespace() const noexcept;

    void addTypeOrdinal(std::size_t ordinal);
    std::span<const std::size_t> typeOrdinals() const noexcept;

    void setVisibleProjectOrdinals(std::vector<std::size_t> ordinals);
    std::span<const std::size_t> visibleProjectOrdinals() const noexcept;

private:
    std::size_t projectOrdinal_ = 0;
    std::string moduleName_;
    GlobalNamespace globalNamespace_;
    std::vector<std::size_t> typeOrdinals_;
    std::vector<std::size_t> visibleProjectOrdinals_;
};
}
