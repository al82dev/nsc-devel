#pragma once

#include "nativesharp/Language/FutureCompletion.h"
#include "nativesharp/Language/PollOutcome.h"
#include "nativesharp/Language/PollState.h"

#include <string>

namespace nativesharp::language
{
class FutureState final
{
public:
    PollState state() const noexcept { return state_; }
    FutureCompletion completion() const noexcept { return completion_; }
    bool poll(PollOutcome outcome, std::string& error);
    bool poll(bool ready, std::string& error);
    bool consume(std::string& error);
    bool cancel(std::string& error);
private:
    PollState state_ = PollState::Created;
    FutureCompletion completion_ = FutureCompletion::None;
};
}
