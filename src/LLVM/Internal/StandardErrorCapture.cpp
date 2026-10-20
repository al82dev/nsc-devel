#include "StandardErrorCapture.h"

#if !defined(_WIN32)
#include <cstdio>
#include <fcntl.h>
#include <fstream>
#include <sstream>
#include <sys/types.h>
#include <unistd.h>

namespace nativesharp::llvm_backend
{
StandardErrorCapture::StandardErrorCapture(std::string path) : path_(std::move(path))
{
    saved_ = ::dup(2);
    if (saved_ == -1) return;
    const int file = ::open(path_.c_str(), O_WRONLY | O_CREAT | O_TRUNC, 0600);
    if (file == -1) { ::close(saved_); saved_ = -1; return; }
    active_ = ::dup2(file, 2) != -1;
    ::close(file);
}

StandardErrorCapture::~StandardErrorCapture()
{
    restore();
}

bool StandardErrorCapture::active() const noexcept
{
    return active_;
}

void StandardErrorCapture::restore()
{
    if (saved_ == -1) return;
    ::dup2(saved_, 2);
    ::close(saved_);
    saved_ = -1;
    active_ = false;
}

std::string StandardErrorCapture::text() const
{
    std::ifstream file(path_, std::ios::binary);
    if (!file) return {};
    std::ostringstream buffer;
    buffer << file.rdbuf();
    return buffer.str();
}

void StandardErrorCapture::remove() const
{
    std::remove(path_.c_str());
}
}
#endif
