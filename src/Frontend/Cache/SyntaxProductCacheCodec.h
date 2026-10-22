#pragma once

#include "nativesharp/AST/AstArena.h"
#include "nativesharp/AST/AstNode.h"
#include "nativesharp/Frontend/ProjectSyntaxUnit.h"
#include "nativesharp/Lexer/Token.h"
#include "nativesharp/Source/SourceFile.h"

#include <memory>
#include <string>
#include <string_view>
#include <vector>

namespace nativesharp::frontend::internal
{
std::string serializeSyntaxProductUnit(const ProjectSyntaxUnit& unit);
bool deserializeSyntaxProductUnit(
    const source::SourceFile& sourceFile,
    std::string_view payload,
    std::vector<lexer::Token>& tokens,
    std::unique_ptr<ast::AstArena>& arena,
    ast::AstNode*& syntaxTree);
// Restores a producer payload while rebinding its ranges to the consumer SourceFile.
bool deserializeRelocatableSyntaxProductUnit(
    const source::SourceFile& sourceFile,
    std::string_view payload,
    std::vector<lexer::Token>& tokens,
    std::unique_ptr<ast::AstArena>& arena,
    ast::AstNode*& syntaxTree);
}
