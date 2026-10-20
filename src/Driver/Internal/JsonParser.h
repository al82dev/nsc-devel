#pragma once

#include <cstddef>
#include <string>
#include <string_view>

namespace nativesharp::driver
{
struct Json;

class JsonParser final
{
  public:
    explicit JsonParser(std::string_view text);

    bool parse(Json &value, std::string &error);

  private:
    bool fail(std::string &error, std::string_view message) const;
    void skip();
    bool parseValue(Json &value, std::string &error);
    bool parseString(std::string &value, std::string &error);
    bool parseObject(Json &value, std::string &error);
    bool parseArray(Json &value, std::string &error);

    std::string_view text_;
    std::size_t position_ = 0;
};
} // namespace nativesharp::driver
