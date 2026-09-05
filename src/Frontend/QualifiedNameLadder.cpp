#include "nativesharp/Frontend/QualifiedNameLadder.h"

#include <string>

namespace nativesharp::frontend
{
namespace
{
std::string joinPrefix(std::span<const std::string> segments, std::size_t count, std::string_view tail)
{
    std::string result;
    for (std::size_t index = 0; index < count && index < segments.size(); ++index)
    {
        if (!result.empty()) result.push_back('.');
        result += segments[index];
    }
    if (!result.empty()) result.push_back('.');
    result.append(tail);
    return result;
}

std::string_view firstSegment(std::string_view name)
{
    const std::size_t dot = name.find('.');
    return dot == std::string_view::npos ? name : name.substr(0, dot);
}

// LOS ESCALONES DE UN NOMBRE CON PUNTOS. Un nombre calificado no vuelve a pasar por alias ni tiene
// forma «simple»: lo unico que se le antepone son prefijos de namespace, de dentro afuera, hasta
// el vacio -- que es el propio nombre tal cual, o sea el global.
void appendQualifiedRungs(
    const QualifiedNameSite& site, std::string_view name, std::vector<QualifiedNameRung>& rungs)
{
    // De DENTRO afuera, y el ultimo paso es prefijo vacio: `N1.C` escrito en `A.B` se prueba como
    // `A.B.N1.C`, `A.N1.C` y `N1.C`.
    for (std::size_t prefixCount = site.namespaceSegments.size() + 1; prefixCount > 0; --prefixCount)
    {
        rungs.push_back(QualifiedNameRung{
            QualifiedNameTier::LexicalNamespace,
            {joinPrefix(site.namespaceSegments, prefixCount - 1, name)}});
    }

    if (!site.importedNamespaces.empty())
    {
        QualifiedNameRung imported{QualifiedNameTier::ImportedNamespace, {}};
        for (const std::string& importedNamespace : site.importedNamespaces)
        {
            std::string candidate = importedNamespace;
            if (!candidate.empty()) candidate.push_back('.');
            candidate.append(name);
            imported.spellings.push_back(std::move(candidate));
        }
        rungs.push_back(std::move(imported));
    }

    for (std::size_t prefixCount = site.namespaceSegments.size() + 1; prefixCount > 0; --prefixCount)
    {
        rungs.push_back(QualifiedNameRung{
            QualifiedNameTier::ReferencedModule,
            {joinPrefix(site.namespaceSegments, prefixCount - 1, name)}});
    }
}
}

std::vector<QualifiedNameRung> qualifiedNameLadder(
    const QualifiedNameSite& site, std::string_view writtenName)
{
    std::vector<QualifiedNameRung> rungs;
    if (writtenName.empty()) return rungs;

    // EL PRIMER ESCALON SON LOS CONTENEDORES, y va antes de los alias a proposito: dentro de
    // `Outer`, `Item` significa `Outer.Item`. Es el mismo sombreado que un namespace interior hace
    // sobre uno exterior, aplicado a la cadena de tipos. De DENTRO afuera: `Item` escrito en
    // `Outer.Medio` es `Outer.Medio.Item` si existe, y solo si no, `Outer.Item`.
    //
    // Vale igual para un nombre CON puntos: `Helper.Nested` dentro de `Outer` se prueba primero
    // como `Outer.Helper.Nested`.
    for (std::size_t depth = site.enclosingTypeNames.size(); depth > 0; --depth)
    {
        std::string candidate = site.enclosingTypeNames[depth - 1];
        candidate.push_back('.');
        candidate.append(writtenName);
        rungs.push_back(QualifiedNameRung{QualifiedNameTier::EnclosingType, {std::move(candidate)}});
    }

    // UN ALIAS SUSTITUYE Y SE ACABA. `using X = A.B;` mas `X.C` es `A.B.C` y nada mas: no se
    // vuelve a buscar `X.C` por su cuenta, porque el alias no es una pista, es una definicion.
    if (site.aliases != nullptr)
    {
        const std::string_view head = firstSegment(writtenName);
        const auto alias = site.aliases->find(std::string(head));
        if (alias != site.aliases->end())
        {
            std::string substituted = alias->second;
            substituted.append(writtenName.substr(head.size()));
            appendQualifiedRungs(site, substituted, rungs);
            return rungs;
        }
    }

    if (writtenName.find('.') != std::string_view::npos)
    {
        appendQualifiedRungs(site, writtenName, rungs);
        return rungs;
    }

    // NOMBRE SIMPLE. La cadena lexica NO llega hasta el prefijo vacio aqui: el global es su propio
    // escalon y va DESPUES de los namespaces importados, no antes. Es la diferencia con la rama
    // calificada de arriba, y no es un descuido de ninguna de las dos: un `using` puede traer un
    // nombre simple que sombree al global, y no puede traer uno que sombree al namespace propio.
    for (std::size_t depth = site.namespaceSegments.size(); depth > 0; --depth)
    {
        rungs.push_back(QualifiedNameRung{
            QualifiedNameTier::LexicalNamespace,
            {joinPrefix(site.namespaceSegments, depth, writtenName)}});
    }

    if (!site.importedNamespaces.empty())
    {
        QualifiedNameRung imported{QualifiedNameTier::ImportedNamespace, {}};
        for (const std::string& importedNamespace : site.importedNamespaces)
        {
            std::string candidate = importedNamespace;
            if (!candidate.empty()) candidate.push_back('.');
            candidate.append(writtenName);
            imported.spellings.push_back(std::move(candidate));
        }
        rungs.push_back(std::move(imported));
    }

    rungs.push_back(QualifiedNameRung{QualifiedNameTier::Global, {std::string(writtenName)}});

    for (std::size_t depth = site.namespaceSegments.size(); depth > 0; --depth)
    {
        rungs.push_back(QualifiedNameRung{
            QualifiedNameTier::ReferencedModule,
            {joinPrefix(site.namespaceSegments, depth, writtenName)}});
    }
    rungs.push_back(
        QualifiedNameRung{QualifiedNameTier::ReferencedModule, {std::string(writtenName)}});

    return rungs;
}
}
