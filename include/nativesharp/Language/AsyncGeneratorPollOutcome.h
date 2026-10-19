#pragma once

namespace nativesharp::language
{
enum class AsyncGeneratorPollOutcome { Pending, ItemReady, ReadyError, Completed };
}
