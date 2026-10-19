#include "PatternAlternativeExpander.h"

#include <algorithm>
#include <cstddef>
#include <iterator>
#include <string>
#include <unordered_set>
#include <utility>
#include <vector>

namespace nativesharp::parser
{
using lexer::TokenKind;
Parser::PatternAlternativeExpander::PatternAlternativeExpander(const Pattern& root)
    : root_(root)
{
}

Parser::Pattern Parser::PatternAlternativeExpander::copyWithoutChildren(
    const Pattern& source)
{
    Pattern copy;
    copy.kind = source.kind;
    copy.bindingMode = source.bindingMode;
    copy.name = source.name;
    copy.range = source.range;
    copy.firstValue = source.firstValue;
    copy.secondValue = source.secondValue;
    copy.firstInteger = source.firstInteger;
    copy.secondInteger = source.secondInteger;
    copy.restIndex = source.restIndex;
    copy.fieldNames = source.fieldNames;
    return copy;
}

Parser::PatternAlternativeExpander::Frame
Parser::PatternAlternativeExpander::makeFrame(const Pattern& source)
{
    Frame frame;
    frame.source = &source;
    if (source.kind != PatternKind::Or)
        frame.alternatives.push_back(copyWithoutChildren(source));
    return frame;
}

void Parser::PatternAlternativeExpander::appendOrAlternatives(
    Frame& frame,
    std::vector<Pattern> alternatives)
{
    frame.alternatives.insert(
        frame.alternatives.end(),
        std::make_move_iterator(alternatives.begin()),
        std::make_move_iterator(alternatives.end()));
}

void Parser::PatternAlternativeExpander::appendChildAlternatives(
    Frame& frame,
    std::vector<Pattern> alternatives)
{
    if (frame.alternatives.empty() || alternatives.empty())
    {
        frame.alternatives.clear();
        return;
    }
    if (frame.alternatives.size() == 1 && alternatives.size() == 1)
    {
        frame.alternatives.front().children.push_back(std::move(alternatives.front()));
        return;
    }
    std::vector<Pattern> product;
    product.reserve(frame.alternatives.size() * alternatives.size());
    for (const Pattern& prefix : frame.alternatives)
    {
        for (const Pattern& alternative : alternatives)
        {
            Pattern copy = prefix;
            copy.children.push_back(alternative);
            product.push_back(std::move(copy));
        }
    }
    frame.alternatives = std::move(product);
}

void Parser::PatternAlternativeExpander::integrateCompletedChild(
    Frame& frame,
    std::vector<Pattern> alternatives)
{
    if (frame.source->kind == PatternKind::Or)
        appendOrAlternatives(frame, std::move(alternatives));
    else
        appendChildAlternatives(frame, std::move(alternatives));
}

std::vector<Parser::Pattern> Parser::PatternAlternativeExpander::run()
{
    std::vector<Frame> frames;
    frames.push_back(makeFrame(root_));
    while (!frames.empty())
    {
        Frame& frame = frames.back();
        if (frame.nextChild < frame.source->children.size())
        {
            const Pattern& child = frame.source->children[frame.nextChild++];
            frames.push_back(makeFrame(child));
            continue;
        }
        std::vector<Pattern> completed = std::move(frame.alternatives);
        frames.pop_back();
        if (frames.empty()) return completed;
        integrateCompletedChild(frames.back(), std::move(completed));
    }
    return {};
}
}
