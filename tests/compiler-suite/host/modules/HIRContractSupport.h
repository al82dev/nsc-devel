#pragma once

#include "nsc_suite/TestRegistry.h"

#include "nativesharp/Frontend/FrontendCompilation.h"
#include "nativesharp/HIR/HirNode.h"
#include "nativesharp/HIR/HirNodeKind.h"

#include <array>
#include <cstdio>
#include <filesystem>
#include <functional>
#include <memory>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace nsc::suite::hir_contract_support
{
using nativesharp::frontend::FrontendCompilation;
using nativesharp::hir::HirNode;
using nativesharp::hir::HirNodeKind;
using MatrixOracle = std::function<void(TestContext&, std::size_t)>;

// Formats a stable three-digit case identifier without relying on locale. Complexity is O(1).
inline std::string caseId(std::size_t ordinal)
{
    std::array<char, 16> buffer{};
    std::snprintf(buffer.data(), buffer.size(), "TF-HIR-%03zu", ordinal);
    return buffer.data();
}

// Formats a stable three-digit contract identifier. Complexity is O(1).
inline std::string contractId(std::size_t ordinal)
{
    std::array<char, 16> buffer{};
    std::snprintf(buffer.data(), buffer.size(), "CF-HIR-%03zu", ordinal);
    return buffer.data();
}

// Registers all eight canonical matrix variants for one contract. Complexity is O(1).
inline void addMatrix(TestRegistry& registry, std::size_t contract, std::string layer, MatrixOracle oracle)
{
    const std::size_t first = ((contract - 1) * 8) + 1;
    for (std::size_t variant = 0; variant < 8; ++variant)
    {
        const std::string id = caseId(first + variant);
        registry.add(TestCase{
            id,
            contractId(contract),
            "Host.HIR",
            layer,
            variant == 7 ? "NIGHTLY" : "PR",
            {"hir", "phase2", "matrix-" + std::to_string(variant + 1)},
            [variant, oracle](TestContext& context) {
                oracle(context, variant);
            },
        });
    }
}

// Finds a node iteratively so deep fixtures never consume recursive test stack. Complexity is O(n).
inline const HirNode* findFirst(const HirNode* root, HirNodeKind kind)
{
    std::vector<const HirNode*> pending;
    if (root != nullptr)
    {
        pending.push_back(root);
    }
    while (!pending.empty())
    {
        const HirNode* node = pending.back();
        pending.pop_back();
        if (node->kind() == kind)
        {
            return node;
        }
        for (auto child = node->children().rbegin(); child != node->children().rend(); ++child)
        {
            if (*child != nullptr) pending.push_back(*child);
        }
        if (node->third() != nullptr) pending.push_back(node->third());
        if (node->second() != nullptr) pending.push_back(node->second());
        if (node->first() != nullptr) pending.push_back(node->first());
    }
    return nullptr;
}

// Counts one node class iteratively. Complexity is O(n).
inline std::size_t countKind(const HirNode* root, HirNodeKind kind)
{
    std::size_t count = 0;
    std::vector<const HirNode*> pending;
    if (root != nullptr) pending.push_back(root);
    while (!pending.empty())
    {
        const HirNode* node = pending.back();
        pending.pop_back();
        count += node->kind() == kind ? 1 : 0;
        for (const HirNode* child : node->children()) if (child != nullptr) pending.push_back(child);
        if (node->first() != nullptr) pending.push_back(node->first());
        if (node->second() != nullptr) pending.push_back(node->second());
        if (node->third() != nullptr) pending.push_back(node->third());
    }
    return count;
}

// Compiles one isolated source and requires a complete HIR product. Complexity follows the frontend pipeline.
inline std::unique_ptr<FrontendCompilation> compileHir(
    TestContext& context, std::string_view label, std::string source)
{
    auto compilation = std::make_unique<FrontendCompilation>();
    const bool completed = compilation->compileBuffer(
        std::filesystem::path(std::string(label) + ".ns"), std::move(source), false);
    context.expect(completed, std::string(label) + " must complete the frontend pipeline");
    context.expect(compilation->hirRoot() != nullptr, std::string(label) + " must produce a HIR root");
    context.expect(compilation->hirError().empty(), std::string(label) + " must not report a HIR error");
    return compilation;
}
}
