#pragma once

#include "nativesharp/Target/Architecture.h"
#include "nativesharp/Target/CpuFeatureLevel.h"
#include "nativesharp/Target/ObjectFormat.h"
#include "nativesharp/Target/OperatingSystem.h"

#include <optional>
#include <string_view>

namespace nativesharp::target
{
class TargetInfo final
{
public:
    static TargetInfo WindowsX64();
    static TargetInfo WindowsX64(CpuFeatureLevel preferred, CpuFeatureLevel available);
    static TargetInfo WindowsArm64();
    static TargetInfo LinuxX64();
    static TargetInfo LinuxX64(CpuFeatureLevel preferred, CpuFeatureLevel available);
    // The x64 target of a compilation that selects none: Linux x64 when the build defines
    // NSC_DEFAULT_TARGET_LINUX_X64, Windows x64 otherwise.
    static TargetInfo DefaultX64();
    static TargetInfo DefaultX64(CpuFeatureLevel preferred, CpuFeatureLevel available);

    Architecture architecture() const noexcept;
    OperatingSystem operatingSystem() const noexcept;
    ObjectFormat objectFormat() const noexcept;

    std::string_view architectureName() const noexcept;
    std::string_view operatingSystemName() const noexcept;
    std::string_view objectFormatName() const noexcept;
    std::string_view llvmTriple() const noexcept;
    std::string_view abiName() const noexcept;
    std::string_view cpuBaselineName() const noexcept;
    std::string_view llvmCpuName() const noexcept;
    std::string_view llvmFeatureString() const noexcept;
    CpuFeatureLevel requestedCpuFeatureLevel() const noexcept;
    CpuFeatureLevel cpuFeatureLevel() const noexcept;
    bool usedCpuFeatureFallback() const noexcept;
    bool supportsCpuFeature(std::string_view feature) const noexcept;
    bool isAssemblyRegister(std::string_view name) const noexcept;
    std::string_view requiredAssemblyFeature(std::string_view mnemonic) const noexcept;

private:
    TargetInfo(
        Architecture architecture,
        OperatingSystem operatingSystem,
        ObjectFormat objectFormat,
        CpuFeatureLevel requestedCpuFeatureLevel = CpuFeatureLevel::Baseline,
        CpuFeatureLevel cpuFeatureLevel = CpuFeatureLevel::Baseline) noexcept;

    Architecture architecture_;
    OperatingSystem operatingSystem_;
    ObjectFormat objectFormat_;
    CpuFeatureLevel requestedCpuFeatureLevel_;
    CpuFeatureLevel cpuFeatureLevel_;
};

// EN: T19-036. The CPU feature level that the current compilation selected for Windows x64 (project property
//     CpuLevel or --cpu-level); Baseline when none was selected. Every LLVM target machine of the compilation and
//     its Clang sessions use it, so code and C/C++ ABI plans agree.
// ES: T19-036. El nivel de CPU que eligio la compilacion actual para Windows x64 (propiedad CpuLevel del proyecto
//     o --cpu-level); Baseline si no se eligio. Lo usan todas las maquinas destino LLVM de la compilacion y sus
//     sesiones de Clang, para que el codigo y los planes ABI C/C++ coincidan.
CpuFeatureLevel selectedWindowsX64CpuFeatureLevel() noexcept;

// EN: `x86-64` (or `x86-64-v1`), `x86-64-v2`, `x86-64-v3` or `x86-64-v4`; any other text gives nothing.
// ES: `x86-64` (o `x86-64-v1`), `x86-64-v2`, `x86-64-v3` o `x86-64-v4`; cualquier otro texto no da nada.
std::optional<CpuFeatureLevel> parseCpuFeatureLevel(std::string_view name) noexcept;

class CpuFeatureLevelScope final
{
public:
    explicit CpuFeatureLevelScope(CpuFeatureLevel level) noexcept;
    CpuFeatureLevelScope(const CpuFeatureLevelScope&) = delete;
    CpuFeatureLevelScope& operator=(const CpuFeatureLevelScope&) = delete;
    ~CpuFeatureLevelScope();

private:
    CpuFeatureLevel previous_;
};
}
