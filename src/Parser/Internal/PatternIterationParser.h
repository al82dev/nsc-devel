#pragma once

#include "nativesharp/Parser/Parser.h"

namespace nativesharp::parser
{
class Parser::PatternIterationParser final
{
public:
    PatternIterationParser(Parser& parser, bool implicitBinding);

    Pattern run();

private:
    enum class WorkKind
    {
        ParseOr,
        FinishOr,
        ParseAnd,
        FinishAnd,
        ParsePrimary,
        FinishNot,
        ContinueList,
        FinishListChild,
        FinishParenthesizedFirst,
        ContinueTuple,
        FinishTupleChild,
        ContinueStruct,
        FinishStructChild,
        ContinueNamedPayload,
        FinishNamedPayloadChild
    };

    struct Work final
    {
        WorkKind kind = WorkKind::ParseOr;
        bool implicitBinding = false;
        bool initial = false;
        std::size_t begin = 0;
        Pattern pattern;
        std::unordered_set<std::string> fields;
    };

    void schedule(WorkKind kind, bool implicitBinding = false, std::size_t begin = 0);
    void schedule(Work work);
    void dispatch(Work work);
    Pattern takeValue();
    void parseOr(bool implicitBinding);
    void finishOr(Work work);
    void parseAnd(bool implicitBinding);
    void finishAnd(Work work);
    void parsePrimary(bool implicitBinding);
    void finishNot(std::size_t begin);
    void continueList(Work work);
    void finishListChild(Work work);
    void finishListElement(Work work, Pattern child);
    void finishParenthesizedFirst(std::size_t begin);
    void continueTuple(Work work);
    void finishTupleChild(Work work);
    void finishTuple(Work work);
    void startStruct(Pattern pattern, std::size_t begin);
    void continueStruct(Work work);
    void finishStructChild(Work work);
    void finishStructElement(Work work, Pattern child);
    void parseNamed(std::size_t begin, bool implicitBinding);
    void continueNamedPayload(Work work);
    void finishNamedPayloadChild(Work work);
    void finishNamedPayload(Work work);

    Parser& parser_;
    bool implicitBinding_ = false;
    std::vector<Work> work_;
    std::vector<Pattern> values_;
};
}
