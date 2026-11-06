#include "nativesharp/LLVM/CppLifetimeObjectEmitter.h"
#include "nativesharp/Interop/CppSemanticFunction.h"
#include "nativesharp/LLVM/LlvmModule.h"
#include "nativesharp/LLVM/LlvmTargetMachine.h"
#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <Windows.h>
#endif

namespace nativesharp::llvm_backend
{
bool emitCStaticInlineObject(const interop::CppInteropSession& session,
    const std::vector<interop::CppCInlineIRExport>& exports, LlvmOptimizationLevel level,
    const std::filesystem::path& output, std::string& error)
{
    error.clear();
    if (session.optionsSnapshot().targetTriple != "x86_64-pc-windows-msvc" || exports.empty() ||
        output.empty() || !output.is_absolute())
    { error = "C inline object requires a Windows x64 session, exports and absolute output path"; return false; }
    const auto ir = session.emitCStaticInlineIR(exports);
    if (!ir) { error = "cannot emit C static inline definitions through Clang"; return false; }
    auto module = LlvmModule::parseText(*ir, error);
    if (!module) return false;
    auto target = LlvmTargetMachine::createWindowsX64(level, error);
    if (!target) return false;
    return target->emit(*module, output, LlvmCodeGenFileType::Object, error);
}

bool emitCppImplicitLifetimeObject(const interop::CppInteropSession& session,
    std::string_view qualifiedType, interop::CppImplicitLifetimeKind kind,
    LlvmOptimizationLevel level, const std::filesystem::path& output, std::string& error)
{
    error.clear();
    if (session.optionsSnapshot().targetTriple != "x86_64-pc-windows-msvc" || qualifiedType.empty() ||
        output.empty() || !output.is_absolute())
    {
        error = "C++ lifetime object requires a Windows x64 session, type and absolute output path";
        return false;
    }
    const auto ir = session.emitImplicitLifetimeIR(qualifiedType, kind);
    if (!ir)
    {
        error = "cannot emit the requested implicit C++ lifetime member for '" + std::string(qualifiedType) + "'";
        return false;
    }
    auto module = LlvmModule::parseText(*ir, error);
    if (!module) return false;
    auto target = LlvmTargetMachine::createWindowsX64(level, error);
    if (!target) return false;
    return target->emit(*module, output, LlvmCodeGenFileType::Object, error);
}

bool emitCppImplicitLifetimeObjects(std::span<const interop::CppImplicitLifetimeEmissionRequest> requests,
    std::span<const std::filesystem::path> destinations, LlvmOptimizationLevel level,
    std::vector<std::filesystem::path>& output, std::string& error)
{
    if (requests.size() != destinations.size())
    { error = "C++ lifetime plan and destination counts differ"; return false; }
    std::vector<std::filesystem::path> canonicalDestinations;
    std::vector<bool> existingDestinations;
    canonicalDestinations.reserve(destinations.size());
    existingDestinations.reserve(destinations.size());
    for (std::size_t i = 0; i < requests.size(); ++i)
    {
        if (!requests[i].session || requests[i].qualifiedType.empty() || requests[i].abiSymbol.empty() ||
            destinations[i].empty() || !destinations[i].is_absolute())
        { error = "C++ lifetime plan contains an invalid session, symbol or destination"; return false; }
        std::error_code filesystemError;
        const auto canonical = std::filesystem::weakly_canonical(destinations[i], filesystemError);
        if (filesystemError)
        { error = "cannot resolve C++ lifetime destination: " + filesystemError.message(); return false; }
        const bool exists = std::filesystem::exists(destinations[i], filesystemError);
        if (filesystemError)
        { error = "cannot inspect C++ lifetime destination: " + filesystemError.message(); return false; }
        for (std::size_t j = 0; j < i; ++j)
        {
#ifdef _WIN32
            const int compared = CompareStringOrdinal(canonical.c_str(), -1, canonicalDestinations[j].c_str(), -1, TRUE);
            if (compared == 0) { error = "cannot compare C++ lifetime destinations"; return false; }
            const bool samePath = compared == CSTR_EQUAL;
#else
            const bool samePath = canonical == canonicalDestinations[j];
#endif
            bool sameFile = false;
            if (exists && existingDestinations[j])
            {
                sameFile = std::filesystem::equivalent(destinations[i], destinations[j], filesystemError);
                if (filesystemError)
                { error = "cannot compare C++ lifetime files: " + filesystemError.message(); return false; }
            }
            if (samePath || sameFile)
            { error = "C++ lifetime destinations must be distinct"; return false; }
        }
        canonicalDestinations.push_back(canonical);
        existingDestinations.push_back(exists);
    }
    std::vector<std::filesystem::path> complete;
    complete.reserve(requests.size());
    for (std::size_t i = 0; i < requests.size(); ++i)
    {
        if (!emitCppImplicitLifetimeObject(*requests[i].session, requests[i].qualifiedType,
                requests[i].kind, level, destinations[i], error)) return false;
        complete.push_back(destinations[i]);
    }
    output = std::move(complete);
    error.clear();
    return true;
}
}
