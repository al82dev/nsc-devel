#include "EnvironmentOverride.h"

#include "ProjectCommandSupport.h"

#include <cstdlib>
#include <utility>

namespace nativesharp::driver
{
EnvironmentOverride::EnvironmentOverride(std::string name, std::optional<std::string> value) : name_(std::move(name))
{
    if (!value.has_value())
        return;
    active_ = true;
    previous_ = environmentValue(name_.c_str());
    set(*value);
}

EnvironmentOverride::~EnvironmentOverride()
{
    if (!active_)
        return;
    if (previous_.has_value())
        set(*previous_);
    else
        clear();
}

void EnvironmentOverride::set(const std::string &value)
{
#if defined(_WIN32)
    _putenv_s(name_.c_str(), value.c_str());
#else
    setenv(name_.c_str(), value.c_str(), 1);
#endif
}

void EnvironmentOverride::clear()
{
#if defined(_WIN32)
    _putenv_s(name_.c_str(), "");
#else
    unsetenv(name_.c_str());
#endif
}
} // namespace nativesharp::driver
