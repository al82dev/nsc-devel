#pragma once

#include "nativesharp/Parser/Parser.h"

#include <cstddef>
#include <functional>
#include <iterator>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

namespace nativesharp::parser
{
class Parser::MatchExpressionPatternAnalyzer final
{
    using BindingKey = std::pair<std::string, PatternBindingMode>;
    struct BindingKeyHash final
    {
        std::size_t operator()(const BindingKey& binding) const noexcept
        {
            const std::size_t name = std::hash<std::string>{}(binding.first);
            const std::size_t mode = std::hash<unsigned>{}(
                static_cast<unsigned>(binding.second));
            return name ^ (mode + 0x9e3779b9U + (name << 6U) + (name >> 2U));
        }
    };
    using BindingCounts = std::unordered_map<BindingKey, std::size_t, BindingKeyHash>;

    struct Summary final
    {
        BindingCounts bindings;
        std::vector<std::string> cases;
        std::vector<std::pair<std::size_t, bool>> listLengths;
        bool catchAll = false;
        bool wildcard = false;
        bool partial = false;
    };

    struct Frame final
    {
        const Pattern* pattern = nullptr;
        std::size_t nextChild = 0;
        std::size_t preorder = 0;
        bool preservesAddress = true;
        std::vector<Summary> children;
    };

public:
    MatchExpressionPatternAnalyzer(Parser& parser, bool reportOrDiagnostics);

    MatchPatternAnalysis analyze(const Pattern& root);

private:
    void pushFrame(const Pattern& pattern, bool preservesAddress);

    Summary finishFrame(Frame& frame);

    void summarizeOr(Frame& frame, Summary& summary);

    static void summarizeVariant(const Frame& frame, Summary& summary);

    static void summarizeList(const Frame& frame, Summary& summary);

    static void mergeBindings(BindingCounts& target, BindingCounts& source);

    template <typename Value>
    static void mergeVector(std::vector<Value>& target, std::vector<Value>& source);

    MatchPatternAnalysis finishAnalysis(Summary summary);

    void reportInvalidOrPatterns();

    Parser& parser_;
    bool reportOrDiagnostics_ = false;
    std::vector<Frame> frames_;
    std::vector<const Pattern*> patterns_;
    std::vector<bool> invalidOr_;
    std::vector<SourceRange> nonAddressableBindingRanges_;
    std::vector<std::pair<SourceRange, PatternBindingMode>> nonAddressableBindings_;
};
}
