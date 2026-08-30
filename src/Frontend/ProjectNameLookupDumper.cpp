#include "nativesharp/Frontend/ProjectNameLookupDumper.h"

#include "nativesharp/Frontend/NamespaceSymbol.h"
#include "nativesharp/Frontend/ProjectNameLookup.h"

#include <ostream>

namespace nativesharp::frontend
{
namespace
{
void indent(std::ostream& output, int depth)
{
    for (int index = 0; index < depth; ++index)
    {
        output << "  ";
    }
}
}

void ProjectNameLookupDumper::dump(const ProjectNameLookup& lookup, std::ostream& output) const
{
    output
        << "ProjectNameLookup\n"
        << "modules: " << lookup.modules().size() << '\n'
        << "named-types: " << lookup.namedTypes().size() << '\n'
        << "type-bindings: " << lookup.bindings().size() << '\n';

    for (const std::unique_ptr<ModuleDeclarationSpace>& module : lookup.modules())
    {
        output
            << "module[" << module->projectOrdinal() << "] " << module->moduleName()
            << " visible-dependencies=" << module->visibleProjectOrdinals().size() << '\n';
        dumpNamespace(lookup, module->globalNamespace().root(), output, 1);
    }

    output << "bindings:\n";
    for (const TypeNameBinding& binding : lookup.bindings())
    {
        output
            << "  file=" << binding.fileId.value()
            << " module=" << lookup.modules()[binding.projectOrdinal]->moduleName()
            << " namespace=" << (binding.namespaceName.empty() ? "<global>" : binding.namespaceName)
            << " name=" << binding.writtenName
            << " status=" << typeNameBindingKindName(binding.kind);
        if (binding.targetTypeOrdinal.has_value())
        {
            output << " -> " << lookup.namedTypes()[*binding.targetTypeOrdinal].identity.key();
        }
        if (binding.kind == TypeNameBindingKind::Ambiguous)
        {
            output << " candidates=" << binding.candidateTypeOrdinals.size();
        }
        output << '\n';
    }
}

void ProjectNameLookupDumper::dumpNamespace(
    const ProjectNameLookup& lookup,
    const NamespaceSymbol& namespaceSymbol,
    std::ostream& output,
    int depth)
{
    if (namespaceSymbol.parent() != nullptr)
    {
        indent(output, depth);
        output << "namespace " << namespaceSymbol.fullName() << '\n';
        ++depth;
    }

    for (const std::size_t ordinal : namespaceSymbol.typeOrdinals())
    {
        const NamedTypeDeclaration& type = lookup.namedTypes()[ordinal];
        indent(output, depth);
        output
            << namedTypeAccessibilityName(type.accessibility) << ' '
            << namedTypeDeclarationKindName(type.kind) << ' '
            << type.identity.fullyQualifiedName()
            << " key=" << type.identity.key()
            << " file=" << type.fileId.value()
            << " partial=" << (type.isPartial ? "true" : "false")
            << " abstract=" << (type.isAbstract ? "true" : "false")
            << " sealed=" << (type.isSealed ? "true" : "false")
            << " parts=" << type.parts.size() << '\n';

        for (std::size_t partIndex = 0; partIndex < type.parts.size(); ++partIndex)
        {
            const NamedTypeDeclarationPart& part = type.parts[partIndex];
            indent(output, depth + 1);
            output
                << "part[" << partIndex << "]"
                << " file=" << part.fileId.value()
                << " source-ordinal=" << part.sourceOrdinal
                << " accessibility="
                << (part.hasExplicitAccessibility
                    ? namedTypeAccessibilityName(part.accessibility)
                    : "unspecified")
                << " partial=" << (part.isPartial ? "true" : "false")
                << " abstract=" << (part.isAbstract ? "true" : "false")
                << " sealed=" << (part.isSealed ? "true" : "false")
                << " path=" << part.sourcePath << '\n';
        }
    }

    for (const std::unique_ptr<NamespaceSymbol>& child : namespaceSymbol.children())
    {
        dumpNamespace(lookup, *child, output, depth);
    }
}
}
