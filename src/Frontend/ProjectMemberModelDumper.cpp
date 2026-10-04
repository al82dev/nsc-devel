#include "nativesharp/Frontend/ProjectMemberModelDumper.h"

#include "nativesharp/Frontend/ProjectMemberModel.h"
#include "nativesharp/Frontend/ProjectNameLookup.h"

#include <ostream>

namespace nativesharp::frontend
{
void ProjectMemberModelDumper::dump(
    const ProjectMemberModel& model,
    const ProjectNameLookup& nameLookup,
    std::ostream& output) const
{
    output << "ProjectMemberModel\n";
    for (std::size_t typeOrdinal = 0; typeOrdinal < nameLookup.namedTypes().size(); ++typeOrdinal)
    {
        const NamedTypeDeclaration& type = nameLookup.namedTypes()[typeOrdinal];
        if (type.kind != NamedTypeDeclarationKind::Class &&
            type.kind != NamedTypeDeclarationKind::Struct)
        {
            continue;
        }

        output << "type[" << typeOrdinal << "] " << type.identity.key()
               << " kind=" << (type.kind == NamedTypeDeclarationKind::Struct ? "struct" : "class")
               << " members=" << model.membersOfType(typeOrdinal).size() << '\n';
        for (const std::size_t memberOrdinal : model.membersOfType(typeOrdinal))
        {
            const ClassMemberSymbol& member = model.members()[memberOrdinal];
            output << "  member[" << memberOrdinal << "] "
                   << classMemberAccessibilityName(member.accessibility) << ' '
                   << classMemberKindName(member.kind) << ' ' << member.name
                   << " type=" << member.declaredTypeKey
                   << " ownership=" << sema::ownershipKindName(member.declaredOwnership)
                   << " part=" << member.partOrdinal
                   << " source-ordinal=" << member.sourceOrdinal
                   << " declaration-ordinal=" << member.declarationOrdinal
                   << " static=" << (member.isStatic ? "true" : "false")
                   << " readonly=" << (member.isReadonly ? "true" : "false")
                   << " ref-return=" << (member.isReferenceReturn ? "true" : "false")
                   << " return-lifetime=" <<
                        (member.returnLifetimeName.empty() ? "none" : member.returnLifetimeName)
                   << " volatile=" << (member.isVolatile ? "true" : "false")
                   << " const=" << (member.isConst ? "true" : "false");
            if (member.declaredTypeKey.ends_with("*"))
            {
                output << " pointer-nullable=" <<
                              (member.declaredPointerNullable ? "true" : "false")
                       << " pointer-mutable=" <<
                              (member.declaredPointerPointeeReadonly ? "false" : "true")
                       << " pointer-provenance=" <<
                              (member.declaredPointerHasProvenance ? "tracked" : "none")
                       << " pointer-align=" << member.declaredPointerKnownAlignment
                       << " pointer-addrspace=" << member.declaredPointerAddressSpace;
            }
            if (member.kind == ClassMemberKind::Method || member.kind == ClassMemberKind::Constructor ||
                member.kind == ClassMemberKind::Destructor)
            {
                output << " signature=" << member.signatureKey
                       << " implicit-this=" << (member.hasImplicitThis ? type.identity.key() : "none")
                       << " virtual=" << (member.isVirtual ? "true" : "false")
                       << " variadic=" << (member.isVariadic ? "true" : "false")
                       << " inlining=" << inliningPolicyName(member.inliningPolicy)
                       << " noheap=" << (member.isNoHeap ? "true" : "false")
                       << " throws=" << (member.declaredErrorTypeKey.empty()
                            ? "<none>" : member.declaredErrorTypeKey)
                       << " effects=" << member.effects.bits()
                       << " fp=";
                switch (member.syntax != nullptr
                        ? member.syntax->floatingPointMode()
                        : ast::FloatingPointModeSyntaxKind::Inherit)
                {
                    case ast::FloatingPointModeSyntaxKind::Inherit: output << "inherit"; break;
                    case ast::FloatingPointModeSyntaxKind::Strict: output << "strict"; break;
                    case ast::FloatingPointModeSyntaxKind::Precise: output << "precise"; break;
                    case ast::FloatingPointModeSyntaxKind::Fast: output << "fast"; break;
                }
                output
                       << " override=" << (member.isOverride ? "true" : "false")
                       << " abstract=" << (member.isAbstract ? "true" : "false")
                       << " sealed=" << (member.isSealed ? "true" : "false");
                if (!member.lifetimeParameters.empty())
                {
                    output << " lifetimes=";
                    for (std::size_t index = 0; index < member.lifetimeParameters.size(); ++index)
                    {
                        if (index != 0) output << ',';
                        output << member.lifetimeParameters[index];
                    }
                }
                for (const auto& [longer, shorter] : member.lifetimeOutlivesConstraints)
                    output << " outlives=" << longer << ':' << shorter;
            }
            if (member.isBitField)
            {
                output << " bit-field=true"
                       << " unnamed=" << (member.isUnnamedBitField ? "true" : "false")
                       << " width-syntax=" << (member.bitWidthSyntax != nullptr ? "present" : "missing");
            }
            output << " source=" << member.sourcePath << '\n';

            for (const MethodParameterSymbol& parameter : member.parameters)
            {
                output << "    parameter " << parameter.name
                       << " type=" << parameter.typeKey
                       << " ownership=" << sema::ownershipKindName(parameter.ownership)
                       << " pass=" << parameterPassKindName(parameter.passKind)
                       << " lifetime=" <<
                            (parameter.lifetimeName.empty() ? "none" : parameter.lifetimeName);
                if (parameter.typeKey.ends_with("*"))
                {
                    output << " pointer-nullable=" <<
                                  (parameter.pointerNullable ? "true" : "false")
                           << " pointer-mutable=" <<
                                  (parameter.pointerPointeeReadonly ? "false" : "true")
                           << " pointer-provenance=" <<
                                  (parameter.pointerHasProvenance ? "tracked" : "none")
                           << " pointer-align=" << parameter.pointerKnownAlignment
                           << " pointer-addrspace=" << parameter.pointerAddressSpace;
                }
                output << '\n';
            }
        }
        for (const std::size_t memberOrdinal : model.promotedMembersOfType(typeOrdinal))
        {
            if (memberOrdinal >= model.members().size()) continue;
            const ClassMemberSymbol& member = model.members()[memberOrdinal];
            output << "  promoted-member[" << memberOrdinal << "] "
                   << classMemberAccessibilityName(member.accessibility) << " field "
                   << member.name << " type=" << member.declaredTypeKey << " via=";
            for (std::size_t index = 0;
                 index < member.anonymousCarrierMemberOrdinals.size();
                 ++index)
            {
                if (index != 0) output << ',';
                output << member.anonymousCarrierMemberOrdinals[index];
            }
            output << " source-member="
                   << member.promotedSourceMemberOrdinal.value_or(model.members().size())
                   << '\n';
        }
    }

    output << "body-bindings=" << model.bodyBindings().size() << '\n';
    for (std::size_t bindingOrdinal = 0;
         bindingOrdinal < model.bodyBindings().size();
         ++bindingOrdinal)
    {
        const MemberBodyBinding& binding = model.bodyBindings()[bindingOrdinal];
        output << "  binding[" << bindingOrdinal << "] "
               << memberBodyBindingKindName(binding.kind)
               << " type=" << binding.declaringTypeOrdinal
               << " callable-member=" << binding.methodMemberOrdinal;
        if (!binding.memberName.empty())
        {
            output << " name=" << binding.memberName << " candidates=";
            for (std::size_t index = 0; index < binding.candidateMemberOrdinals.size(); ++index)
            {
                if (index != 0)
                {
                    output << ',';
                }
                output << binding.candidateMemberOrdinals[index];
            }
        }
        output << '\n';
    }
}
}
