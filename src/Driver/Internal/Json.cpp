#include "Json.h"

namespace nativesharp::driver
{
const Json::Object *Json::object() const
{
    return std::get_if<Object>(&value);
}

const Json::Array *Json::array() const
{
    return std::get_if<Array>(&value);
}

const std::string *Json::string() const
{
    return std::get_if<std::string>(&value);
}

const bool *Json::boolean() const
{
    return std::get_if<bool>(&value);
}
} // namespace nativesharp::driver
