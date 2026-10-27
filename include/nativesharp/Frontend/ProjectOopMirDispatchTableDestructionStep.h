#pragma once

#include "nativesharp/Frontend/ProjectOopMirDispatchTable.h"

namespace nativesharp::frontend
{
struct ProjectOopMirDispatchTable::DestructionStep final
{
    std::string destructorCalleeName;
    OwnedField ownedField;
    bool isFieldRelease = false;
    // EN: Freeze 3.0 C-11: a value field with a non-trivial lifetime (a struct with a destructor
    //     or owned fields) is dropped in place; ownedField carries its member and offset.
    // ES: Congelamiento 3.0 C-11: un campo de valor con lifetime no trivial (un struct con
    //     destructor o campos owned) se destruye en su lugar; ownedField lleva su miembro y offset.
    bool isValueFieldDrop = false;
    std::string valueFieldTypeKey;
};
}
