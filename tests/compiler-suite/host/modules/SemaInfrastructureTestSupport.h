#pragma once

#include <cstdint>

namespace nativesharp::sema
{
class SemanticAnalyzer;
class Symbol;
}

namespace nsc::suite::sema_infrastructure_support
{
// Exposes the existing private diagnostic helper only inside the component-test
// target. The production declaration and implementation remain unchanged.
void reportPreviousDeclarationForTesting(
    nativesharp::sema::SemanticAnalyzer& analyzer,
    const nativesharp::sema::Symbol* symbol);

// Exposes the anonymous-namespace product helper from the same translation
// unit; the calculation itself remains solely in SemanticAnalyzer.cpp.
std::uint64_t semanticMaskForWidthForTesting(std::uint32_t width) noexcept;
}
