#pragma once

#include <string_view>

namespace nativesharp::target
{
enum class Architecture
{
    X86_64,
    Arm64
};

enum class OperatingSystem
{
    Windows
};

enum class ObjectFormat
{
    COFF
};

class TargetInfo final
{
public:
    static TargetInfo WindowsX64();
    static TargetInfo WindowsArm64();

    Architecture architecture() const noexcept;
    OperatingSystem operatingSystem() const noexcept;
    ObjectFormat objectFormat() const noexcept;

    std::string_view architectureName() const noexcept;
    std::string_view operatingSystemName() const noexcept;
    std::string_view objectFormatName() const noexcept;
    std::string_view llvmTriple() const noexcept;
    std::string_view abiName() const noexcept;
    std::string_view cpuBaselineName() const noexcept;
    bool supportsCpuFeature(std::string_view feature) const noexcept;
    bool isAssemblyRegister(std::string_view name) const noexcept;
    std::string_view requiredAssemblyFeature(std::string_view mnemonic) const noexcept;

private:
    TargetInfo(
        Architecture architecture,
        OperatingSystem operatingSystem,
        ObjectFormat objectFormat) noexcept;

    Architecture architecture_;
    OperatingSystem operatingSystem_;
    ObjectFormat objectFormat_;
};
}
