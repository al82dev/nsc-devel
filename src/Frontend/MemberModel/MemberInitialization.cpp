#include "nativesharp/Frontend/ProjectMemberModel.h"
#include "../Internal/EnumInitializerResolver.h"
#include "../Internal/AnonymousMemberPromotionVisitState.h"
#include "../Internal/EnumValue.h"
#include "../Internal/EnumSuccessor.h"

#include "nativesharp/Language/AggregateSemantics.h"
#include "nativesharp/Language/CallableSemantics.h"
#include "nativesharp/Language/AbiSemantics.h"
#include "nativesharp/Language/TraitSemantics.h"
#include "nativesharp/Language/GenericSemantics.h"
#include "nativesharp/Language/CoroutineSemantics.h"
#include "nativesharp/Frontend/ProjectGenericModel.h"
#include "nativesharp/Compilation/ProjectGraph.h"

#include "nativesharp/Frontend/EnumUnderlyingType.h"
#include "nativesharp/Frontend/ProjectIntegerEvaluator.h"
#include "nativesharp/Frontend/PrecompiledMemberIdentityIndex.h"
#include "nativesharp/Frontend/ProjectTypeConversion.h"

#include "nativesharp/Diagnostics/DiagnosticEngine.h"
#include "nativesharp/Diagnostics/DiagnosticId.h"
#include "nativesharp/Diagnostics/DiagnosticSeverity.h"
#include "nativesharp/Lexer/TokenKind.h"

#include <algorithm>
#include <cctype>
#include <deque>
#include <filesystem>
#include <functional>
#include <limits>
#include <sstream>
#include <string>
#include <vector>
#include <unordered_map>
#include <unordered_set>
#include <utility>

