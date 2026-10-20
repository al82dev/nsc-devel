#include "IdentityWriter.h"

namespace nativesharp::llvm_backend
{
IdentityWriter::IdentityWriter()
{
    output_.reserve(4096);
}

void IdentityWriter::text(std::string_view value)
{
    appendInteger(value.size());
    output_.push_back(':');
    output_.append(value);
    output_.push_back(';');
}

void IdentityWriter::texts(const std::vector<std::string>& values)
{
    scalar(values.size());
    for (const std::string& value : values) text(value);
}

std::string IdentityWriter::take()
{
    return std::move(output_);
}
}
