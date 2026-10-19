#pragma once

namespace nativesharp::language
{
enum class AsyncGeneratorState { Pending, ItemReady, Completed, Error, Cancelled };
}
