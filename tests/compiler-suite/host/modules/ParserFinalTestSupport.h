#pragma once

#include "ParserFoundationTestSupport.h"

#include "nativesharp/AST/AstNodeKind.h"

#include <cstddef>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace nsc::suite::parser_final_support
{
using parser_support::ParserFixture;
using nativesharp::ast::AstNode;
using nativesharp::ast::AstNodeKind;

// Walks the product tree iteratively so the test suite does not add another recursion limit.
inline std::vector<const AstNode*> flatten(const AstNode* root)
{
    std::vector<const AstNode*> result;
    std::vector<const AstNode*> pending;
    if (root != nullptr) pending.push_back(root);
    while (!pending.empty())
    {
        const AstNode* const node = pending.back();
        pending.pop_back();
        result.push_back(node);
        if (node->first() != nullptr) pending.push_back(node->first());
        if (node->second() != nullptr) pending.push_back(node->second());
        if (node->third() != nullptr) pending.push_back(node->third());
        for (const AstNode* child : node->children())
        {
            if (child != nullptr) pending.push_back(child);
        }
    }
    return result;
}

inline const AstNode* firstKind(const AstNode* root, AstNodeKind kind)
{
    for (const AstNode* node : flatten(root))
    {
        if (node->kind() == kind) return node;
    }
    return nullptr;
}

inline const AstNode* firstText(const AstNode* root, std::string_view text)
{
    for (const AstNode* node : flatten(root))
    {
        if (node->text() == text) return node;
    }
    return nullptr;
}

inline std::size_t countAuxiliary(const AstNode* root, std::string_view value)
{
    std::size_t count = 0;
    for (const AstNode* node : flatten(root)) count += node->auxiliaryText() == value ? 1U : 0U;
    return count;
}

inline void addFinalParserCase(
    TestRegistry& registry,
    std::string id,
    std::string contract,
    TestFunction function,
    std::string profile = "PR",
    TestDisposition disposition = TestDisposition::Required,
    std::vector<std::string> tags = {})
{
    tags.push_back("parser");
    tags.push_back("final-block");
    registry.add(TestCase{std::move(id), std::move(contract), "Host.Parser", "component",
        std::move(profile), std::move(tags), std::move(function), disposition});
}
}
