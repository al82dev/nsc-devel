#include "TeeBuffer.h"

namespace nsc_internal
{
TeeBuffer::TeeBuffer(std::streambuf* first, std::streambuf* second)
    : first_(first), second_(second)
{
}

int TeeBuffer::overflow(const int value)
{
    if (value == traits_type::eof()) return traits_type::not_eof(value);
    const char character = static_cast<char>(value);
    const bool firstOk = first_ == nullptr || first_->sputc(character) != traits_type::eof();
    const bool secondOk = second_ == nullptr || second_->sputc(character) != traits_type::eof();
    return firstOk && secondOk ? value : traits_type::eof();
}

int TeeBuffer::sync()
{
    const bool firstOk = first_ == nullptr || first_->pubsync() == 0;
    const bool secondOk = second_ == nullptr || second_->pubsync() == 0;
    return firstOk && secondOk ? 0 : -1;
}
}
