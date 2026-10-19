#pragma once

#include "ProjectNameLookupStage.h"

namespace nativesharp::frontend
{
struct ProjectNameLookup::Frame final
{
    const ast::AstNode* node = nullptr;
    const LookupEnvironment* environment = nullptr;
    Stage stage = Stage::Enter;
};
}
