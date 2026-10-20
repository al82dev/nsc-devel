#include "ProgressJsonForwardingBuffer.h"

#include <string_view>

namespace nativesharp::driver
{
namespace
{
constexpr std::string_view kProgressJsonPrefix = "{\"schema\":\"nativesharp-progress-event\"";
}

ProgressJsonForwardingBuffer::ProgressJsonForwardingBuffer(std::ostream &forwarded, std::string &retained)
    : forwarded_(forwarded), retained_(retained)
{
}

void ProgressJsonForwardingBuffer::finish()
{
    if (!line_.empty())
        retainLine();
}

ProgressJsonForwardingBuffer::int_type ProgressJsonForwardingBuffer::overflow(int_type character)
{
    if (traits_type::eq_int_type(character, traits_type::eof()))
        return traits_type::not_eof(character);
    const char value = traits_type::to_char_type(character);
    append(&value, 1U);
    return traits_type::not_eof(character);
}

std::streamsize ProgressJsonForwardingBuffer::xsputn(const char *characters, std::streamsize count)
{
    if (count <= 0)
        return 0;
    append(characters, static_cast<std::size_t>(count));
    return count;
}

int ProgressJsonForwardingBuffer::sync()
{
    return 0;
}

void ProgressJsonForwardingBuffer::append(const char *characters, std::size_t count)
{
    for (std::size_t index = 0; index < count; ++index)
    {
        line_.push_back(characters[index]);
        if (characters[index] == '\n')
            flushLine();
    }
}

void ProgressJsonForwardingBuffer::flushLine()
{
    if (line_.starts_with(kProgressJsonPrefix))
    {
        forwarded_ << line_;
        forwarded_.flush();
        line_.clear();
        return;
    }
    retainLine();
}

void ProgressJsonForwardingBuffer::retainLine()
{
    retained_ += line_;
    line_.clear();
}
} // namespace nativesharp::driver