namespace nativesharp::frontend
{

void ProjectMemberModel::validateExportedGenericMember(
    const ast::AstNode* directMember,
    diagnostics::DiagnosticEngine& diagnostics) const
{
    if (directMember == nullptr ||
        directMember->kind() != ast::AstNodeKind::MethodDeclaration ||
        directMember->genericParameters().empty() ||
        (!directMember->hasModifier(lexer::TokenKind::KwExport) &&
         !directMember->hasModifier(lexer::TokenKind::KwExtern)))
        return;
    diagnostics.report(
        diagnostics::DiagnosticId::InvalidType,
        diagnostics::DiagnosticSeverity::Error,
        directMember->range(),
        "Stage 47 export extern C does not expose generic methods or members of generic types");
}

void ProjectMemberModel::registerTypeMemberDeclaration(
    const ast::AstNode* declaredMember,
    const NamedTypeDeclaration& type,
    std::unordered_map<std::string, const ast::AstNode*>& propertyDeclarations,
    std::unordered_map<std::string, const ast::AstNode*>& ordinaryDeclarations,
    diagnostics::DiagnosticEngine& diagnostics) const
{
    if (declaredMember == nullptr) return;
    if (declaredMember->kind() == ast::AstNodeKind::PropertyDeclaration)
    {
        const auto ordinary = ordinaryDeclarations.find(declaredMember->text());
        const auto previousProperty = propertyDeclarations.find(declaredMember->text());
        // NS304-T12-F03: two partial property declarations merge; the accessors merge in the
        // member model (F02). Keep the first declaration as the canonical property.
        const bool partialPropertyMerge =
            previousProperty != propertyDeclarations.end() &&
            previousProperty->second != nullptr &&
            previousProperty->second->auxiliaryText().find("partial-property") !=
                std::string::npos &&
            declaredMember->auxiliaryText().find("partial-property") != std::string::npos;
        if (partialPropertyMerge) return;
        if (ordinary == ordinaryDeclarations.end() &&
            previousProperty == propertyDeclarations.end())
        {
            propertyDeclarations.emplace(declaredMember->text(), declaredMember);
            return;
        }
        const ast::AstNode* previous = ordinary != ordinaryDeclarations.end()
            ? ordinary->second : previousProperty->second;
        diagnostics.report(
            diagnostics::DiagnosticId::DuplicateSymbol,
            diagnostics::DiagnosticSeverity::Error,
            declaredMember->range(),
            "property name '" + declaredMember->text() +
                "' conflicts with an existing member in type '" +
                type.identity.fullyQualifiedName() + "'");
        if (previous != nullptr)
            diagnostics.report(
                diagnostics::DiagnosticId::PreviousDeclaration,
                diagnostics::DiagnosticSeverity::Note,
                previous->range(),
                "previous declaration is here");
        return;
    }
    if (declaredMember->kind() != ast::AstNodeKind::FieldDeclaration &&
        declaredMember->kind() != ast::AstNodeKind::EventDeclaration &&
        declaredMember->kind() != ast::AstNodeKind::MethodDeclaration)
        return;
    if (propertyDeclarations.contains(declaredMember->text()))
    {
        diagnostics.report(
            diagnostics::DiagnosticId::DuplicateSymbol,
            diagnostics::DiagnosticSeverity::Error,
            declaredMember->range(),
            "member name '" + declaredMember->text() +
                "' conflicts with an existing property in type '" +
                type.identity.fullyQualifiedName() + "'");
    }
    ordinaryDeclarations.emplace(declaredMember->text(), declaredMember);
}

ClassMemberKind ProjectMemberModel::classMemberKind(
    const ast::AstNode* memberNode) noexcept
{
    if (memberNode->kind() == ast::AstNodeKind::FieldDeclaration)
        return ClassMemberKind::Field;
    if (memberNode->kind() == ast::AstNodeKind::EventDeclaration)
        return ClassMemberKind::Event;
    if (memberNode->kind() == ast::AstNodeKind::ConstructorDeclaration)
        return ClassMemberKind::Constructor;
    if (memberNode->kind() == ast::AstNodeKind::DestructorDeclaration)
        return ClassMemberKind::Destructor;
    return ClassMemberKind::Method;
}

void ProjectMemberModel::applyMemberGenericSpecialization(
    ClassMemberSymbol& symbol,
    const ast::AstNode* memberNode) const
{
    if (genericModel_ == nullptr) return;
    const GenericSpecializationRecord* specialization =
        genericModel_->specializationForSyntax(memberNode);
    if (specialization == nullptr ||
        specialization->templateOrdinal >= genericModel_->templates().size())
        return;
    symbol.genericTemplateOrdinal = specialization->templateOrdinal;
    const GenericTemplateRecord& genericTemplate =
        genericModel_->templates()[specialization->templateOrdinal];
    symbol.isControlledSpecialization = genericTemplate.isControlledSpecialization;
    symbol.isControlledSpecializationFamily =
        genericTemplate.isControlledSpecializationFamily;
    symbol.genericTemplateKey = genericTemplate.canonicalKey;
}

void ProjectMemberModel::applyMemberOrigin(
    ClassMemberSymbol& symbol,
    const NamedTypeDeclarationPart& part,
    const ast::AstNode* memberNode) const
{
    // NSMETA 1.13 may place the producer's real `inline`/`forceinline` body in the
    // synthetic API unit. Those methods are executable input owned by this consumer,
    // unlike the empty declarations beside them. The source modifier is an explicit,
    // format-stable discriminator and also carries the required optimization policy.
    const bool transportedInlineBody =
        part.isPrecompiled && memberNode->kind() == ast::AstNodeKind::MethodDeclaration &&
        (memberNode->hasModifier(lexer::TokenKind::KwInline) ||
         memberNode->hasModifier(lexer::TokenKind::KwForceInline));
    symbol.isPrecompiled = part.isPrecompiled && !transportedInlineBody;
    symbol.isTransportedInlineBody = transportedInlineBody;
    symbol.name = memberNode->text();
}

bool ProjectMemberModel::initializeMemberDeclaredType(
    ClassMemberSymbol& symbol,
    const NamedTypeDeclaration& type,
    const ast::AstNode* memberNode,
    const ProjectNameLookup& nameLookup,
    diagnostics::DiagnosticEngine& diagnostics) const
{
    // NS304-T05-F03: `async` viaja como token `;async` en auxiliaryText
    // (Parser.cpp:4033-4035). Un metodo async devuelve un Future opaco lazy
    // (`future:<T>`) y lleva Effect::Async. El lowering productivo es F03.
    const bool isAsyncMethod = symbol.kind == ClassMemberKind::Method &&
        (memberNode->auxiliaryText() == "async" ||
         memberNode->auxiliaryText().starts_with("async;"));
    symbol.declaredTypeSyntax = memberNode->typeSyntax();
    symbol.declaredTypeKey = symbol.kind == ClassMemberKind::Constructor
        ? "type:" + type.identity.key()
        : symbol.kind == ClassMemberKind::Destructor
            ? "builtin:void"
            : canonicalTypeKey(memberNode->typeSyntax(), nameLookup);
    symbol.declaredErrorTypeSyntax = memberNode->throwsTypeSyntax();
    symbol.declaredErrorTypeKey = memberNode->throwsTypeSyntax() == nullptr
        ? std::string{}
        : canonicalTypeKey(memberNode->throwsTypeSyntax(), nameLookup);
    if (!isAsyncMethod) return false;
    // NS304-T05-F07: a throwing async future carries its error in the type key
    // (`future:<T>|<E>`), so the error is not silently discarded.
    symbol.declaredTypeKey = language::futureTypeKey(
        symbol.declaredTypeKey, symbol.declaredErrorTypeKey);
    // NS304-T05-F08: read [FrameStorage(Stack|Arena|Heap)] from the marker.
    const std::string_view metadata = memberNode->auxiliaryText();
    const std::size_t storagePos = metadata.find("framestorage:");
    if (storagePos == std::string_view::npos) return true;
    const std::size_t nameBegin = storagePos + std::string("framestorage:").size();
    const std::size_t nameEnd = metadata.find(';', nameBegin);
    const std::string_view storageName = metadata.substr(
        nameBegin,
        nameEnd == std::string_view::npos ? std::string_view::npos : nameEnd - nameBegin);
    if (storageName == "Stack") symbol.frameStorage = language::FrameStorage::CallerStack;
    else if (storageName == "Arena") symbol.frameStorage = language::FrameStorage::CallerArena;
    else if (storageName == "Heap") symbol.frameStorage = language::FrameStorage::ExplicitHeap;
    else
        diagnostics.report(
            diagnostics::DiagnosticId::InvalidOperand,
            diagnostics::DiagnosticSeverity::Error,
            symbol.range,
            "unknown FrameStorage '" + std::string(storageName) +
                "' (expected Stack, Arena or Heap)");
    return true;
}

void ProjectMemberModel::initializeMemberLifetimeContract(
    ClassMemberSymbol& symbol,
    const ast::AstNode* memberNode) const
{
    // NS304-T11-F07: `ref T` is a reference return on a method AND a reference field on a
    // field (a storage pointer). Both mark the member so escape analysis can reject a
    // containing struct that would outlive the storage.
    symbol.isReferenceReturn = memberNode->isReference();
    symbol.returnLifetimeName = memberNode->lifetimeName();
    for (const ast::LifetimeParameterSyntax& parameter : memberNode->lifetimeParameters())
        symbol.lifetimeParameters.push_back(parameter.name);
    for (const ast::LifetimeOutlivesConstraint& constraint :
         memberNode->lifetimeOutlivesConstraints())
        symbol.lifetimeOutlivesConstraints.emplace_back(
            constraint.longer, constraint.shorter);
}

void ProjectMemberModel::validateMemberReferenceContract(
    const ClassMemberSymbol& symbol,
    diagnostics::DiagnosticEngine& diagnostics) const
{
    if (symbol.isReferenceReturn && symbol.declaredTypeKey == "builtin:void")
    {
        diagnostics.report(
            diagnostics::DiagnosticId::InvalidType,
            diagnostics::DiagnosticSeverity::Error,
            symbol.range,
            "ref-returning method '" + symbol.name + "' cannot return void");
    }
    if (symbol.isReferenceReturn &&
        symbol.declaredOwnership != sema::OwnershipKind::None)
    {
        diagnostics.report(
            diagnostics::DiagnosticId::InvalidType,
            diagnostics::DiagnosticSeverity::Error,
            symbol.range,
            "ref-returning method '" + symbol.name +
                "' cannot combine ref with owned or borrowed return ownership");
    }
}

void ProjectMemberModel::initializeMemberEffects(
    ClassMemberSymbol& symbol,
    const ast::AstNode* memberNode,
    bool isAsyncMethod) const
{
    symbol.isConst = symbol.kind == ClassMemberKind::Field &&
        memberNode->hasModifier(lexer::TokenKind::KwConst);
    symbol.inliningPolicy = symbol.kind == ClassMemberKind::Method &&
            memberNode->hasModifier(lexer::TokenKind::KwNoInline)
        ? InliningPolicy::Forbidden
        : symbol.kind == ClassMemberKind::Method &&
                memberNode->hasModifier(lexer::TokenKind::KwForceInline)
            ? InliningPolicy::Required
            : symbol.kind == ClassMemberKind::Method &&
                    memberNode->hasModifier(lexer::TokenKind::KwInline)
                ? InliningPolicy::Preferred
                : InliningPolicy::Automatic;
    symbol.isNoHeap = (symbol.kind == ClassMemberKind::Method ||
                       symbol.kind == ClassMemberKind::Constructor) &&
        memberNode->hasModifier(lexer::TokenKind::KwNoHeap);
    symbol.isUnsafe = (symbol.kind == ClassMemberKind::Method ||
                       symbol.kind == ClassMemberKind::Constructor) &&
        memberNode->isUnsafe();
    symbol.effects = {};
    if (symbol.isNoHeap) symbol.effects = symbol.effects.with(sema::Effect::NoHeap);
    if (symbol.isUnsafe) symbol.effects = symbol.effects.with(sema::Effect::Unsafe);
    if (!symbol.declaredErrorTypeKey.empty())
        symbol.effects = symbol.effects.with(sema::Effect::Throws);
    if (isAsyncMethod) symbol.effects = symbol.effects.with(sema::Effect::Async);
    // NS304-T06-F02: a generator is marked by its declared iterator return type
    // (`Iterator<T>` -> `iterator:<T>`), not by a textual scan for `yield`.
    if (language::iteratorInnerTypeKey(symbol.declaredTypeKey).has_value())
        symbol.effects = symbol.effects.with(sema::Effect::Generator);
    // NS304-T07-F02: an async generator carries BOTH effects (await + yield in one machine).
    if (language::asyncIteratorInnerTypeKey(symbol.declaredTypeKey).has_value())
        symbol.effects = symbol.effects.with(sema::Effect::Generator)
            .with(sema::Effect::Async);
}

void ProjectMemberModel::initializeMemberStorageModifiers(
    ClassMemberSymbol& symbol,
    const NamedTypeDeclaration& type,
    const ast::AstNode* memberNode) const
{
    symbol.isStatic =
        (symbol.kind == ClassMemberKind::Field ||
         symbol.kind == ClassMemberKind::Event ||
         symbol.kind == ClassMemberKind::Method ||
         symbol.kind == ClassMemberKind::Constructor) &&
        (memberNode->hasModifier(lexer::TokenKind::KwStatic) || symbol.isConst);
    symbol.isNewSlot = memberNode->hasModifier(lexer::TokenKind::KwNew);
    const bool readonlyDeclaringStruct = type.kind == NamedTypeDeclarationKind::Struct &&
        type.syntax != nullptr &&
        type.syntax->hasModifier(lexer::TokenKind::KwReadonly);
    const bool readonlyPropertyBacking = readonlyDeclaringStruct &&
        memberNode->auxiliaryText().starts_with("property-backing:");
    symbol.isReadonly =
        (symbol.kind == ClassMemberKind::Field &&
         (memberNode->hasModifier(lexer::TokenKind::KwReadonly) ||
          readonlyPropertyBacking)) ||
        (symbol.kind == ClassMemberKind::Method && !symbol.isStatic &&
         (memberNode->hasModifier(lexer::TokenKind::KwReadonly) ||
          readonlyDeclaringStruct));
    symbol.isVolatile = symbol.kind == ClassMemberKind::Field &&
        memberNode->hasModifier(lexer::TokenKind::KwVolatile);
}

void ProjectMemberModel::validateReadonlyStructMember(
    const ClassMemberSymbol& symbol,
    const NamedTypeDeclaration& type,
    diagnostics::DiagnosticEngine& diagnostics) const
{
    if (type.syntax == nullptr ||
        !type.syntax->hasModifier(lexer::TokenKind::KwReadonly) ||
        symbol.kind != ClassMemberKind::Field || symbol.isStatic || symbol.isReadonly)
        return;
    diagnostics.report(
        diagnostics::DiagnosticId::InvalidType,
        diagnostics::DiagnosticSeverity::Error,
        symbol.range,
        "every instance field of readonly struct '" + type.identity.name() +
            "' must be declared readonly");
}

void ProjectMemberModel::validateStaticOwnedMember(
    const ClassMemberSymbol& symbol,
    diagnostics::DiagnosticEngine& diagnostics) const
{
    // Bloque 1: a STATIC field cannot be `owned`, and this has to be said here rather than
    // left to fail downstream.
    //
    // Ownership in this language is discharged by a destruction entry: an instance field
    // declared `owned` is released when its containing object dies (see
    // ProjectOopMirDispatchTable::ownedFields). A static field has no containing object and
    // no end of life -- nothing runs at process exit -- so an `owned` static could only
    // ever be a leak with an ownership annotation on it.
    //
    // Without this the declaration was accepted and the failure surfaced at the first
    // `Slot = move(x)` as "move(...) is only valid for a direct RAII local or owned
    // parameter...", which points at the transfer and says nothing about the `static` that
    // actually caused it -- a diagnostic that sends the reader to fix the wrong line. The
    // check also covers the compiler-generated backing field of a `static owned` automatic
    // property, whose range is the property's own name.
    if (symbol.kind != ClassMemberKind::Field || !symbol.isStatic ||
        symbol.declaredOwnership != sema::OwnershipKind::Owned)
        return;
    diagnostics.report(
        diagnostics::DiagnosticId::InvalidType,
        diagnostics::DiagnosticSeverity::Error,
        symbol.range,
        "static field '" + symbol.name +
            "' cannot be owned because no destruction ever releases it; ownership is "
            "discharged by the containing object's destruction");
}

void ProjectMemberModel::initializeMemberDispatchAndInterop(
    ClassMemberSymbol& symbol,
    const NamedTypeDeclaration& type,
    const ast::AstNode* memberNode,
    diagnostics::DiagnosticEngine& diagnostics) const
{
    symbol.declaringTypeIsStruct = type.kind == NamedTypeDeclarationKind::Struct;
    symbol.hasImplicitThis =
        (symbol.kind == ClassMemberKind::Constructor && !symbol.isStatic) ||
        symbol.kind == ClassMemberKind::Destructor ||
        (symbol.kind == ClassMemberKind::Method && !symbol.isStatic);
    symbol.isAbstract = symbol.kind == ClassMemberKind::Method &&
        memberNode->hasModifier(lexer::TokenKind::KwAbstract);
    symbol.isSealed = symbol.kind == ClassMemberKind::Method &&
        memberNode->hasModifier(lexer::TokenKind::KwSealed);
    symbol.isVirtual = symbol.kind == ClassMemberKind::Method &&
        (memberNode->hasModifier(lexer::TokenKind::KwVirtual) || symbol.isAbstract);
    symbol.isOverride = symbol.kind == ClassMemberKind::Method &&
        memberNode->hasModifier(lexer::TokenKind::KwOverride);
    symbol.isExported = symbol.kind == ClassMemberKind::Method &&
        memberNode->hasModifier(lexer::TokenKind::KwExport);
    symbol.isExternC = symbol.kind == ClassMemberKind::Method &&
        memberNode->hasModifier(lexer::TokenKind::KwExtern);
    if (symbol.isExternC && !symbol.declaredErrorTypeKey.empty())
        diagnostics.report(
            diagnostics::DiagnosticId::InvalidExternCSignature,
            diagnostics::DiagnosticSeverity::Error,
            symbol.range,
            "extern C callable '" + symbol.name +
                "' cannot declare throws; catch and translate the error in an explicit "
                "non-throwing adapter");
    symbol.isVariadic = symbol.kind == ClassMemberKind::Method && memberNode->isVariadic();
    // Interop Foundation: a member born inside `extern C from "<dll>" { ... }` is an
    // IMPORTED C function -- the class-side face of a symbol that lives in an external
    // DLL. Its C symbol is the member name verbatim; the (dll, symbol) pair flows to the
    // linker; and it deliberately has NO body anywhere in the project.
    if (symbol.kind == ClassMemberKind::Method &&
        memberNode->auxiliaryText().starts_with("cimport:"))
    {
        symbol.isImportedC = true;
        symbol.isUnsafe = true;
        symbol.effects = symbol.effects.with(sema::Effect::Unsafe);
        symbol.importDllName = memberNode->auxiliaryText().substr(
            std::string("cimport:").size());
        symbol.cAbiName = symbol.name;
    }
    symbol.isExportedC = symbol.isExported && symbol.isExternC;
    if (symbol.isVariadic && !symbol.isImportedC)
        diagnostics.report(
            diagnostics::DiagnosticId::InvalidExternCSignature,
            diagnostics::DiagnosticSeverity::Error,
            symbol.range,
            "a variadic callable must be a body-less extern C import declaration");
    if (symbol.isReferenceReturn && (symbol.isExportedC || symbol.isImportedC))
        diagnostics.report(
            diagnostics::DiagnosticId::InvalidType,
            diagnostics::DiagnosticSeverity::Error,
            symbol.range,
            "ref returns are a NativeSharp ABI contract and cannot cross an extern C boundary");
    if (symbol.isExportedC) symbol.cAbiName = symbol.name;
}

void ProjectMemberModel::applyExplicitMemberLayout(
    ClassMemberSymbol& symbol,
    const NamedTypeDeclaration& type,
    const ast::AstNode* memberNode,
    diagnostics::DiagnosticEngine& diagnostics) const
{
    // C2.2 "union and explicit layout": `at(N)` on a field. Only an INSTANCE field of a
    // struct or union has a byte offset to name -- a static field is a global, and a
    // class field sits after a header this compiler owns -- so anything else is a
    // diagnostic rather than a silently ignored modifier.
    if (memberNode->auxiliaryText().starts_with("at:"))
    {
        const std::string offsetText =
            memberNode->auxiliaryText().substr(std::string("at:").size());
        symbol.explicitFieldOffset = std::stoull(offsetText);
        if (symbol.kind != ClassMemberKind::Field || symbol.isStatic ||
            type.kind != NamedTypeDeclarationKind::Struct)
        {
            diagnostics.report(
                diagnostics::DiagnosticId::InvalidType,
                diagnostics::DiagnosticSeverity::Error,
                symbol.range,
                "an explicit field offset applies only to an instance field of a struct or union");
            symbol.explicitFieldOffset.reset();
        }
    }
    const std::size_t marker = memberNode->auxiliaryText().find("field-align:");
    if (marker == std::string::npos) return;
    const std::size_t valueBegin = marker + std::string("field-align:").size();
    symbol.explicitFieldAlignment =
        std::stoull(memberNode->auxiliaryText().substr(valueBegin));
    if (symbol.kind != ClassMemberKind::Field || symbol.isStatic)
    {
        diagnostics.report(
            diagnostics::DiagnosticId::InvalidType,
            diagnostics::DiagnosticSeverity::Error,
            symbol.range,
            "explicit field alignment applies only to an instance field");
        symbol.explicitFieldAlignment.reset();
    }
}

void ProjectMemberModel::initializeGeneratedMemberMetadata(
    ClassMemberSymbol& symbol,
    const ast::AstNode* memberNode) const
{
    const std::string& metadata = memberNode->auxiliaryText();
    symbol.isCompilerGenerated = metadata.starts_with("property-backing:") ||
        metadata.starts_with("event-backing:") ||
        metadata.find("anonymous-member-carrier") != std::string::npos ||
        metadata.find("bitfield-unnamed") != std::string::npos;
    symbol.isBitField = metadata.find("bitfield") != std::string::npos;
    symbol.isUnnamedBitField = metadata.find("bitfield-unnamed") != std::string::npos;
    symbol.bitWidthSyntax = symbol.isBitField ? memberNode->second() : nullptr;
    symbol.isEvent = symbol.kind == ClassMemberKind::Event;
    symbol.isEventBacking = metadata.starts_with("event-backing:");
    symbol.isPropertyAccessor = metadata.starts_with("property:get:") ||
        metadata.starts_with("property:set:");
    symbol.isIndexerAccessor = metadata.starts_with("indexer:get:") ||
        metadata.starts_with("indexer:set:");
    symbol.isGetter = metadata.starts_with("property:get:") ||
        metadata.starts_with("indexer:get:");
    symbol.isSetter = metadata.starts_with("property:set:") ||
        metadata.starts_with("indexer:set:");
    symbol.isInit = metadata.find(":init") != std::string::npos;
    symbol.isRequired = metadata.find("required") != std::string::npos;
    symbol.isUserOperator = metadata.starts_with("operator:");
    symbol.isUserConversion = metadata.starts_with("conversion:");
    symbol.isImplicitUserConversion = metadata == "conversion:implicit";
}

void ProjectMemberModel::initializeAccessorMetadata(ClassMemberSymbol& symbol) const
{
    const std::string& metadata = symbol.syntax->auxiliaryText();
    // Bloque 1 item 9: a GETTER of an `owned`-declared property/indexer returns a
    // borrow, not ownership.
    //
    // The parser gives the getter accessor the property's own TypeSyntax as its return
    // type (Parser::parsePropertyTail / parseIndexerTail), so a declaration written
    // `public owned Res Slot { get { return backing; } set { backing = move(value); } }`
    // arrives here as a method whose declared return ownership is Owned. Left alone,
    // ProjectMethodBodyModel::analyzeReturn then demands an ownership-transfer
    // expression from the getter -- "owned return requires an ownership-transfer
    // expression such as move(owner), new T(...), or an owned-returning call" -- which
    // no correct getter can ever supply: the only thing it could move out is the field
    // the object still owns and still destroys.
    //
    // That diagnostic firing on every well-formed getter was the reason `owned` was
    // undeclarable on a property at all. Rather than special-case the check downstream,
    // the ownership is resolved once, here, at the point where the property's shape is
    // still visible: the getter's return is an ordinary (unqualified) reference, and the
    // property's ownership is remembered on `ownsAccessorStorage` so the assignment side
    // can still recognize the setter as a legitimate move(...) destination.
    //
    // Deliberately NOT `Borrowed`: a borrowed return in this language carries a proven
    // lifetime binding back to a live owner local or borrowed parameter, and a field read
    // has no such provenance to offer (measured: `borrowed Res Peek() { return backing; }`
    // is rejected with "borrowed return requires a borrowed value whose lifetime
    // outlives the call"). An unqualified return is what every read-only container
    // accessor in the library already returns today.
    if ((symbol.isPropertyAccessor || symbol.isIndexerAccessor) && symbol.isGetter &&
        symbol.declaredOwnership == sema::OwnershipKind::Owned)
    {
        symbol.ownsAccessorStorage = true;
        symbol.declaredOwnership = sema::OwnershipKind::None;
    }
    if (symbol.isPropertyAccessor)
    {
        const std::size_t prefix = symbol.isGetter
            ? std::string("property:get:").size()
            : std::string("property:set:").size();
        symbol.associatedName = metadata.substr(prefix);
    }
    else if (symbol.isIndexerAccessor) symbol.associatedName = "Item";
    else if (symbol.isEventBacking)
        symbol.associatedName = metadata.substr(std::string("event-backing:").size());
    else if (symbol.isCompilerGenerated)
        symbol.associatedName = metadata.substr(std::string("property-backing:").size());
}

void ProjectMemberModel::validateEventMember(
    const ClassMemberSymbol& symbol,
    const ProjectNameLookup& nameLookup,
    diagnostics::DiagnosticEngine& diagnostics) const
{
    if (!symbol.isEvent) return;
    const NamedTypeDeclaration* eventType =
        nameLookup.resolvedDeclaration(symbol.declaredTypeSyntax);
    if (eventType == nullptr || eventType->kind != NamedTypeDeclarationKind::Delegate)
    {
        diagnostics.report(
            diagnostics::DiagnosticId::InvalidType,
            diagnostics::DiagnosticSeverity::Error,
            symbol.range,
            "Stage 42 event '" + symbol.name + "' must have a delegate type");
    }
    else if (eventType->syntax != nullptr &&
             canonicalTypeKey(eventType->syntax->typeSyntax(), nameLookup) != "builtin:void")
    {
        diagnostics.report(
            diagnostics::DiagnosticId::InvalidType,
            diagnostics::DiagnosticSeverity::Error,
            symbol.range,
            "Stage 42 events require a void-returning delegate type");
    }
}
}
