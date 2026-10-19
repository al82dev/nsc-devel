#pragma once

#include "nativesharp/Language/SmartPointerFamily.h"
#include "nativesharp/Language/SmartPointerOperation.h"

#include <cstdint>

namespace nativesharp::language
{
struct SmartPointerOperationContext final
{
    SmartPointerFamily family = SmartPointerFamily::Unique;
    SmartPointerOperation operation = SmartPointerOperation::Move;
    std::uint64_t strongCount = 1;
    std::uint64_t weakCount = 0;
    bool payloadSendSync = false;
    bool producerModulePinned = true;
};
}
