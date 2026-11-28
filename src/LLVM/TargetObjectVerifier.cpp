#include "nativesharp/LLVM/TargetObjectVerifier.h"

#include "nativesharp/LLVM/CoffObjectVerifier.h"
#include "nativesharp/LLVM/ElfObjectVerifier.h"
#include "nativesharp/Target/TargetInfo.h"

namespace nativesharp::llvm_backend
{
namespace
{
bool targetUsesElf()
{
    return target::TargetInfo::DefaultX64().objectFormat() == target::ObjectFormat::ELF;
}
}

bool TargetObjectVerifier::verify(const std::filesystem::path& path)
{
    info_ = {};
    error_.clear();
    if (targetUsesElf())
    {
        ElfObjectVerifier verifier;
        const bool verified = verifier.verify(path);
        info_.machine = verifier.info().machine;
        info_.sectionCount = verifier.info().sectionCount;
        error_ = verifier.error();
        return verified;
    }
    CoffObjectVerifier verifier;
    const bool verified = verifier.verify(path);
    info_.machine = verifier.info().machine;
    info_.sectionCount = verifier.info().sectionCount;
    error_ = verifier.error();
    return verified;
}

const TargetObjectInfo& TargetObjectVerifier::info() const noexcept
{
    return info_;
}

const std::string& TargetObjectVerifier::error() const noexcept
{
    return error_;
}

std::string_view TargetObjectVerifier::formatName() const noexcept
{
    if (targetUsesElf()) return "ELF";
    return "COFF";
}

std::string_view TargetObjectVerifier::machineName() const noexcept
{
    if (targetUsesElf()) return "x86-64 (62)";
    return "AMD64 (0x8664)";
}
}
