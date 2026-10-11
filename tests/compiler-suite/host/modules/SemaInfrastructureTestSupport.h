#pragma once

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
}
