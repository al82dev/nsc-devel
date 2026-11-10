#pragma once

#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>
#include <unordered_map>

namespace nativesharp::ast { class AstNode; }

namespace nativesharp::frontend::internal
{
using ComptimeEnvironment = std::unordered_map<std::string, std::int32_t>;

// One continuation on the explicit, budgeted evaluator worklist.
struct ComptimeTask final
{
    const ast::AstNode* node;
    unsigned phase;
    std::size_t depth;
    std::shared_ptr<const ComptimeEnvironment> environment;
    std::string nameSpace;
    std::string ownerClass;
};
}
