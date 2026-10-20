#pragma once

#include <streambuf>

namespace nsc_internal
{
class TeeBuffer final : public std::streambuf
{
public:
    TeeBuffer(std::streambuf* first, std::streambuf* second);

protected:
    int overflow(int value) override;
    int sync() override;

private:
    std::streambuf* first_;
    std::streambuf* second_;
};
}
