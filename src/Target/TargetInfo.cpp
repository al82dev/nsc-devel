#include "nativesharp/Target/TargetInfo.h"

#include <charconv>
#include <unordered_set>

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

std::string_view TargetInfo::cpuBaselineName() const noexcept
{
    return "x86-64-v1";
}

bool TargetInfo::supportsCpuFeature(std::string_view feature) const noexcept
{
    return feature.empty() || feature == "x87" || feature == "mmx" ||
        feature == "sse" || feature == "sse2";
}

bool TargetInfo::isAssemblyRegister(std::string_view name) const noexcept
{
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
