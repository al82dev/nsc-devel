#pragma once

#include "PrecompiledSnapshotReader.h"
#include "PrecompiledSnapshotWriter.h"

#include "nativesharp/Frontend/NamespaceFunctionSymbol.h"
#include "nativesharp/Frontend/NamespaceStateSymbol.h"

namespace nativesharp::frontend
{
// EN: Plan item 3.4. Encodes what namespace-function collection computes from the declaration.
//     Fields copied from the NamespaceFunctionDeclaration (accessibility, ordinals, FileId, range,
//     source path, module, namespace, name, syntax, definition, precompiled flag, the single part)
//     and the parameter FileId and ranges are not written; the installer fills them.
// ES: Punto 3.4 del plan. Codifica lo que la recolección de funciones de namespace calcula desde la
//     declaración. Los campos copiados de NamespaceFunctionDeclaration (accesibilidad, ordinales,
//     FileId, rango, ruta, módulo, namespace, nombre, sintaxis, definición, marca de precompilado,
//     la parte única) y el FileId y rango de los parámetros no se escriben; el instalador los llena.
void writeNamespaceFunctionSnapshot(PrecompiledSnapshotWriter& writer, const NamespaceFunctionSymbol& function);
bool readNamespaceFunctionSnapshot(PrecompiledSnapshotReader& reader, NamespaceFunctionSymbol& function);

// EN: Same for a namespace state: the declaration-copied fields are not written.
// ES: Igual para un estado de namespace: los campos copiados de la declaración no se escriben.
void writeNamespaceStateSnapshot(PrecompiledSnapshotWriter& writer, const NamespaceStateSymbol& state);
bool readNamespaceStateSnapshot(PrecompiledSnapshotReader& reader, NamespaceStateSymbol& state);
}
