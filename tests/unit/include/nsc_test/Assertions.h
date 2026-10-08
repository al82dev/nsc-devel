#pragma once

#include "nsc_test/TestContext.h"

#include <sstream>
#include <string>
#include <string_view>

namespace nsc::test
{
inline void expectTrue(TestContext& context, bool value, std::string message)
{
    context.expect(value, std::move(message));
}

template <typename Actual, typename Expected>
void expectEqual(
    TestContext& context,
    const Actual& actual,
    const Expected& expected,
    std::string_view description)
{
    if (actual == expected)
    {
        return;
    }
    std::ostringstream message;
    message << description << ": actual=" << actual << ", expected=" << expected;
    context.fail(message.str());
}
}
