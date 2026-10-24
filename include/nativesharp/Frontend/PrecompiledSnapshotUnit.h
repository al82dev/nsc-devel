#pragma once

#include "nativesharp/Compilation/PrecompiledSemanticSnapshot.h"
#include "nativesharp/Frontend/PrecompiledSyntaxNodeIndex.h"

#include <cstddef>
#include <string>

namespace nativesharp::frontend
{
// EN: A semantic snapshot accepted for one restored public API unit: the unit ordinal in
//     ProjectCompilation::units(), the decoded snapshot and the index of the unit's syntax tree the
//     snapshot references resolve against.
// ES: Un snapshot semántico aceptado para una unidad de API pública restaurada: el ordinal de la
//     unidad en ProjectCompilation::units(), el snapshot decodificado y el índice del árbol
//     sintáctico de la unidad contra el que se resuelven las referencias del snapshot.
struct PrecompiledSnapshotUnit final
{
    std::size_t unitOrdinal = 0;
    std::string moduleName;
    compilation::PrecompiledSemanticSnapshot snapshot;
    PrecompiledSyntaxNodeIndex index;
};
}
