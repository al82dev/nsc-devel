#pragma once

namespace nativesharp::llvm_backend
{
inline constexpr bool kColdLlvmTraceEnabled = false;
inline constexpr bool kLoweringPhaseTraceEnabled = false;
// ES: Traza Debug del programa generado (INSTRUMENTATION CODE, tanda NSC305): queda aparcada. Con `true`,
// los ejecutables -g vuelven a escribir A:\temp\nsc-runtime-debug-trace.log en cada llamada, reserva,
// liberación y acceso a campo.
// EN: Debug trace of the generated program (INSTRUMENTATION CODE, NSC305 tranche): parked. With `true`, -g
// executables write A:\temp\nsc-runtime-debug-trace.log again on every call, allocation, release and field
// access.
inline constexpr bool kRuntimeDebugTraceEnabled = false;
}
