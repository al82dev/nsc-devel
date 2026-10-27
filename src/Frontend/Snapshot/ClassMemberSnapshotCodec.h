#pragma once

#include "PrecompiledSnapshotReader.h"
#include "PrecompiledSnapshotWriter.h"

#include "nativesharp/Frontend/ClassMemberSymbol.h"

namespace nativesharp::frontend
{
// EN: Plan item 3.3. Encodes the part of a ClassMemberSymbol that member collection computes from
//     the declaration and that the consumer cannot rebuild cheaply. Fields derived from the
//     collection position or the consumer unit (type, part, source and declaration ordinals,
//     FileId, ranges, syntax node, source path) and the generic specialization fields are not
//     written; the installer fills them exactly as collection does.
// ES: Punto 3.3 del plan. Codifica la parte de un ClassMemberSymbol que la recolección de miembros
//     calcula desde la declaración y que el consumidor no puede reconstruir de forma barata. Los
//     campos derivados de la posición de recolección o de la unidad consumidora (ordinales de tipo,
//     parte, fuente y declaración, FileId, rangos, nodo de sintaxis, ruta) y los campos de
//     especialización genérica no se escriben; el instalador los llena igual que la recolección.
void writeClassMemberSnapshot(PrecompiledSnapshotWriter& writer, const ClassMemberSymbol& member);

// EN: Reads a member written by writeClassMemberSnapshot; false when the bytes do not read.
// ES: Lee un miembro escrito por writeClassMemberSnapshot; false si los bytes no se leen.
bool readClassMemberSnapshot(PrecompiledSnapshotReader& reader, ClassMemberSymbol& member);
}
