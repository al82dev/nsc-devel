#pragma once

#include "nativesharp/Basic/CompilationEvent.h"

#include <atomic>
#include <cstddef>
#include <functional>
#include <mutex>
#include <string>

namespace nativesharp::llvm_backend
{
class CodeUnitProgress final
{
public:
    CodeUnitProgress(
        const std::function<void(const basic::CompilationEvent&)>& sink,
        std::atomic_size_t& completed,
        std::mutex& mutex,
        std::size_t total,
        std::string name);

    ~CodeUnitProgress();

private:
    const std::function<void(const basic::CompilationEvent&)>& sink_;
    std::atomic_size_t& completed_;
    std::mutex& mutex_;
    std::size_t total_;
    std::string name_;
};
}
