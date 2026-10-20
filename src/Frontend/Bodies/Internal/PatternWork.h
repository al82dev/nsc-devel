#pragma once

#include <string>

namespace nativesharp::ast
{
class AstNode;
}

namespace nativesharp::frontend
{
struct PatternWork final
{
    const ast::AstNode* binding = nullptr;
    std::string typeKey;
    std::string path;
};
}
