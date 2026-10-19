#pragma once

#include "nativesharp/Frontend/ProjectGenericModel.h"
#include "../../Internal/ByteReader.h"
#include "../../Internal/MirCacheRecord.h"

namespace nativesharp::frontend::internal
{
std::string computeTemplateSourceFingerprint(
    const ProjectSyntaxUnit& unit,
    const ast::AstNode* node);
std::string templateCacheKey(std::string_view sourceFingerprint);
std::string serializeAstNodeTree(const ast::AstNode* node);
ast::AstNode* deserializeAstNodeTree(ast::AstArena& arena, ByteReader& reader);
std::string serializeCatalogEntry(const GenericTemplateRecord& record);
bool restoreCatalogEntry(
    ast::AstArena& arena,
    std::string_view bytes,
    GenericTemplateRecord& record);
std::string serializeMirCacheRecord(const MirCacheRecord& record);
bool restoreMirCacheRecord(std::string_view bytes, MirCacheRecord& record);
}
