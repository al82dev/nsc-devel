#include "nativesharp/Frontend/ProjectVirtualDispatchModelDumper.h"

#include "nativesharp/Frontend/ProjectMemberModel.h"
#include "nativesharp/Frontend/ProjectNameLookup.h"
#include "nativesharp/Frontend/ProjectVirtualDispatchModel.h"

#include <ostream>

namespace nativesharp::frontend
{
void ProjectVirtualDispatchModelDumper::dump(
    const ProjectVirtualDispatchModel& model,
    const ProjectMemberModel& members,
    const ProjectNameLookup& names,
    std::ostream& output) const
{
    output << "project-virtual-dispatch\n";
    for (const ClassVirtualTable& table : model.classTables())
    {
        if (table.typeOrdinal >= names.namedTypes().size())
        {
            continue;
        }
        output << "  class=" << names.namedTypes()[table.typeOrdinal].identity.key()
               << " abstract=" << (table.isAbstract ? "true" : "false")
               << " sealed=" << (table.isSealed ? "true" : "false")
               << " slots=" << table.slotMemberOrdinals.size()
               << " base=";
        if (table.baseTypeOrdinal.has_value() && *table.baseTypeOrdinal < names.namedTypes().size())
        {
            output << names.namedTypes()[*table.baseTypeOrdinal].identity.key();
        }
        else
        {
            output << "<none>";
        }
        output << '\n';
        for (std::size_t slot = 0; slot < table.slotMemberOrdinals.size(); ++slot)
        {
            const std::size_t memberOrdinal = table.slotMemberOrdinals[slot];
            output << "    slot[" << slot << "]=";
            if (memberOrdinal < members.members().size())
            {
                output << members.members()[memberOrdinal].signatureKey
                       << " member=" << memberOrdinal
                       << " abstract=" << (slot < table.slotAbstract.size() && table.slotAbstract[slot] ? "true" : "false");
            }
            else
            {
                output << "<invalid>";
            }
            output << '\n';
        }
    }

    output << "  virtual-methods=" << model.virtualMethods().size() << '\n';
    for (const VirtualMethodInfo& info : model.virtualMethods())
    {
        output << "    member=" << info.memberOrdinal
               << " slot=" << info.slotOrdinal
               << " root=" << info.rootVirtualMemberOrdinal;
        if (info.overriddenMemberOrdinal.has_value())
        {
            output << " overrides=" << *info.overriddenMemberOrdinal;
        }
        output << '\n';
    }
}
}
