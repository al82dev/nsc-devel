#pragma once

#include <string_view>

namespace nativesharp::target
{
enum class Architecture
{
    X86_64
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

    Architecture architecture() const noexcept;
    OperatingSystem operatingSystem() const noexcept;
    ObjectFormat objectFormat() const noexcept;

    std::string_view architectureName() const noexcept;
    std::string_view operatingSystemName() const noexcept;
    std::string_view objectFormatName() const noexcept;
    std::string_view llvmTriple() const noexcept;
    std::string_view abiName() const noexcept;

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
