#pragma once

#include "../../Cache/CachePayloadReader.h"
#include "../../Internal/MirCacheRecord.h"
#include "nativesharp/AST/AstArena.h"
#include "nativesharp/AST/AstNode.h"
#include "nativesharp/Basic/FileId.h"
#include "nativesharp/Frontend/GenericTemplateRecord.h"
#include "nativesharp/Frontend/ProjectSyntaxUnit.h"

#include <string>
#include <string_view>

namespace nativesharp::frontend::internal
{
std::string computeTemplateSourceFingerprint(
    const ProjectSyntaxUnit& unit,
    const ast::AstNode* node);
std::string templateCacheKey(std::string_view sourceFingerprint);
bool deserializeTemplateRecord(std::string_view bytes, GenericTemplateRecord& record);
std::string serializeAstNodeTree(const ast::AstNode* node);
ast::AstNode* deserializeAstNodeTree(ast::AstArena& arena, CachePayloadReader& reader);
// Preserves offsets and invalid sentinels while rebinding valid ranges to a consumer FileId.
ast::AstNode* deserializeAstNodeTree(
    ast::AstArena& arena,
    CachePayloadReader& reader,
    FileId relocatedFileId);
std::string serializeCatalogEntry(const GenericTemplateRecord& record);
bool restoreCatalogEntry(
    ast::AstArena& arena,
    std::string_view bytes,
    GenericTemplateRecord& record);
std::string serializeMirCacheRecord(const MirCacheRecord& record);
bool restoreMirCacheRecord(std::string_view bytes, MirCacheRecord& record);
}
