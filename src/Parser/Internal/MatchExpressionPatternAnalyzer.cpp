#include "MatchExpressionPatternAnalyzer.h"

#include <algorithm>
#include <utility>

namespace nativesharp::parser
{
Parser::MatchExpressionPatternAnalyzer::MatchExpressionPatternAnalyzer(
    Parser& parser,
    bool reportOrDiagnostics)
    : parser_(parser), reportOrDiagnostics_(reportOrDiagnostics)
{
    }

Parser::MatchPatternAnalysis Parser::MatchExpressionPatternAnalyzer::analyze(
    const Pattern& root)
{
        pushFrame(root, true);
        while (!frames_.empty())
        {
            Frame& frame = frames_.back();
            if (frame.nextChild < frame.pattern->children.size())
            {
                const bool childPreservesAddress = frame.preservesAddress &&
                    frame.pattern->kind != PatternKind::Tuple &&
                    frame.pattern->kind != PatternKind::Variant;
                pushFrame(frame.pattern->children[frame.nextChild++], childPreservesAddress);
                continue;
            }
            Summary summary = finishFrame(frame);
            frames_.pop_back();
            if (frames_.empty()) return finishAnalysis(std::move(summary));
            frames_.back().children.push_back(std::move(summary));
        }
        return {};
    }

void Parser::MatchExpressionPatternAnalyzer::pushFrame(const Pattern& pattern, bool preservesAddress)
{
        const std::size_t preorder = patterns_.size();
        patterns_.push_back(&pattern);
        invalidOr_.push_back(false);
        if (pattern.kind == PatternKind::Binding &&
            pattern.bindingMode != PatternBindingMode::Value && !preservesAddress)
        {
            nonAddressableBindingRanges_.push_back(pattern.range);
            nonAddressableBindings_.emplace_back(pattern.range, pattern.bindingMode);
        }
        Frame frame{&pattern, 0, preorder, preservesAddress, {}};
        frame.children.reserve(pattern.children.size());
        frames_.push_back(std::move(frame));
    }

Parser::MatchExpressionPatternAnalyzer::Summary
Parser::MatchExpressionPatternAnalyzer::finishFrame(Frame& frame)
{
        Summary summary;
        const Pattern& pattern = *frame.pattern;
        summary.catchAll = pattern.kind == PatternKind::Wildcard ||
            pattern.kind == PatternKind::Binding;
        summary.wildcard = summary.catchAll;
        if (pattern.kind == PatternKind::Binding)
            summary.bindings[{pattern.name, pattern.bindingMode}] = 1U;
        if (pattern.kind == PatternKind::Or) summarizeOr(frame, summary);
        if (pattern.kind == PatternKind::Variant) summarizeVariant(frame, summary);
        if (pattern.kind == PatternKind::List) summarizeList(frame, summary);
        for (Summary& child : frame.children)
            mergeBindings(summary.bindings, child.bindings);
        return summary;
    }

void Parser::MatchExpressionPatternAnalyzer::summarizeOr(Frame& frame, Summary& summary)
{
        if (!frame.children.empty())
        {
            const BindingCounts& expected = frame.children.front().bindings;
            invalidOr_[frame.preorder] = std::ranges::any_of(
                frame.children.begin() + 1, frame.children.end(),
                [&](const Summary& child) { return child.bindings != expected; });
        }
        for (Summary& child : frame.children)
        {
            summary.catchAll = summary.catchAll || child.catchAll;
            summary.wildcard = summary.wildcard || child.wildcard;
            summary.partial = summary.partial || child.partial;
            mergeVector(summary.cases, child.cases);
            mergeVector(summary.listLengths, child.listLengths);
        }
    }

void Parser::MatchExpressionPatternAnalyzer::summarizeVariant(const Frame& frame, Summary& summary)
{
        summary.cases.push_back(frame.pattern->name);
        if (!frame.children.empty() && !frame.children.front().catchAll)
            summary.partial = true;
    }

void Parser::MatchExpressionPatternAnalyzer::summarizeList(const Frame& frame, Summary& summary)
{
        const Pattern& pattern = *frame.pattern;
        const std::size_t fixedCount = pattern.children.size() -
            (pattern.restIndex.has_value() ? 1U : 0U);
        summary.listLengths.emplace_back(fixedCount, pattern.restIndex.has_value());
        for (std::size_t index = 0; index < frame.children.size(); ++index)
        {
            if (pattern.restIndex.has_value() && *pattern.restIndex == index) continue;
            if (!frame.children[index].catchAll) { summary.partial = true; return; }
        }
    }

void Parser::MatchExpressionPatternAnalyzer::mergeBindings(BindingCounts& target, BindingCounts& source)
{
        if (target.size() < source.size()) target.swap(source);
        for (const auto& [binding, count] : source) target[binding] += count;
        source.clear();
    }

template <typename Value>
static void Parser::MatchExpressionPatternAnalyzer::mergeVector(std::vector<Value>& target, std::vector<Value>& source)
{
        if (target.size() < source.size()) target.swap(source);
        target.insert(
            target.end(),
            std::make_move_iterator(source.begin()),
            std::make_move_iterator(source.end()));
        source.clear();
    }

MatchPatternAnalysis Parser::MatchExpressionPatternAnalyzer::finishAnalysis(Summary summary)
{
        reportInvalidOrPatterns();
        MatchPatternAnalysis result;
        for (const auto& [binding, count] : summary.bindings)
            for (std::size_t index = 0; index < count; ++index)
                result.bindings.push_back(binding);
        std::ranges::sort(result.bindings);
        for (std::size_t index = 0; index < invalidOr_.size(); ++index)
            if (invalidOr_[index]) result.invalidOrRanges.push_back(patterns_[index]->range);
        result.nonAddressableBindingRanges = std::move(nonAddressableBindingRanges_);
        result.nonAddressableBindings = std::move(nonAddressableBindings_);
        result.cases = std::move(summary.cases);
        result.listLengths = std::move(summary.listLengths);
        result.wildcard = summary.wildcard;
        result.partial = summary.partial;
        return result;
    }

void Parser::MatchExpressionPatternAnalyzer::reportInvalidOrPatterns()
{
        if (!reportOrDiagnostics_) return;
        for (std::size_t index = 0; index < invalidOr_.size(); ++index)
        {
            if (!invalidOr_[index]) continue;
            parser_.diagnostics_.report(
                diagnostics::DiagnosticId::InvalidOperand,
                diagnostics::DiagnosticSeverity::Error,
                patterns_[index]->range,
                "OR-pattern alternatives in a match expression must bind identical names "
                "with identical value/ref/move modes");
        }
    }
}
