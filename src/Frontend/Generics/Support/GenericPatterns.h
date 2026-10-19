#pragma once

#include "nativesharp/Frontend/ProjectGenericModel.h"

namespace nativesharp::frontend::internal
{
std::string methodSpecializationFamilyKey(
    const GenericTemplateRecord& record,
    const ast::AstNode* syntax);
bool genericArityAccepts(const GenericTemplateRecord& record, std::size_t arity) noexcept;
std::string genericAritySpelling(const GenericTemplateRecord& record);
}
