#pragma once

#include "nativesharp/Frontend/ProjectOopMirDispatchTable.h"

namespace nativesharp::frontend
{
struct ProjectOopMirDispatchTable::DestructionStep final
{
    std::string destructorCalleeName;
    OwnedField ownedField;
    bool isFieldRelease = false;
    // EN: Freeze C-11: a value field with a non-trivial lifetime (a struct with a destructor
    //     or owned fields) is dropped in place; ownedField carries its member and offset.
    // ES: Congelamiento C-11: un campo de valor con lifetime no trivial (un struct con
    //     destructor o campos owned) se destruye en su lugar; ownedField lleva su miembro y offset.
    bool isValueFieldDrop = false;
    std::string valueFieldTypeKey;
    // X3-01: the C++ adapter's destroy function (`void f(void* object)`) of a class derived from a C++ class with a
    //     non-trivial destructor. / La funcion de destruccion del adaptador C++.
    std::string cppInterfaceDestroySymbol;
    // X2-06: the destructor of a non-trivial C++ record field, called with the field's address (ownedField.offset).
    //     / El destructor de un campo record C++ no trivial, llamado con la direccion del campo.
    std::string cppFieldDestructorCalleeName;
};
}
