#pragma once

#include <cstddef>
#include <string_view>

namespace nativesharp::language
{
// NativeSharp 3.0 T05-F07. Operators are language items, not ad-hoc method-name conventions.
// The spelling+arity pair is the source identity; langItem is the stable semantic identity used
// by trait requirements and witnesses; methodName is only the existing ABI spelling. Keeping the
// three in one table prevents parser, Sema and metadata reconstruction from inventing different
// mappings for the same operator.
struct OperatorTraitContract final
{
    std::string_view spelling;
    std::size_t arity = 0;
    std::string_view langItem;
    std::string_view methodName;
};
}
