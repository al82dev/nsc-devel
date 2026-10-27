#include "NameSnapshotCapture.h"

#include "PrecompiledSnapshotWriter.h"

#include "nativesharp/AST/AstNode.h"
#include "nativesharp/AST/AstNodeKind.h"
#include "nativesharp/Frontend/ProjectCompilation.h"

#include <optional>
#include <span>
#include <vector>

namespace nativesharp::frontend
{
namespace
{
// EN: Writes one type ordinal as its identity key; false for an ordinal outside the model.
// ES: Escribe un ordinal de tipo como su llave de identidad; false si está fuera del modelo.
bool writeTypeOrdinal(PrecompiledSnapshotWriter& writer, const ProjectNameLookup& names, std::size_t ordinal)
{
    if (ordinal >= names.namedTypes().size()) return false;
    writer.typeKey(names.namedTypes()[ordinal].identity.key());
    return true;
}

// EN: Writes a list of type ordinals as identity keys; false for an ordinal outside the model.
// ES: Escribe una lista de ordinales de tipo como llaves de identidad; false si un ordinal está
//     fuera del modelo.
bool writeTypeOrdinals(
    PrecompiledSnapshotWriter& writer,
    const ProjectNameLookup& names,
    std::span<const std::size_t> ordinals)
{
    writer.u32(static_cast<std::uint32_t>(ordinals.size()));
    for (const std::size_t ordinal : ordinals)
    {
        if (!writeTypeOrdinal(writer, names, ordinal)) return false;
    }
    return true;
}

// EN: The entries of `all` whose syntax belongs to the captured unit, as one contiguous block.
// ES: Las entradas de `all` cuya sintaxis pertenece a la unidad capturada, como un bloque contiguo.
template <typename Binding, typename SyntaxOf>
bool unitBlock(std::span<const Binding> all, SyntaxOf syntaxOf, std::vector<const Binding*>& block)
{
    std::optional<std::size_t> previous;
    for (std::size_t position = 0; position < all.size(); ++position)
    {
        if (!syntaxOf(all[position])) continue;
        if (previous.has_value() && *previous + 1U != position) return false;
        previous = position;
        block.push_back(&all[position]);
    }
    return true;
}
}

bool captureNameSnapshot(
    const ProjectCompilation& reference,
    const PrecompiledSnapshotUnit& capture,
    std::string& payload,
    std::string& error)
{
    const ProjectNameLookup* const names = reference.nameLookup();
    if (names == nullptr || !reference.diagnostics().diagnostics().empty())
    {
        error = "reference has no clean name lookup";
        return false;
    }
    for (const ast::AstNode* node : capture.index.nodes())
    {
        if (node->kind() == ast::AstNodeKind::UsingAliasDirective)
        {
            error = "the API unit declares type aliases";
            return false;
        }
    }
    const ProjectSyntaxUnit& unit = *reference.units()[capture.unitOrdinal];
    std::vector<const TypeNameBinding*> bindings;
    std::vector<const ExpressionTypeNameBinding*> expressions;
    if (!unitBlock(names->bindings(), [&](const TypeNameBinding& binding)
            { return capture.index.indexOf(binding.syntax).has_value(); }, bindings) ||
        !unitBlock(names->expressionTypeBindings(), [&](const ExpressionTypeNameBinding& binding)
            { return capture.index.indexOf(binding.expression).has_value(); }, expressions))
    {
        error = "the API unit bindings are not contiguous";
        return false;
    }
    PrecompiledSnapshotWriter writer(capture.index);
    bool written = true;
    writer.u64(bindings.size());
    for (const TypeNameBinding* binding : bindings)
    {
        written = written && binding->projectOrdinal == unit.projectOrdinal() && binding->fileId == unit.fileId();
        writer.type(binding->syntax);
        writer.byte(static_cast<std::uint8_t>(binding->kind));
        writer.text(binding->namespaceName);
        writer.text(binding->writtenName);
        writer.byte(binding->targetTypeOrdinal.has_value() ? 1U : 0U);
        if (binding->targetTypeOrdinal.has_value())
            written = written && writeTypeOrdinal(writer, *names, *binding->targetTypeOrdinal);
        written = written && writeTypeOrdinals(writer, *names, binding->candidateTypeOrdinals) &&
            writeTypeOrdinals(writer, *names, binding->enclosingTypeOrdinals);
    }
    writer.u64(expressions.size());
    for (const ExpressionTypeNameBinding* binding : expressions)
    {
        writer.node(binding->expression);
        writer.text(binding->writtenName);
        written = written && writeTypeOrdinals(writer, *names, binding->candidateTypeOrdinals) &&
            writeTypeOrdinals(writer, *names, binding->enclosingTypeOrdinals);
    }
    if (!written || writer.failed())
    {
        error = "a binding of the API unit cannot be written by identity";
        return false;
    }
    payload = writer.take();
    return true;
}

bool captureNameDeclarationSnapshot(
    const ProjectCompilation& reference,
    const PrecompiledSnapshotUnit& capture,
    std::string& payload,
    std::string& error)
{
    const ProjectNameLookup* const names = reference.nameLookup();
    if (names == nullptr || !reference.diagnostics().diagnostics().empty())
    {
        error = "reference has no clean name lookup";
        return false;
    }
    return names->writePrecompiledSnapshotDeclarations(
        capture.index, *reference.units()[capture.unitOrdinal], payload, error);
}
}
