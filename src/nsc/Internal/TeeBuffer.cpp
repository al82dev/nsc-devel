#include "TeeBuffer.h"

#include <string_view>

namespace
{
std::string_view renderConciseLine(std::string_view line)
{
    const std::string_view original = line;
    if (line.ends_with('\n')) line.remove_suffix(1);
    if (line.ends_with('\r')) line.remove_suffix(1);
    if (line.find(" emission: ok") != std::string_view::npos)
        return "Build succeeded.\n";
    if (line.starts_with("NativeSharp Compiler ") ||
        line.starts_with("Copyright ") ||
        line.starts_with("Build hash: ") ||
        (line.starts_with('[') && line.find("] Compiling ") != std::string_view::npos))
        return original;
    return {};
}
}

namespace nsc_internal
{
TeeBuffer::TeeBuffer(std::streambuf* first, std::streambuf* second, const bool conciseFirst)
    : first_(first), second_(second), conciseFirst_(conciseFirst)
{
}

int TeeBuffer::overflow(const int value)
{
    if (value == traits_type::eof()) return traits_type::not_eof(value);
    const char character = static_cast<char>(value);
    const bool secondOk = second_ == nullptr || second_->sputc(character) != traits_type::eof();
    bool firstOk = true;
    if (first_ != nullptr && !conciseFirst_)
        firstOk = first_->sputc(character) != traits_type::eof();
    else if (first_ != nullptr)
    {
        firstLine_.push_back(character);
        if (character == '\n')
        {
            const std::string_view rendered = renderConciseLine(firstLine_);
            if (!rendered.empty())
                firstOk = first_->sputn(rendered.data(), rendered.size()) ==
                    static_cast<std::streamsize>(rendered.size());
            firstLine_.clear();
        }
    }
    return firstOk && secondOk ? value : traits_type::eof();
}

int TeeBuffer::sync()
{
    const bool firstOk = first_ == nullptr || first_->pubsync() == 0;
    const bool secondOk = second_ == nullptr || second_->pubsync() == 0;
    return firstOk && secondOk ? 0 : -1;
}
}
