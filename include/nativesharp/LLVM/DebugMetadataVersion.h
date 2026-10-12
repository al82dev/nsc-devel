#pragma once

#include <cstdint>

namespace nativesharp::llvm_backend
{
// NSDEBUG 1.1 is the first NativeSharp-owned debug contract. It identifies the CodeView/PDB
// projection that preserves T01 static-initialization helpers and lexical condition/static locals.
inline constexpr std::uint32_t kDebugMetadataMajor = 1;
inline constexpr std::uint32_t kDebugMetadataMinor = 1;
}
