#pragma once

namespace nativesharp::ast
{
class AstNode;
}

namespace nativesharp::diagnostics
{
class DiagnosticEngine;
}

namespace nativesharp::parser
{
const ast::AstNode* conditionDeclarationBinding(
    ast::AstNode* declaration,
    diagnostics::DiagnosticEngine& diagnostics);
}
