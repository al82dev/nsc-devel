#include "nativesharp/Target/TargetInfo.h"

namespace nativesharp::target
{
TargetInfo::TargetInfo(
    Architecture architecture,
    OperatingSystem operatingSystem,
    ObjectFormat objectFormat) noexcept
    : architecture_(architecture),
      operatingSystem_(operatingSystem),
      objectFormat_(objectFormat)
{
}

TargetInfo TargetInfo::WindowsX64()
{
    return TargetInfo(
        Architecture::X86_64,
        OperatingSystem::Windows,
        ObjectFormat::COFF);
}

Architecture TargetInfo::architecture() const noexcept
{
    return architecture_;
}

OperatingSystem TargetInfo::operatingSystem() const noexcept
{
    return operatingSystem_;
}

ObjectFormat TargetInfo::objectFormat() const noexcept
{
    return objectFormat_;
}

std::string_view TargetInfo::architectureName() const noexcept
{
    return "x86-64";
}

std::string_view TargetInfo::operatingSystemName() const noexcept
{
    return "windows";
}

std::string_view TargetInfo::objectFormatName() const noexcept
{
    return "COFF";
}

std::string_view TargetInfo::llvmTriple() const noexcept
{
    return "x86_64-pc-windows-msvc";
}

std::string_view TargetInfo::abiName() const noexcept
{
    return "Microsoft Windows x64 ABI";
}
}
