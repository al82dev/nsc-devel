#pragma once

#include "nativesharp/Language/ExpressionErgonomics.h"

#include <string>
#include <string_view>

namespace nativesharp::language
{
class BuiltinStaticInterpolationHandler final : public StaticInterpolationHandler
{
public:
    explicit BuiltinStaticInterpolationHandler(std::string& result) noexcept;
    bool appendLiteral(std::string_view text, std::string& error) override;
    bool appendHole(const InterpolationPart& hole, std::string& error) override;

private:
    std::string& result_;
};
}
