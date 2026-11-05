#pragma once

#include "nativesharp/Interop/CppInteropSession.h"

#include <atomic>
#include <memory>
#include <string>
#include <variant>
#include <vector>

namespace nativesharp::interop
{
// Copies share an owned cancellation flag and do not borrow a caller's stack.
class CppCancellationToken
{
public:
    CppCancellationToken() : flag_(std::make_shared<std::atomic_bool>(false)) {}
    void cancel() const noexcept { flag_->store(true, std::memory_order_relaxed); }
    bool cancelled() const noexcept { return flag_->load(std::memory_order_relaxed); }

private:
    std::shared_ptr<std::atomic_bool> flag_;
};

class CppInteropSessionFactory
{
public:
    virtual ~CppInteropSessionFactory() = default;
    virtual CppSessionOpenResult open(CppSessionOptions options, CppCancellationToken cancellation) = 0;
};
}
