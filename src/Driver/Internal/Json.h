#pragma once

#include <cstddef>
#include <map>
#include <string>
#include <variant>
#include <vector>

namespace nativesharp::driver
{
struct Json final
{
    using Object = std::map<std::string, Json>;
    using Array = std::vector<Json>;
    std::variant<std::nullptr_t, bool, double, std::string, Object, Array> value = nullptr;

    const Object *object() const;
    const Array *array() const;
    const std::string *string() const;
    const bool *boolean() const;
};
} // namespace nativesharp::driver
