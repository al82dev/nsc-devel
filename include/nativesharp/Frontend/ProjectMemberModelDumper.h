#pragma once

#include <cstddef>
#include <iosfwd>

namespace nativesharp::frontend
{
struct ClassMemberSymbol;
struct MethodParameterSymbol;
struct NamedTypeDeclaration;
class ProjectMemberModel;
class ProjectNameLookup;

class ProjectMemberModelDumper final
{
public:
    void dump(
        const ProjectMemberModel& model,
        const ProjectNameLookup& nameLookup,
        std::ostream& output) const;

private:
    static void dumpPointerType(
        const ClassMemberSymbol& member,
        std::ostream& output);
    static void dumpCallableMember(
        const ClassMemberSymbol& member,
        const NamedTypeDeclaration& type,
        std::ostream& output);
    static void dumpMethodParameter(
        const MethodParameterSymbol& parameter,
        std::ostream& output);
    static void dumpMember(
        const ProjectMemberModel& model,
        const NamedTypeDeclaration& type,
        std::size_t memberOrdinal,
        std::ostream& output);
    static void dumpPromotedMember(
        const ProjectMemberModel& model,
        std::size_t memberOrdinal,
        std::ostream& output);
    static void dumpType(
        const ProjectMemberModel& model,
        const ProjectNameLookup& nameLookup,
        std::size_t typeOrdinal,
        std::ostream& output);
    static void dumpBodyBindings(
        const ProjectMemberModel& model,
        std::ostream& output);
};
}
