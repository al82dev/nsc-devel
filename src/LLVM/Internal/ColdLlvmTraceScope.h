#pragma once

#include "nativesharp/LLVM/CodeUnitTelemetry.h"

#include <cstdint>
#include <string>

namespace nativesharp::llvm_backend
{
class ColdLlvmTrace;

class ColdLlvmTraceScope final
{
public:
    ColdLlvmTraceScope(
        ColdLlvmTrace& trace,
        const CodeUnitTelemetry& telemetry,
        const std::uint8_t& hit,
        const std::uint8_t& miss,
        const std::string& error);

    ~ColdLlvmTraceScope();

    ColdLlvmTraceScope(const ColdLlvmTraceScope&) = delete;
    ColdLlvmTraceScope& operator=(const ColdLlvmTraceScope&) = delete;

private:
    ColdLlvmTrace& trace_;
    const CodeUnitTelemetry& telemetry_;
    const std::uint8_t& hit_;
    const std::uint8_t& miss_;
    const std::string& error_;
};
}
