#pragma once

#include "nativesharp/Driver/CompilerLanguage.h"

namespace nativesharp::driver
{
class CompilerLanguageScope final
{
  public:
    explicit CompilerLanguageScope(CompilerLanguage language) noexcept;
    ~CompilerLanguageScope();

    CompilerLanguageScope(const CompilerLanguageScope &) = delete;
    CompilerLanguageScope &operator=(const CompilerLanguageScope &) = delete;

  private:
    CompilerLanguage previous_;
};
} // namespace nativesharp::driver
