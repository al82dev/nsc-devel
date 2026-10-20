#pragma once

#include "PublicSurfaceBuilder/AccessorContract.h"
#include "PublicSurfaceBuilder/AnonymousCarrierFrame.h"
#include "PublicSurfaceBuilder/PublishedType.h"
#include "PublicSurfaceBuilder/PublishedTypeFrame.h"

#include <cstdint>
#include <map>
#include <set>
#include <span>
#include <sstream>
#include <string>
#include <string_view>
#include <vector>

namespace nativesharp::driver
{
class PublicSurfaceBuilder final
{
  public:
    PublicSurfaceBuilder(const interop::LibraryMetadata &metadata, std::string &source, std::string &error);

    bool run();

  private:
    bool collectBaseAlias(const std::string &typeKey);
    bool sourceBaseType(const std::string &typeKey, std::string &sourceType);
    void emitLifetimeParameters(const std::vector<std::string> &lifetimes);
    void emitLifetimeConstraints(const std::vector<std::pair<std::string, std::string>> &constraints);
    bool emitThrowsContract(const std::string &errorTypeKey);
    bool emitParameter(const interop::LibraryMetadataParameter &parameter, const std::string &memberName,
                       const std::size_t index);
    bool emitParameterSlice(std::string_view memberName, std::span<const interop::LibraryMetadataParameter> parameters,
                            const std::size_t begin, const std::size_t count);
    bool emitParameterRange(std::string_view memberName, std::span<const interop::LibraryMetadataParameter> parameters,
                            const std::size_t count);
    bool emitParameters(const interop::LibraryMetadataMember &member);
    bool emitAssociatedItems(const interop::LibraryMetadataType &owner);
    bool recoverAccessorIdentity(const interop::LibraryMetadataMember &member, bool &indexer, bool &getter,
                                 std::string &associatedName, std::string &contractKey);
    bool collectAccessorContracts(const std::vector<const interop::LibraryMetadataMember *> &members,
                                  std::vector<AccessorContract> &contracts);
    bool validateAccessorContract(const AccessorContract &contract);
    std::uint8_t accessorAccessibility(const AccessorContract &contract);
    void emitAccessorModifiers(const interop::LibraryMetadataMember &shape, bool interfaceContext, bool preserveStatic,
                               bool declarationReadonly);
    bool emitAccessorMember(const interop::LibraryMetadataMember &accessor, const AccessorContract &contract,
                            std::uint8_t accessibility, bool interfaceContext, bool declarationReadonly);
    bool emitAccessorContract(const AccessorContract &contract, bool interfaceContext, bool preserveStatic,
                              std::set<const interop::LibraryMetadataMember *> &materialized);
    bool emitAccessorContracts(const std::vector<const interop::LibraryMetadataMember *> &members,
                               bool interfaceContext, bool preserveStatic,
                               std::set<const interop::LibraryMetadataMember *> &materialized);
    bool emitDelegateType(const interop::LibraryMetadataType &type, std::string_view typeAccessibility,
                          std::string_view typeName, bool topLevel, const std::string &namespaceName);
    bool emitEnumType(const interop::LibraryMetadataType &type, std::string_view typeAccessibility,
                      std::string_view typeName, bool topLevel, const std::string &namespaceName);
    bool emitInterfaceMethod(const interop::LibraryMetadataMember &member, bool isTrait,
                             const std::set<const interop::LibraryMetadataMember *> &materializedAccessors);
    bool emitInterfaceType(const interop::LibraryMetadataType &type,
                           const std::vector<const interop::LibraryMetadataMember *> &orderedMethods,
                           std::string_view typeAccessibility, std::string_view typeName, bool topLevel,
                           const std::string &namespaceName);
    bool emitVariantType(const interop::LibraryMetadataType &type, std::string_view typeAccessibility,
                         std::string_view typeName, bool topLevel, const std::string &namespaceName);
    void emitNominalKindPrefix(const interop::LibraryMetadataType &type);
    void emitRecordDerives(const interop::LibraryMetadataType &type);
    bool emitNominalBases(const interop::LibraryMetadataType &type);
    bool emitNominalHeader(const interop::LibraryMetadataType &type, std::string_view typeAccessibility,
                           const std::string &typeName);
    bool openAnonymousCarrier(AnonymousCarrierFrame &frame);
    bool emitAnonymousField(const AnonymousCarrierFrame &frame, const interop::LibraryMetadataField &child,
                            const std::size_t childIndex);
    bool emitAnonymousCarrier(const interop::LibraryMetadataType &owner, const interop::LibraryMetadataField &carrier,
                              std::string_view indentation);
    bool emitNominalField(const interop::LibraryMetadataType &owner, const interop::LibraryMetadataField &field,
                          std::size_t fieldIndex, std::set<const interop::LibraryMetadataMember *> &materializedEvents);
    bool emitStaticFieldMember(const interop::LibraryMetadataMember &member);
    bool emitEventMember(const interop::LibraryMetadataMember &member,
                         const std::set<const interop::LibraryMetadataMember *> &materializedEvents);
    bool emitConstructorMember(const interop::LibraryMetadataMember &member, const std::string &typeName);
    bool emitImportedMethod(const interop::LibraryMetadataMember &member, const std::string &returnType);
    bool emitUserOperator(const interop::LibraryMetadataMember &member, const std::string &returnType);
    bool emitUserConversion(const interop::LibraryMetadataMember &member, const std::string &returnType);
    bool emitOrdinaryMethod(const interop::LibraryMetadataMember &member, const std::string &returnType);
    bool emitMethodMember(const interop::LibraryMetadataMember &member);
    bool emitNominalMember(const interop::LibraryMetadataMember &member, const std::string &nominalTypeName,
                           const std::set<const interop::LibraryMetadataMember *> &materializedAccessors,
                           const std::set<const interop::LibraryMetadataMember *> &materializedEvents);
    bool emitNominalType(const interop::LibraryMetadataType &type,
                         const std::vector<const interop::LibraryMetadataMember *> &orderedMethods,
                         std::string_view typeAccessibility, const std::string &typeName, bool topLevel,
                         const std::string &namespaceName, const std::map<std::string, std::string> &renderedTypes);
    std::vector<const interop::LibraryMetadataMember *> orderedPublishedMethods(
        const interop::LibraryMetadataType &type, const std::vector<const interop::LibraryMetadataMember *> &methods);
    bool emitPublishedType(const std::string &identity, const bool topLevel,
                           const std::map<std::string, std::string> &renderedTypes);
    std::size_t dispatchRank(const interop::LibraryMetadataType &type, const std::string &declaringTypeKey,
                             const interop::LibraryMetadataDispatch *dispatch,
                             const interop::LibraryMetadataMember *member);

