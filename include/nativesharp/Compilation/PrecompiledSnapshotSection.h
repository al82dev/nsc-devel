#pragma once

#include <string>

namespace nativesharp::compilation
{
// EN: The entries one semantic model captured for the public API unit, as an opaque payload that
//     only that model writes and reads.
// ES: Las entradas que un modelo semántico capturó para la unidad de API pública, como un payload
//     opaco que solo ese modelo escribe y lee.
struct PrecompiledSnapshotSection final
{
    std::string model;
    std::string payload;
};
}
