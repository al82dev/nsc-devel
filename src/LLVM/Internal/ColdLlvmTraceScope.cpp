#include "ColdLlvmTraceScope.h"

#include "ColdLlvmTrace.h"

namespace nativesharp::llvm_backend
{
ColdLlvmTraceScope::ColdLlvmTraceScope(
    ColdLlvmTrace& trace,
    const CodeUnitTelemetry& telemetry,
    const std::uint8_t& hit,
    const std::uint8_t& miss,
    const std::string& error)
    : trace_(trace), telemetry_(telemetry), hit_(hit), miss_(miss), error_(error)
{
}

ColdLlvmTraceScope::~ColdLlvmTraceScope()
{
    trace_.record(telemetry_, hit_ != 0U, miss_ != 0U, error_);
}
}
