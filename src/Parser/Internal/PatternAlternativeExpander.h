#pragma once

#include "nativesharp/Parser/Parser.h"

namespace nativesharp::parser
{
class Parser::PatternAlternativeExpander final
{
public:
    explicit PatternAlternativeExpander(const Pattern& root);

    std::vector<Pattern> run();

private:
    struct Frame final
    {
        const Pattern* source = nullptr;
        std::size_t nextChild = 0;
        std::vector<Pattern> alternatives;
    };

    static Pattern copyWithoutChildren(const Pattern& source);
    static Frame makeFrame(const Pattern& source);
    static void appendOrAlternatives(
        Frame& frame,
        std::vector<Pattern> alternatives);
    static void appendChildAlternatives(
        Frame& frame,
        std::vector<Pattern> alternatives);
    static void integrateCompletedChild(
        Frame& frame,
        std::vector<Pattern> alternatives);

    const Pattern& root_;
};
}