    void collectGenericMethodOwners();
    void collectPublishedTypes();
    bool collectPublishedMembers();
    void collectNamespaceContractTypes();
    bool publishContractTypes();
    bool emitAliases();
    void collectNestedChildren();
    bool renderPublishedTypes();
    bool emitStaticNamespaceExtension(const interop::LibraryMetadataNamespaceFunction &function);
    bool emitInstanceNamespaceExtension(const interop::LibraryMetadataNamespaceFunction &function);
    bool emitNamespaceFunctionDeclaration(const interop::LibraryMetadataNamespaceFunction &function, bool extension,
                                          bool staticExtension, const std::string &returnType);
    bool emitNamespaceFunction(const interop::LibraryMetadataNamespaceFunction &function);
    bool emitNamespaceFunctions();
    bool emitNamespaceState(const interop::LibraryMetadataNamespaceState &state);
    bool emitNamespaceStates();
    bool emitTraitImpl(const interop::LibraryMetadataTraitImpl &traitImpl);
    bool emitTraitImpls();

    const interop::LibraryMetadata &metadata;
    std::string &source;
    std::string &error;
    std::map<std::string, PublishedType> published;
    std::set<std::string> contractTypeIdentities;
    std::set<std::string> genericMethodOwners;
    std::ostringstream generatedStorage;
    std::ostringstream *generated;
    std::map<std::string, std::string> baseAliases;
    std::map<std::string, std::string> extensionReceiverAliases;
    std::map<std::string, std::vector<std::string>> nestedChildren;
};
} // namespace nativesharp::driver
