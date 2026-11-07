#pragma once

#include "nativesharp/Interop/CppInteropSession.h"
#include "nativesharp/LLVM/LlvmOptimizationLevel.h"
#include <filesystem>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace nativesharp::interop { struct CppImplicitLifetimeEmissionRequest; struct CppCInlineIRExport; }

namespace nativesharp::llvm_backend
{
bool emitCStaticInlineObject(const interop::CppInteropSession& session,
    const std::vector<interop::CppCInlineIRExport>& exports,
    const std::vector<interop::CppCInlineIRExport>& globals, LlvmOptimizationLevel level,
    const std::filesystem::path& output, std::string& error);
// Experimental: produces one object, but does not schedule it in the project link.
bool emitCppImplicitLifetimeObject(const interop::CppInteropSession& session,
    std::string_view qualifiedType, interop::CppImplicitLifetimeKind kind,
    LlvmOptimizationLevel level, const std::filesystem::path& output, std::string& error);
// Publishes the output list only on complete success; partial files remain caller-owned on failure.
bool emitCppImplicitLifetimeObjects(std::span<const interop::CppImplicitLifetimeEmissionRequest> requests,
    std::span<const std::filesystem::path> destinations, LlvmOptimizationLevel level,
    std::vector<std::filesystem::path>& output, std::string& error);
}
