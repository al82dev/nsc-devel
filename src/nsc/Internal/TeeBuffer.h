#pragma once

#include <streambuf>
#include <string>

namespace nsc_internal
{
class TeeBuffer final : public std::streambuf
{
public:
    TeeBuffer(std::streambuf* first, std::streambuf* second, bool conciseFirst = false);

protected:
    int overflow(int value) override;
    int sync() override;

private:
    std::streambuf* first_;
    std::streambuf* second_;
    bool conciseFirst_ = false;
    std::string firstLine_;
};
}
