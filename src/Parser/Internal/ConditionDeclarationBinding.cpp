#include "ConditionDeclarationBinding.h"

#include "nativesharp/AST/AstNode.h"
#include "nativesharp/AST/AstNodeKind.h"
#include "nativesharp/Diagnostics/DiagnosticEngine.h"
#include "nativesharp/Diagnostics/DiagnosticId.h"
#include "nativesharp/Diagnostics/DiagnosticSeverity.h"

namespace nativesharp::parser
{
const ast::AstNode* conditionDeclarationBinding(
    ast::AstNode* declaration,
    diagnostics::DiagnosticEngine& diagnostics)
{
    if (declaration == nullptr)
    {
        return nullptr;
    }

    const ast::AstNode* binding = declaration;
    if (declaration->kind() == ast::AstNodeKind::BlockStatement &&
        declaration->auxiliaryText() == "declaration-list")
    {
        diagnostics.report(
            diagnostics::DiagnosticId::InvalidCondition,
            diagnostics::DiagnosticSeverity::Error,
            declaration->range(),
            "a condition declaration must declare exactly one local");
        if (declaration->children().empty())
        {
            return nullptr;
        }
        binding = declaration->children().front();
    }
    return binding;
}
}
