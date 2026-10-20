#pragma once

#include <cstddef>
#include <ostream>
#include <streambuf>
#include <string>

namespace nativesharp::driver
{
class ProgressJsonForwardingBuffer final : public std::streambuf
{
  public:
    ProgressJsonForwardingBuffer(std::ostream &forwarded, std::string &retained);

    void finish();

  protected:
    int_type overflow(int_type character) override;
    std::streamsize xsputn(const char *characters, std::streamsize count) override;
    int sync() override;

  private:
    void append(const char *characters, std::size_t count);
    void flushLine();
    void retainLine();

    std::ostream &forwarded_;
    std::string &retained_;
    std::string line_;
};
} // namespace nativesharp::driver
