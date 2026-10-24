#pragma once

#include "nativesharp/Compilation/PrecompiledSemanticSnapshot.h"

#include <string>
#include <string_view>

namespace nativesharp::compilation
{
// EN: Byte format of PrecompiledSemanticSnapshot inside the v2 sidecar. decode() accepts only the
//     exact layout written by encode(): known schema, no truncation and no trailing bytes.
// ES: Formato en bytes de PrecompiledSemanticSnapshot dentro del sidecar v2. decode() solo acepta
//     el formato exacto que escribe encode(): esquema conocido, sin truncar y sin bytes sobrantes.
class PrecompiledSemanticSnapshotCodec final
{
public:
    static std::string encode(const PrecompiledSemanticSnapshot& snapshot);

    static bool decode(
        std::string_view bytes,
        PrecompiledSemanticSnapshot& snapshot,
        std::string& error);
};
}
