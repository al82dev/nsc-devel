#pragma once

#include "nativesharp/Language/AsyncGeneratorPollOutcome.h"
#include "nativesharp/Language/AsyncGeneratorState.h"

#include <string>

namespace nativesharp::language
{
class AsyncGeneratorProtocolState final
{
public:
    AsyncGeneratorState state() const noexcept { return state_; }
    bool demandOutstanding() const noexcept { return demandOutstanding_; }
    bool itemOutstanding() const noexcept { return itemOutstanding_; }
    bool requestNext(std::string& error);
    bool resume(AsyncGeneratorPollOutcome outcome, std::string& error);
    bool takeItem(std::string& error);
    bool stopEarly(std::string& error);
private:
    AsyncGeneratorState state_ = AsyncGeneratorState::Pending;
    bool demandOutstanding_ = false;
    bool itemOutstanding_ = false;
};
}
