#include "../Internal/DriverInternal.h"

namespace nativesharp::driver
{
using target::BuiltinLayoutKind;

bool isPubliclyReachableNamedType(std::span<const frontend::NamedTypeDeclaration> types,
                                  std::size_t typeOrdinal) noexcept
{
    while (typeOrdinal < types.size())
    {
        const frontend::NamedTypeDeclaration &type = types[typeOrdinal];
        if (type.accessibility != frontend::NamedTypeAccessibility::Public)
            return false;
        if (!type.containingTypeOrdinal.has_value())
            return true;
        typeOrdinal = *type.containingTypeOrdinal;
    }
    return false;
}

bool isPubliclyReachableMetadataType(const interop::LibraryMetadata &metadata,
                                     const interop::LibraryMetadataType &type) noexcept
{
    constexpr std::uint8_t kPublicAccessibility = 3U;
    const interop::LibraryMetadataType *current = &type;
    while (current != nullptr)
    {
        if (current->accessibility != kPublicAccessibility)
            return false;
        const std::size_t separator = current->fullyQualifiedName.find_last_of('+');
        if (separator == std::string::npos)
            return true;
        const std::string parentName = current->fullyQualifiedName.substr(0, separator);
        current = metadataTypeForKey(metadata, "type:" + metadata.moduleName + "::" + parentName);
        if (current == nullptr)
            return false;
    }
    return false;
}

bool isPublishedNativeConstructor(const interop::LibraryMetadata &metadata,
                                  const interop::LibraryMetadataMember &member) noexcept
{
    constexpr std::uint8_t kConstructorKind = 3U;
    constexpr std::uint8_t kPublicAccessibility = 3U;
    if (member.kind != kConstructorKind || !hasConsumerCallableAccessibility(member.accessibility) ||
        member.mangledName.empty())
    {
        return false;
    }
    const interop::LibraryMetadataType *declaringType = metadataTypeForKey(metadata, member.declaringTypeKey);
    return declaringType != nullptr && declaringType->accessibility == kPublicAccessibility &&
           isPubliclyReachableMetadataType(metadata, *declaringType);
}

bool canMaterializeContractType(const interop::LibraryMetadata &metadata, const std::string &typeKey,
                                std::unordered_set<std::string> &visiting)
{
    const std::string base = metadataBaseTypeKey(typeKey);
    if (base.starts_with("builtin:"))
        return true;
    const interop::LibraryMetadataType *type = metadataTypeForKey(metadata, base);
    if (type == nullptr)
    {
        // A public signature may name a type owned by another LibraryReference. NSMETA keeps
        // ownership honest by storing only the producer's own declarations, so absence here is
        // not evidence that the signature cannot be reconstructed. The generated declaration
        // spells the canonical qualified name from the type key; the combined consumer graph is
        // then responsible for resolving that name to the other metadata module.
        std::string ignoredSourceType;
        std::string ignoredError;
        return sourceTypeForMetadataKey(metadata, typeKey, ignoredSourceType, ignoredError);
    }
    if (type->fullyQualifiedName.find('`') != std::string::npos ||
        (type->kind != interop::LibraryMetadataTypeKind::Class &&
         type->kind != interop::LibraryMetadataTypeKind::Interface &&
         type->kind != interop::LibraryMetadataTypeKind::Struct &&
         type->kind != interop::LibraryMetadataTypeKind::Enum &&
         type->kind != interop::LibraryMetadataTypeKind::Delegate))
    {
        return false;
    }
    if (type->kind == interop::LibraryMetadataTypeKind::Enum)
        return true;
    if (type->kind == interop::LibraryMetadataTypeKind::Delegate)
    {
        const bool externC = (type->delegateFlags & 1U) != 0U;
        // NSMETA 1.5 hizo reconstruible el puntero de funcion C. Un delegate
        // ordinario necesita una garantía adicional: que productor y consumidor
        // hablan la misma celda code/context/ destroy/reserved. Esa garantía
        // aparece por primera vez en el contrato 1.8.
        const bool managedAbiKnown = metadata.formatMinor >= 8 &&
                                     metadata.delegateAbiVersion == interop::kDelegateAbiVersion &&
                                     metadata.managedDelegateCellSize == interop::kManagedDelegateCellSize;
        if (metadata.formatMinor < 5 || (!externC && !managedAbiKnown) || type->delegateReturnTypeKey.empty())
        {
            return false;
        }
        // Re-entering a delegate is a completed nominal edge, not an invalid signature.  A
        // callback can receive a pointer to a struct that stores that same callback type (the
        // ImDrawCallback -> ImDrawCmd* -> ImDrawCallback shape).  Struct recursion below already
        // accepts this closure; rejecting it only at the delegate node made otherwise complete
        // NSMETA methods disappear from the materialized contract and left their physical DLL
        // exports looking unexpected.
        if (!visiting.insert(type->identityKey).second)
            return true;
        bool valid = canMaterializeContractType(metadata, type->delegateReturnTypeKey, visiting);
        if (!type->delegateErrorTypeKey.empty())
            valid = canMaterializeContractType(metadata, type->delegateErrorTypeKey, visiting) && valid;
        for (const std::string &parameterTypeKey : type->delegateParameterTypeKeys)
            valid = canMaterializeContractType(metadata, parameterTypeKey, visiting) && valid;
        visiting.erase(type->identityKey);
        return valid;
    }
    if (type->kind == interop::LibraryMetadataTypeKind::Class ||
        type->kind == interop::LibraryMetadataTypeKind::Interface)
    {
        // Precompiled classes and interfaces are opaque native references in a
        // signature, not values copied by layout. The same is true when the
        // reference is wrapped in a NativeSharp array: source reconstruction
        // preserves the [] suffix while ABI validation remains anchored to the
        // publicly reachable element contract. Interfaces additionally have to
        // be publicly reachable because admitting a hidden contract here would
        // make the physical DLL surface broader than the reconstructible NSMETA
        // surface. A trailing pointer is accepted because ref/out/in parameters
        // use it internally.
        constexpr std::uint8_t kPublicAccessibility = 3U;
        if (type->kind == interop::LibraryMetadataTypeKind::Interface &&
            (type->accessibility != kPublicAccessibility || !isPubliclyReachableMetadataType(metadata, *type)))
        {
            return false;
        }
        if (type->referenceSize != metadata.pointerSize || type->referenceAlignment != metadata.pointerAlignment)
        {
            return false;
        }
        return true;
    }
    if (!visiting.insert(type->identityKey).second)
        return true;
    for (const interop::LibraryMetadataField &field : type->fields)
    {
        if (!canMaterializeContractType(metadata, field.typeKey, visiting))
        {
            visiting.erase(type->identityKey);
            return false;
        }
    }
    visiting.erase(type->identityKey);
    return true;
}

bool canMaterializeContractType(const interop::LibraryMetadata &metadata, const std::string &typeKey)
{
    std::unordered_set<std::string> visiting;
    return canMaterializeContractType(metadata, typeKey, visiting);
}

bool canMaterializeNativeMethod(const interop::LibraryMetadata &metadata, const interop::LibraryMetadataMember &member)
{
    if (!isConsumerCallableNativeMethod(member))
        return false;
    constexpr std::uint32_t kStatic = 1U << 0U;
    constexpr std::uint8_t kPublicAccessibility = 3U;
    const interop::LibraryMetadataType *declaringType = metadataTypeForKey(metadata, member.declaringTypeKey);
    if (declaringType == nullptr || declaringType->accessibility != kPublicAccessibility ||
        !isPubliclyReachableMetadataType(metadata, *declaringType))
        return false;
    if ((member.flags & kStatic) == 0U && declaringType->kind != interop::LibraryMetadataTypeKind::Struct &&
        declaringType->kind != interop::LibraryMetadataTypeKind::Class &&
        declaringType->kind != interop::LibraryMetadataTypeKind::Interface)
    {
        return false;
    }
    // NSMETA already carries the complete layout for structs, the underlying type
    // for enums and the pointer-sized reference carrier for opaque classes. Admit
    // a method only when every nominal type in its signature can be reconstructed
    // under those exact rules.
    if (!canMaterializeContractType(metadata, member.declaredTypeKey))
        return false;
    if (!member.declaredErrorTypeKey.empty() && !canMaterializeContractType(metadata, member.declaredErrorTypeKey))
        return false;
    if (std::ranges::any_of(member.parameters, [&](const auto &parameter)
                            { return !canMaterializeContractType(metadata, parameter.typeKey); }))
    {
        return false;
    }
    if (metadata.formatMinor >= 4)
        return true;
    // NSMETA <=1.3 encoded the pointer carrier but not whether the source said
    // ref/out/in.
    return member.signatureKey.find("ref ") == std::string::npos &&
           member.signatureKey.find("out ") == std::string::npos &&
           member.signatureKey.find("in ") == std::string::npos;
}

bool canMaterializeConstructor(const interop::LibraryMetadata &metadata, const interop::LibraryMetadataMember &member)
{
    constexpr std::uint8_t kConstructorKind = 3U;
    if (member.kind != kConstructorKind || !hasConsumerCallableAccessibility(member.accessibility) ||
        member.mangledName.empty())
    {
        return false;
    }
    const interop::LibraryMetadataType *declaringType = metadataTypeForKey(metadata, member.declaringTypeKey);
    if (declaringType == nullptr || (declaringType->kind != interop::LibraryMetadataTypeKind::Class &&
                                     declaringType->kind != interop::LibraryMetadataTypeKind::Struct))
    {
        return false;
    }
    if (declaringType->kind == interop::LibraryMetadataTypeKind::Struct)
    {
        // Value-type construction calls the constructor symbol directly. Unlike a
        // class allocation it does not require a descriptor accessor, so rejecting
        // it here silently removed a constructor that was already present in NSMETA
        // and in the producer's physical export set.
        return std::ranges::all_of(member.parameters, [&](const auto &parameter)
                                   { return canMaterializeContractType(metadata, parameter.typeKey); });
    }
    const auto dispatch = std::ranges::find_if(metadata.dispatchTables, [&](const auto &candidate)
                                               { return candidate.typeKey == member.declaringTypeKey; });
    if (dispatch == metadata.dispatchTables.end() || dispatch->descriptorAccessorName.empty())
    {
        // NSMETA <=1.5 remains readable and its ordinary methods remain consumable,
        // but it did not publish a PE-safe way to obtain the class descriptor, so
        // `new` is intentionally not materialized for that predecessor.
        return false;
    }
    return std::ranges::all_of(member.parameters, [&](const auto &parameter)
                               { return canMaterializeContractType(metadata, parameter.typeKey); });
}

bool canMaterializePrecompiledDeclaration(const interop::LibraryMetadata &metadata,
                                          const interop::LibraryMetadataMember &member)
{
    constexpr std::uint8_t kFieldKind = 0U;
    constexpr std::uint8_t kEventKind = 1U;
    constexpr std::uint8_t kMethodKind = 2U;
    constexpr std::uint8_t kConstructorKind = 3U;
    constexpr std::uint8_t kDestructorKind = 4U;
    constexpr std::uint32_t kAbstract = 1U << 5U;
    constexpr std::uint32_t kExternC = 1U << 8U;
    constexpr std::uint32_t kExportedC = 1U << 9U;
    constexpr std::uint32_t kCompilerGenerated = 1U << 10U;
    constexpr std::uint32_t kImportedC = 1U << 20U;

    const interop::LibraryMetadataType *declaringType = metadataTypeForKey(metadata, member.declaringTypeKey);
    if (declaringType == nullptr)
        return false;

    if (member.kind == kFieldKind)
    {
        constexpr std::uint32_t kStatic = 1U << 0U;
        constexpr std::uint32_t kConst = 1U << 2U;
        const bool materializable = hasConsumerCallableAccessibility(member.accessibility) &&
                                    canMaterializeContractType(metadata, member.declaredTypeKey);
        if ((member.flags & kConst) != 0U)
            return materializable && member.constantKind != 0U;
        return materializable && (member.flags & kStatic) != 0U && !member.mangledName.empty();
    }
    if (member.kind == kEventKind)
    {
        return hasConsumerCallableAccessibility(member.accessibility) &&
               canMaterializeContractType(metadata, member.declaredTypeKey);
    }
    if (member.kind == kConstructorKind)
        return canMaterializeConstructor(metadata, member);
    if (member.kind == kDestructorKind)
    {
        return declaringType->kind == interop::LibraryMetadataTypeKind::Class && !member.mangledName.empty();
    }
    if (member.kind != kMethodKind || (member.flags & kCompilerGenerated) != 0U)
    {
        return false;
    }
    // A plain `extern C` declaration belongs to the producer's native implementation. Its raw
    // support symbol can remain undefined in a static archive and is intentionally not exported
    // by a DLL. Reconstructing it as an ordinary precompiled NativeSharp method invents a mangled
    // symbol that neither product publishes. Only `export extern C` is part of the consumable
    // NativeSharp surface (and keeps its ordinary NativeSharp entry alongside the C ABI thunk).
    // An imported C declaration is different: its owning DLL is part of NSMETA 1.10 and the
    // consumer must recreate that external import instead of looking for it in the producer.
    if ((member.flags & kExternC) != 0U && (member.flags & (kExportedC | kImportedC)) == 0U)
    {
        return false;
    }

    if (!hasConsumerCallableAccessibility(member.accessibility) &&
        declaringType->kind != interop::LibraryMetadataTypeKind::Interface)
    {
        return false;
    }
    if ((member.flags & kAbstract) == 0U && member.mangledName.empty())
        return false;
    if (!canMaterializeContractType(metadata, member.declaredTypeKey))
        return false;
    if (!member.declaredErrorTypeKey.empty() && !canMaterializeContractType(metadata, member.declaredErrorTypeKey))
        return false;
    return std::ranges::all_of(member.parameters, [&](const auto &parameter)
                               { return canMaterializeContractType(metadata, parameter.typeKey); });
}

std::string_view sourceAccessibility(std::uint8_t accessibility) noexcept
{
    switch (accessibility)
    {
    case 0U:
        return "private";
    case 1U:
        return "protected";
    case 2U:
        return "internal";
    case 3U:
        return "public";
    default:
        return "private";
    }
}

const interop::LibraryMetadataMember *metadataFieldMember(const interop::LibraryMetadata &metadata,
                                                          const interop::LibraryMetadataType &type,
                                                          std::string_view name)
{
    constexpr std::uint8_t kFieldKind = 0U;
    const std::string typeKey = "type:" + type.identityKey;
    auto found = std::lower_bound(metadata.members.begin(), metadata.members.end(), typeKey,
                                  [](const interop::LibraryMetadataMember &member, const std::string &value)
                                  { return member.declaringTypeKey < value; });
    for (; found != metadata.members.end() && found->declaringTypeKey == typeKey; ++found)
    {
        if (found->kind == kFieldKind && found->name == name)
            return &*found;
    }
    return nullptr;
}

const interop::LibraryMetadataMember *metadataEventForBacking(const interop::LibraryMetadata &metadata,
                                                              const interop::LibraryMetadataType &type,
                                                              const interop::LibraryMetadataField &field)
{
    constexpr std::uint8_t kEventKind = 1U;
    constexpr std::uint32_t kCompilerGenerated = 1U << 10U;
    if ((field.flags & kCompilerGenerated) == 0U)
        return nullptr;
    const std::string declaringTypeKey = "type:" + type.identityKey;
    auto found = std::lower_bound(metadata.members.begin(), metadata.members.end(), declaringTypeKey,
                                  [](const interop::LibraryMetadataMember &member, const std::string &value)
                                  { return member.declaringTypeKey < value; });
    for (; found != metadata.members.end() && found->declaringTypeKey == declaringTypeKey; ++found)
    {
        if (found->kind == kEventKind && found->declaredTypeKey == field.typeKey &&
            field.name == "__nsc_event_" + found->name + "$stage42")
            return &*found;
    }
    return nullptr;
}

std::string enumConstantSource(const interop::LibraryMetadataType &type, std::uint64_t bits)
{
    const bool signedUnderlying =
        type.enumUnderlyingTypeKey == "builtin:sbyte" || type.enumUnderlyingTypeKey == "builtin:short" ||
        type.enumUnderlyingTypeKey == "builtin:int" || type.enumUnderlyingTypeKey == "builtin:long" ||
        type.enumUnderlyingTypeKey == "builtin:nint";
    if (signedUnderlying)
        return std::to_string(std::bit_cast<std::int64_t>(bits));
    return std::to_string(bits);
}

bool metadataConstantSource(const interop::LibraryMetadataMember &member, std::string &source, std::string &error)
{
    switch (member.constantKind)
    {
    case 1U:
    {
        const auto signedValue = [&](const unsigned width)
        {
            if (width == 64U)
                return std::bit_cast<std::int64_t>(member.constantBits);
            const std::uint64_t sign = std::uint64_t{1} << (width - 1U);
            const std::uint64_t mask = (std::uint64_t{1} << width) - 1U;
            const std::uint64_t value = member.constantBits & mask;
            return static_cast<std::int64_t>((value ^ sign) - sign);
        };
        if (member.declaredTypeKey == "builtin:sbyte")
            source = std::to_string(signedValue(8));
        else if (member.declaredTypeKey == "builtin:short")
            source = std::to_string(signedValue(16));
        else if (member.declaredTypeKey == "builtin:int")
            source = std::to_string(signedValue(32));
        else if (member.declaredTypeKey == "builtin:long" || member.declaredTypeKey == "builtin:nint")
            source = std::to_string(signedValue(64)) + "l";
        else
        {
            source = std::to_string(member.constantBits);
            if (member.declaredTypeKey == "builtin:uint")
                source += 'u';
            else if (member.declaredTypeKey == "builtin:ulong" || member.declaredTypeKey == "builtin:nuint")
                source += "ul";
        }
        return true;
    }
    case 3U:
        source = member.constantBits == 0U ? "false" : "true";
        return true;
    case 4U:
    {
        std::ostringstream stream;
        stream << std::setprecision(std::numeric_limits<float>::max_digits10)
               << std::bit_cast<float>(static_cast<std::uint32_t>(member.constantBits));
        source = stream.str();
        if (source.find_first_of(".eE") == std::string::npos)
            source += ".0";
        source += 'f';
        return true;
    }
    case 5U:
    {
        std::ostringstream stream;
        stream << std::setprecision(std::numeric_limits<double>::max_digits10)
               << std::bit_cast<double>(member.constantBits);
        source = stream.str();
        if (source.find_first_of(".eE") == std::string::npos)
            source += ".0";
        return true;
    }
    case 6U:
        source = escapedStringLiteral(member.constantText);
        return true;
    default:
        error = "metadata constant '" + member.name + "' has an unsupported value kind";
        return false;
    }
}

std::string_view metadataOperatorToken(std::string_view name) noexcept
{
    if (name == "op_UnaryPlus" || name == "op_Addition")
        return "+";
    if (name == "op_UnaryNegation" || name == "op_Subtraction")
        return "-";
    if (name == "op_Multiply")
        return "*";
    if (name == "op_Division")
        return "/";
    if (name == "op_Modulus")
        return "%";
    if (name == "op_LogicalNot")
        return "!";
    if (name == "op_OnesComplement")
        return "~";
    if (name == "op_BitwiseAnd")
        return "&";
    if (name == "op_BitwiseOr")
        return "|";
    if (name == "op_ExclusiveOr")
        return "^";
    if (name == "op_LeftShift")
        return "<<";
    if (name == "op_RightShift")
        return ">>";
    if (name == "op_Equality")
        return "==";
    if (name == "op_Inequality")
        return "!=";
    if (name == "op_LessThan")
        return "<";
    if (name == "op_LessThanOrEqual")
        return "<=";
    if (name == "op_GreaterThan")
        return ">";
    if (name == "op_GreaterThanOrEqual")
        return ">=";
    return {};
}

std::string parameterPassPrefix(interop::LibraryMetadataParameterPassKind passKind, std::string_view lifetime = {})
{
    std::string result;
    switch (passKind)
    {
    case interop::LibraryMetadataParameterPassKind::Value:
        return result;
    case interop::LibraryMetadataParameterPassKind::Ref:
        result = "ref";
        break;
    case interop::LibraryMetadataParameterPassKind::Out:
        result = "out";
        break;
    case interop::LibraryMetadataParameterPassKind::In:
        result = "in";
        break;
    }
    result += lifetimeAnnotation(lifetime);
    result.push_back(' ');
    return result;
}

std::vector<std::string> publishedNativeSymbols(const interop::LibraryMetadata &metadata)
{
    std::vector<std::string> symbols;
    for (const interop::LibraryMetadataMember &member : metadata.members)
    {
        // Physical validation and source materialization answer different
        // questions. The verifier must account for every symbol the producer
        // exported, including struct constructors that a precompiled consumer
        // does not reconstruct as `new` declarations.
        if (isConsumerCallableNativeMethod(member) && canMaterializeNativeMethod(metadata, member))
        {
            symbols.push_back(member.mangledName);
        }
        else if (isPublishedNativeConstructor(metadata, member))
        {
            symbols.push_back(member.mangledName);
        }
        else if (member.kind == 0U && (member.flags & (1U << 0U)) != 0U && (member.flags & (1U << 2U)) == 0U &&
                 hasConsumerCallableAccessibility(member.accessibility) && !member.mangledName.empty())
        {
            constexpr std::uint8_t kPublicAccessibility = 3U;
            const interop::LibraryMetadataType *declaringType = metadataTypeForKey(metadata, member.declaringTypeKey);
            // A member cannot be more visible than its declaring type. Public fields inside an
            // internal or privately nested type remain producer implementation details and must
            // not become required archive/DLL symbols for consumers.
            if (declaringType != nullptr && declaringType->accessibility == kPublicAccessibility &&
                isPubliclyReachableMetadataType(metadata, *declaringType))
            {
                symbols.push_back(member.mangledName);
            }
        }
    }
    for (const interop::LibraryMetadataNamespaceFunction &function : metadata.namespaceFunctions)
    {
        constexpr std::uint8_t kPublicAccessibility = 1U;
        constexpr std::uint8_t kExternC = 1U << 2U;
        if (function.accessibility == kPublicAccessibility && (function.flags & kExternC) == 0U &&
            !function.mangledName.empty())
        {
            symbols.push_back(function.mangledName);
        }
    }
    constexpr std::uint8_t kPublicAccessibility = 3U;
    for (const interop::LibraryMetadataDispatch &dispatch : metadata.dispatchTables)
    {
        const interop::LibraryMetadataType *type = metadataTypeForKey(metadata, dispatch.typeKey);
        if (type != nullptr && type->accessibility == kPublicAccessibility && (type->flags & (1U << 3U)) == 0U &&
            isPubliclyReachableMetadataType(metadata, *type) && !dispatch.descriptorAccessorName.empty())
        {
            symbols.push_back(dispatch.descriptorAccessorName);
        }
    }
    for (const interop::LibraryMetadataNamespaceState &state : metadata.namespaceStates)
    {
        if (state.accessibility == 1U && (state.flags & 1U) == 0U && !state.accessorName.empty())
        {
            symbols.push_back(state.accessorName);
        }
    }
    std::sort(symbols.begin(), symbols.end());
    symbols.erase(std::unique(symbols.begin(), symbols.end()), symbols.end());
    return symbols;
}

std::vector<std::string> metadataRequiredSymbols(const interop::LibraryMetadata &metadata)
{
    std::vector<std::string> symbols = publishedNativeSymbols(metadata);
    symbols.reserve(symbols.size() + metadata.exports.size());
    for (const interop::LibraryMetadataExport &exportRecord : metadata.exports)
    {
        symbols.push_back(exportRecord.cAbiName);
    }
    std::sort(symbols.begin(), symbols.end());
    symbols.erase(std::unique(symbols.begin(), symbols.end()), symbols.end());
    return symbols;
}

std::vector<std::string> missingSymbols(std::span<const std::string> expected, std::span<const std::string> actual)
{
    const std::unordered_set<std::string> actualSet(actual.begin(), actual.end());
    std::vector<std::string> missing;
    for (const std::string &symbol : expected)
    {
        if (!actualSet.contains(symbol))
            missing.push_back(symbol);
    }
    return missing;
}

std::string describeSymbols(std::span<const std::string> symbols)
{
    std::ostringstream description;
    constexpr std::size_t kDisplayedLimit = 12;
    const std::size_t displayed = std::min(symbols.size(), kDisplayedLimit);
    for (std::size_t index = 0; index < displayed; ++index)
    {
        if (index != 0)
            description << ", ";
        description << '\'' << symbols[index] << '\'';
    }
    if (symbols.size() > displayed)
        description << ", ... (" << (symbols.size() - displayed) << " more)";
    return description.str();
}

void printMetadataAbiContract(const interop::LibraryMetadata &metadata, std::ostream &output)
{
    if (metadata.formatMinor < 8)
    {
        output << "abi-contract: legacy implicit contract (NSMETA " << metadata.formatMajor << '.'
               << metadata.formatMinor << ")\n";
        return;
    }
    output << "native-abi: " << metadata.nativeAbiMajor << '.' << metadata.nativeAbiMinor << '\n'
           << "runtime-abi: " << metadata.runtimeAbiMajor << '.' << metadata.runtimeAbiMinor << '\n'
           << "mangling-abi: " << metadata.manglingAbiVersion << '\n'
           << "object-abi: " << metadata.objectAbiVersion << '\n'
           << "delegate-abi: " << metadata.delegateAbiVersion << '\n'
           << "delegate-cell-size: " << metadata.managedDelegateCellSize << '\n'
           << "exception-abi: " << metadata.exceptionAbiVersion << '\n';
}

bool sourceDefaultValue(const interop::LibraryMetadataParameter &parameter, std::string &source, std::string &error)
{
    using Kind = interop::LibraryMetadataDefaultValueKind;
    switch (parameter.defaultValueKind)
    {
    case Kind::None:
        source.clear();
        return true;
    case Kind::Null:
        source = "null";
        return true;
    case Kind::Boolean:
        source = parameter.defaultValueBits == 0 ? "false" : "true";
        return true;
    case Kind::Integer:
        source = (parameter.defaultValueFlags & 1U) != 0U ? "-" : "";
        source += std::to_string(parameter.defaultValueBits);
        return true;
    case Kind::Floating:
    {
        std::ostringstream stream;
        stream << std::setprecision(std::numeric_limits<double>::max_digits10)
               << std::bit_cast<double>(parameter.defaultValueBits);
        source = stream.str();
        if (source.find_first_of(".eE") == std::string::npos)
            source += ".0";
        return true;
    }
    case Kind::Character:
    {
        const std::uint64_t value = parameter.defaultValueBits;
        if (value == '\\')
            source = "'\\\\'";
        else if (value == '\'')
            source = "'\\\''";
        else if (value == '\n')
            source = "'\\n'";
        else if (value == '\r')
            source = "'\\r'";
        else if (value == '\t')
            source = "'\\t'";
        else if (value >= 32 && value <= 126)
            source = std::string("'") + static_cast<char>(value) + "'";
        else
        {
            error = "metadata-backed source import cannot spell non-ASCII character "
                    "default";
            return false;
        }
        return true;
    }
    case Kind::String:
        source = escapedStringLiteral(parameter.defaultValueText);
        return true;
    }
    error = "metadata contains an unknown default-value kind";
    return false;
}
} // namespace nativesharp::driver
