#pragma once

namespace nativesharp::llvm_backend
{
// EN: Outcome of the second-level object reuse keyed by the pre-optimization LLVM IR of a code
//     unit. `Off`: not attempted (no artifact cache, full debug information, ThinLTO, or the MIR
//     identity already hit). `Miss`: the IR key had no verified object, so the unit was optimized
//     and emitted. `Hit`: a verified object compiled from byte-identical IR was restored.
// ES: Resultado del reuso de objetos de segundo nivel por el IR de LLVM previo a la optimizacion
//     de una unidad. `Off`: no se intento (sin cache de artefactos, depuracion completa, ThinLTO,
//     o la identidad MIR ya acerto). `Miss`: la clave de IR no tenia objeto verificado y la unidad
//     se optimizo y emitio. `Hit`: se restauro un objeto verificado compilado desde IR identico.
enum class CodeUnitIrReuse
{
    Off,
    Miss,
    Hit
};

// EN: Stable report spelling of a CodeUnitIrReuse value (`off`, `miss`, `hit`).
// ES: Ortografia estable en el reporte de un valor CodeUnitIrReuse (`off`, `miss`, `hit`).
constexpr const char* codeUnitIrReuseName(CodeUnitIrReuse reuse) noexcept
{
    if (reuse == CodeUnitIrReuse::Hit)
    {
        return "hit";
    }
    if (reuse == CodeUnitIrReuse::Miss)
    {
        return "miss";
    }
    return "off";
}
}
