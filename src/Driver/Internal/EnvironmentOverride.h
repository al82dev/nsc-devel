#pragma once

#include <optional>
#include <string>

namespace nativesharp::driver
{
class EnvironmentOverride final
{
  public:
    EnvironmentOverride(std::string name, std::optional<std::string> value);
    ~EnvironmentOverride();

  private:
    void set(const std::string &value);
    void clear();

    std::string name_;
    std::optional<std::string> previous_;
    bool active_ = false;
};
} // namespace nativesharp::driver
