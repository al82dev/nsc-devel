#include "Stage37ConstantValue.h"

#include <utility>

namespace nativesharp::frontend::method_body_detail
{
Stage37ConstantValue::Stage37ConstantValue(
    Stage37ConstantKind valueKind,
    std::string valueTypeKey,
    std::uint64_t integerValue,
    double floatingValue,
    bool booleanValue,
    std::string stringValue)
    : kind(valueKind),
      typeKey(std::move(valueTypeKey)),
      integer(integerValue),
      floating(floatingValue),
      boolean(booleanValue),
      string(std::move(stringValue))
{
}
}
