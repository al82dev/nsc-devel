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
    // EN: LKF-250: objects and arrays recurse through parseValue; nesting deeper than this is
    //     rejected instead of exhausting the stack.
    // ES: LKF-250: objetos y arreglos recursan por parseValue; una anidación más profunda que esta
    //     se rechaza en lugar de agotar la pila.
    static constexpr std::size_t kMaximumDepth = 256;
    std::size_t depth_ = 0;
};
} // namespace nativesharp::driver
