#include "CodeUnitProgress.h"

#include <utility>

namespace nativesharp::llvm_backend
{
CodeUnitProgress::CodeUnitProgress(
    const std::function<void(const basic::CompilationEvent&)>& sink,
    std::atomic_size_t& completed,
    std::mutex& mutex,
    std::size_t total,
    std::string name)
    : sink_(sink), completed_(completed), mutex_(mutex), total_(total),
      name_(std::move(name))
{
    if (sink_)
        sink_({basic::CompilationEventKind::WorkStarted,
            "LLVM", name_, 0, 0, {}});
}

CodeUnitProgress::~CodeUnitProgress()
{
    if (!sink_) return;
    const std::lock_guard<std::mutex> lock(mutex_);
    const std::size_t current = completed_.fetch_add(1, std::memory_order_relaxed) + 1U;
    sink_({basic::CompilationEventKind::WorkFinished,
        "LLVM", name_, current, total_});
}
}
