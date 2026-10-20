#pragma once

#include <filesystem>

namespace nativesharp::driver
{
class TemporaryFile final
{
  public:
    explicit TemporaryFile(std::filesystem::path path);
    ~TemporaryFile();

  private:
    std::filesystem::path path_;
};
} // namespace nativesharp::driver
