#include "nativesharp/Target/TargetInfo.h"

#include <charconv>
#include <unordered_set>

namespace nativesharp::target
{
TargetInfo::TargetInfo(
    Architecture architecture,
    OperatingSystem operatingSystem,
    ObjectFormat objectFormat,
    CpuFeatureLevel requestedCpuFeatureLevel,
    CpuFeatureLevel cpuFeatureLevel) noexcept
    : architecture_(architecture),
      operatingSystem_(operatingSystem),
      objectFormat_(objectFormat),
      requestedCpuFeatureLevel_(requestedCpuFeatureLevel),
      cpuFeatureLevel_(cpuFeatureLevel)
{
}

TargetInfo TargetInfo::WindowsX64()
{
    return TargetInfo(
        Architecture::X86_64,
        OperatingSystem::Windows,
        ObjectFormat::COFF);
}

TargetInfo TargetInfo::WindowsX64(CpuFeatureLevel preferred, CpuFeatureLevel available)
{
    const auto rank = [](CpuFeatureLevel level) noexcept {
        return static_cast<unsigned>(level);
    };
    const CpuFeatureLevel selected = rank(preferred) <= rank(available) ? preferred : available;
    return TargetInfo(
        Architecture::X86_64,
        OperatingSystem::Windows,
        ObjectFormat::COFF,
        preferred,
        selected);
}

TargetInfo TargetInfo::WindowsArm64()
{
    return TargetInfo(
        Architecture::Arm64,
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
    return architecture_ == Architecture::Arm64 ? "arm64" : "x86-64";
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
    return architecture_ == Architecture::Arm64
        ? "aarch64-pc-windows-msvc" : "x86_64-pc-windows-msvc";
}

std::string_view TargetInfo::abiName() const noexcept
{
    return architecture_ == Architecture::Arm64
        ? "Microsoft Windows ARM64 ABI" : "Microsoft Windows x64 ABI";
}

std::string_view TargetInfo::cpuBaselineName() const noexcept
{
    if (architecture_ == Architecture::Arm64) return "armv8-a";
    switch (cpuFeatureLevel_)
    {
        case CpuFeatureLevel::Baseline:
            return "x86-64-v1";
        case CpuFeatureLevel::X86_64_V2:
            return "x86-64-v2";
        case CpuFeatureLevel::X86_64_V3:
            return "x86-64-v3";
        case CpuFeatureLevel::X86_64_V4:
            return "x86-64-v4";
    }
    return {};
}

std::string_view TargetInfo::llvmCpuName() const noexcept
{
    if (architecture_ == Architecture::Arm64) return "generic";
    return cpuFeatureLevel_ == CpuFeatureLevel::Baseline ? "x86-64" : cpuBaselineName();
}

std::string_view TargetInfo::llvmFeatureString() const noexcept
{
    if (architecture_ == Architecture::Arm64) return "+neon,+fp-armv8";
    switch (cpuFeatureLevel_)
    {
        case CpuFeatureLevel::Baseline:
            return "+sse,+sse2";
        case CpuFeatureLevel::X86_64_V2:
            return "+sse3,+ssse3,+sse4.1,+sse4.2,+popcnt";
        case CpuFeatureLevel::X86_64_V3:
            return "+avx,+avx2,+bmi,+bmi2,+f16c,+fma,+lzcnt,+movbe,+xsave";
        case CpuFeatureLevel::X86_64_V4:
            return "+avx512f,+avx512bw,+avx512cd,+avx512dq,+avx512vl";
    }
    return {};
}

CpuFeatureLevel TargetInfo::requestedCpuFeatureLevel() const noexcept
{
    return requestedCpuFeatureLevel_;
}

CpuFeatureLevel TargetInfo::cpuFeatureLevel() const noexcept
{
    return cpuFeatureLevel_;
}

bool TargetInfo::usedCpuFeatureFallback() const noexcept
{
    return requestedCpuFeatureLevel_ != cpuFeatureLevel_;
}

bool TargetInfo::supportsCpuFeature(std::string_view feature) const noexcept
{
    if (architecture_ == Architecture::Arm64)
        return feature.empty() || feature == "neon" || feature == "fp-armv8";
    if (feature.empty() || feature == "x87" || feature == "mmx" ||
        feature == "sse" || feature == "sse2") return true;
    if (cpuFeatureLevel_ == CpuFeatureLevel::Baseline) return false;
    if (feature == "sse3" || feature == "ssse3" || feature == "sse4.1" ||
        feature == "sse4.2" || feature == "popcnt") return true;
    if (cpuFeatureLevel_ == CpuFeatureLevel::X86_64_V2) return false;
    if (feature == "avx" || feature == "avx2" || feature == "bmi" ||
        feature == "bmi2" || feature == "f16c" || feature == "fma" ||
        feature == "lzcnt" || feature == "movbe" || feature == "xsave") return true;
    if (cpuFeatureLevel_ == CpuFeatureLevel::X86_64_V3) return false;
    return feature == "avx512f" || feature == "avx512bw" ||
        feature == "avx512cd" || feature == "avx512dq" || feature == "avx512vl";
}

bool TargetInfo::isAssemblyRegister(std::string_view name) const noexcept
{
    if (architecture_ == Architecture::Arm64)
    {
        const auto numberedArm = [&](std::string_view prefix, unsigned maximum)
        {
            if (!name.starts_with(prefix) || name.size() == prefix.size()) return false;
            unsigned ordinal = 0;
            const std::string_view digits = name.substr(prefix.size());
            const auto parsed = std::from_chars(digits.data(), digits.data() + digits.size(), ordinal);
            return parsed.ec == std::errc{} && parsed.ptr == digits.data() + digits.size() &&
                ordinal <= maximum;
        };
        return name == "sp" || name == "fp" || name == "lr" || name == "xzr" || name == "wzr" ||
            numberedArm("x", 30) || numberedArm("w", 30) || numberedArm("v", 31);
    }
    static const std::unordered_set<std::string_view> registers = {
        "rax", "eax", "ax", "al", "ah", "rbx", "ebx", "bx", "bl", "bh",
        "rcx", "ecx", "cx", "cl", "ch", "rdx", "edx", "dx", "dl", "dh",
        "rsi", "esi", "si", "sil", "rdi", "edi", "di", "dil",
        "rbp", "ebp", "bp", "bpl", "rsp", "esp", "sp", "spl"};
    if (registers.contains(name)) return true;
    const auto numbered = [&](std::string_view prefix, unsigned maximum,
                              std::initializer_list<std::string_view> suffixes)
    {
        if (!name.starts_with(prefix)) return false;
        std::string_view rest = name.substr(prefix.size());
        for (const std::string_view suffix : suffixes)
        {
            if (!suffix.empty() && rest.ends_with(suffix)) rest.remove_suffix(suffix.size());
            if (rest.empty()) continue;
            unsigned ordinal = 0;
            const auto parsed = std::from_chars(rest.data(), rest.data() + rest.size(), ordinal);
            if (parsed.ec == std::errc{} && parsed.ptr == rest.data() + rest.size() &&
                ordinal <= maximum) return true;
        }
        return false;
    };
    if (numbered("r", 15, {"d", "w", "b", ""}))
    {
        unsigned ordinal = 0;
        std::string_view digits = name.substr(1);
        while (!digits.empty() && (digits.back() == 'd' || digits.back() == 'w' || digits.back() == 'b'))
            digits.remove_suffix(1);
        std::from_chars(digits.data(), digits.data() + digits.size(), ordinal);
        return ordinal >= 8;
    }
    return numbered("xmm", 15, {""}) || numbered("mm", 7, {""}) ||
        numbered("st", 7, {""});
}

std::string_view TargetInfo::requiredAssemblyFeature(std::string_view mnemonic) const noexcept
{
    if (architecture_ == Architecture::Arm64) return {};
    if (mnemonic.starts_with("v")) return "avx";
    if (mnemonic.starts_with("aes")) return "aes";
    if (mnemonic.starts_with("pclmul")) return "pclmul";
    if (mnemonic == "popcnt") return "popcnt";
    if (mnemonic == "lzcnt") return "lzcnt";
    if (mnemonic == "crc32" || mnemonic.starts_with("pcmpestr") ||
        mnemonic.starts_with("pcmpistr")) return "sse4.2";
    static const std::unordered_set<std::string_view> bmi = {
        "andn", "bextr", "blsi", "blsmsk", "blsr", "bzhi", "mulx", "pdep", "pext",
        "rorx", "sarx", "shlx", "shrx"};
    if (bmi.contains(mnemonic)) return "bmi";
    return {};
}
}
