#include "SemaConversionEvaluationTestSupport.h"
#include "SemaInfrastructureTestSupport.h"

// The directed Sema tests observe private analyzer contracts. Under the MSVC
// ABI, access control participates in decorated names, so the product body must
// see the same test-only declaration as its callers. The compiler product is
// unchanged; only this test target compiles the existing implementation through
// the access-consistent declaration prepared by the support header.
#include "../../../../src/Sema/SemanticAnalyzer.cpp"

namespace nsc::suite::sema_infrastructure_support
{
void reportPreviousDeclarationForTesting(
    nativesharp::sema::SemanticAnalyzer& analyzer,
    const nativesharp::sema::Symbol* symbol)
{
    analyzer.reportPreviousDeclaration(symbol);
}
}
