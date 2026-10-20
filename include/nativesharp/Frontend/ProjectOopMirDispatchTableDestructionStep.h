#pragma once

#include "nativesharp/Frontend/ProjectOopMirDispatchTable.h"

namespace nativesharp::frontend
{
struct ProjectOopMirDispatchTable::DestructionStep final
{
    std::string destructorCalleeName;
    OwnedField ownedField;
    bool isFieldRelease = false;
};
}
