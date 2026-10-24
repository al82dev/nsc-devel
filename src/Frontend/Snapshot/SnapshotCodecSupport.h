#pragma once

#include "PrecompiledSnapshotReader.h"
#include "PrecompiledSnapshotWriter.h"

#include "nativesharp/Frontend/MethodParameterSymbol.h"
#include "nativesharp/Language/EscapeClass.h"

#include <cstdint>
#include <initializer_list>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace nativesharp::frontend
{
// EN: Encoding helpers shared by the model sections of the semantic snapshot (plan items 3.3, 3.4).
//     Every write has a read counterpart that consumes exactly the same bytes.
// ES: Ayudantes de codificación compartidos por las secciones de modelo del snapshot semántico
//     (puntos 3.3 y 3.4 del plan). Cada escritura tiene su lectura que consume los mismos bytes.
void writeSnapshotFlags(PrecompiledSnapshotWriter& writer, std::initializer_list<bool> flags);
void readSnapshotFlags(PrecompiledSnapshotReader& reader, std::initializer_list<bool*> flags);
void writeSnapshotOptional(PrecompiledSnapshotWriter& writer, const std::optional<std::uint64_t>& value);
std::optional<std::uint64_t> readSnapshotOptional(PrecompiledSnapshotReader& reader);
void writeSnapshotTexts(PrecompiledSnapshotWriter& writer, const std::vector<std::string>& values);
std::vector<std::string> readSnapshotTexts(PrecompiledSnapshotReader& reader);
void writeSnapshotTextPairs(
    PrecompiledSnapshotWriter& writer,
    const std::vector<std::pair<std::string, std::string>>& values);
std::vector<std::pair<std::string, std::string>> readSnapshotTextPairs(PrecompiledSnapshotReader& reader);
void writeSnapshotEscapes(PrecompiledSnapshotWriter& writer, const std::vector<language::EscapeClass>& values);
std::vector<language::EscapeClass> readSnapshotEscapes(PrecompiledSnapshotReader& reader);

// EN: A method parameter except FileId and range, which the installer derives from the
//     declaration and the parameter node.
// ES: Un parámetro de método salvo FileId y rango, que el instalador deriva de la declaración y
//     del nodo del parámetro.
void writeSnapshotParameter(PrecompiledSnapshotWriter& writer, const MethodParameterSymbol& parameter);
bool readSnapshotParameter(PrecompiledSnapshotReader& reader, MethodParameterSymbol& parameter);
void writeSnapshotParameters(PrecompiledSnapshotWriter& writer, const std::vector<MethodParameterSymbol>& parameters);
bool readSnapshotParameters(PrecompiledSnapshotReader& reader, std::vector<MethodParameterSymbol>& parameters);
}
