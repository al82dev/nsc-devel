#include "../Internal/OopLlvmImplementation.h"
#include "../Internal/DropCase.h"
#include "../Internal/LlvmExpressionFrame.h"
#include "../Internal/MatchValuePop.h"
#include "OopLlvmSupport.h"

namespace nativesharp::llvm_backend
{
using namespace llvm_api;

bool OopLlvmImplementation::initializeDebugInformation(std::string& error)
{
        if (debugInformation_ == DebugInformationKind::None) return true;
        if (sourceManager_ == nullptr || memberModel_ == nullptr)
            return fail(error, "debug information requires project members and a source manager");

        debugBuilder_ = LLVMCreateDIBuilder(module_);
        if (debugBuilder_ == nullptr)
            return fail(error, "failed to create the LLVM debug information builder");
        configureDebugModuleFlags(module_, context_);

        LLVMMetadataRef primaryFile = nullptr;
        for (const frontend::ProjectOopMirFunction& function : mirModel_.functions())
        {
            if (function.memberOrdinal >= memberModel_->members().size()) continue;
            const frontend::ClassMemberSymbol& member = memberModel_->members()[function.memberOrdinal];
            if (!member.range.isValid()) continue;
            primaryFile = debugFile(member.range.begin());
            if (primaryFile != nullptr) break;
        }
        if (primaryFile == nullptr)
        {
            static constexpr std::string_view unknown = "unknown.ns";
            primaryFile = LLVMDIBuilderCreateFile(
                debugBuilder_, unknown.data(), unknown.size(), "", 0);
        }
        if (primaryFile == nullptr)
            return fail(error, "failed to create the primary LLVM debug source file");

        static constexpr std::string_view producer = "NativeSharp nsc 3.0.0";
        const LLVMDWARFEmissionKind emission =
            debugInformation_ == DebugInformationKind::LineTablesOnly
            ? LLVMDWARFEmissionLineTablesOnly
            : LLVMDWARFEmissionFull;
        debugCompileUnit_ = LLVMDIBuilderCreateCompileUnit(
            debugBuilder_, LLVMDWARFSourceLanguageC_sharp, primaryFile,
            producer.data(), producer.size(),
            optimizationLevel_ == LlvmOptimizationLevel::O0 ? 0 : 1,
            "", 0, 0, "", 0, emission, 0, 0, 0, "", 0, "", 0);
        if (debugCompileUnit_ == nullptr)
            return fail(error, "failed to create the LLVM debug compile unit");
        return true;
    }

LLVMMetadataRef OopLlvmImplementation::debugFile(SourceLocation location)
{
        return createDebugFile(
            debugBuilder_, sourceManager_, debugPrefixMaps_, sourceChecksums_, debugFiles_,
            debugCodeViewFileNumbers_, location);
    }

unsigned OopLlvmImplementation::debugCodeViewFileNumber(SourceLocation location)
{
        return debugCodeViewFileNumberFor(
            debugBuilder_, sourceManager_, debugPrefixMaps_, sourceChecksums_, debugFiles_,
            debugCodeViewFileNumbers_, location);
    }

LLVMMetadataRef OopLlvmImplementation::createDebugSubprogram(const frontend::ProjectOopMirFunction& function)
{
        if (debugBuilder_ == nullptr || function.isImportedC || function.isImportedNative)
            return nullptr;
        if (function.namespaceFunctionOrdinal.has_value())
        {
            if (function.body == nullptr || function.body->syntax == nullptr)
                return nullptr;
            const SourceRange range = function.body->syntax->range();
            const std::string displayName = function.debugDisplayName.empty()
                ? function.mangledName
                : function.debugDisplayName;
            return createSynthesizedDebugSubprogram(
                range,
                displayName,
                function.mangledName,
                function.referenceReturn ? function.returnTypeKey + "*" : function.returnTypeKey,
                function.parameterTypeKeys,
                true,
                false,
                &function.parameterPassKinds);
        }
        if (function.isSyntheticImplicitConstructor)
        {
            if (function.declaringTypeOrdinal >= nameLookup_.namedTypes().size()) return nullptr;
            const frontend::NamedTypeDeclaration& type =
                nameLookup_.namedTypes()[function.declaringTypeOrdinal];
            const std::vector<std::string> noParameters;
            return createSynthesizedDebugSubprogram(
                type.range,
                type.identity.fullyQualifiedName() + ".implicit-ctor",
                function.mangledName,
                "builtin:void",
                noParameters,
                false,
                false);
        }
        return createMemberDebugSubprogram(function);
    }

LLVMMetadataRef OopLlvmImplementation::createMemberDebugSubprogram(
        const frontend::ProjectOopMirFunction& function)
{
        if (memberModel_ == nullptr || function.memberOrdinal >= memberModel_->members().size())
            return nullptr;
        const frontend::ClassMemberSymbol& member = memberModel_->members()[function.memberOrdinal];
        SourceRange range = member.range;
        if (function.isLambda && function.body != nullptr && function.body->syntax != nullptr &&
            function.body->syntax->range().isValid())
            range = function.body->syntax->range();
        if (!range.isValid()) return nullptr;
        LLVMMetadataRef file = debugFile(range.begin());
        if (file == nullptr) return nullptr;
        const source::SourcePosition position = sourceManager_->getPosition(range.begin());
        std::string displayName =
            nameLookup_.namedTypes()[function.declaringTypeOrdinal].identity.fullyQualifiedName() +
            "." + member.name;
        // NS304-T05-F10: the async poll shares the memberOrdinal with the constructor; its
        // mangled name ends with "$poll". Distinguish the DISubprogram so the poll does not
        // collide with the constructor in the PDB.
        if (function.mangledName.ends_with("$poll"))
            displayName += "$poll";
        if (function.isLambda)
            displayName += ".lambda#" + std::to_string(function.lambdaOrdinal.value_or(0));
        std::vector<LLVMMetadataRef> signatureTypes;
        signatureTypes.reserve(function.parameterTypeKeys.size() + 1);
        signatureTypes.push_back(debugType(
            function.referenceReturn ? function.returnTypeKey + "*" : function.returnTypeKey));
        for (std::size_t index = 0; index < function.parameterTypeKeys.size(); ++index)
        {
            const std::string_view parameterName = index < function.parameterNames.size()
                ? std::string_view(function.parameterNames[index])
                : std::string_view{};
            const frontend::ParameterPassKind passKind =
                index < function.parameterPassKinds.size()
                ? function.parameterPassKinds[index]
                : frontend::ParameterPassKind::Value;
            signatureTypes.push_back(debugParameterType(
                function.parameterTypeKeys[index], passKind, parameterName));
        }
        LLVMMetadataRef functionType = LLVMDIBuilderCreateSubroutineType(
            debugBuilder_, file, signatureTypes.data(),
            static_cast<unsigned>(signatureTypes.size()), LLVMDIFlagZero);
        if (functionType == nullptr) return nullptr;
        return LLVMDIBuilderCreateFunction(
            debugBuilder_, file,
            displayName.data(), displayName.size(),
            function.mangledName.data(), function.mangledName.size(),
            file, position.line, functionType,
            function.isExportedC ? 0 : 1, 1, position.line,
            LLVMDIFlagZero,
            optimizationLevel_ == LlvmOptimizationLevel::O0 ? 0 : 1);
    }

LLVMMetadataRef OopLlvmImplementation::createSynthesizedDebugSubprogram(
        SourceRange range,
        std::string_view displayName,
        std::string_view linkageName,
        std::string_view returnTypeKey,
        const std::vector<std::string>& parameterTypeKeys,
        bool localToUnit,
        bool thunk,
        const std::vector<frontend::ParameterPassKind>* parameterPassKinds )
{
        if (debugBuilder_ == nullptr || sourceManager_ == nullptr)
            return nullptr;
        if (!range.isValid()) return nullptr;
        LLVMMetadataRef file = debugFile(range.begin());
        if (file == nullptr) return nullptr;
        const source::SourcePosition position = sourceManager_->getPosition(range.begin());
        std::vector<LLVMMetadataRef> signatureTypes;
        signatureTypes.reserve(parameterTypeKeys.size() + 1);
        signatureTypes.push_back(debugType(returnTypeKey));
        for (std::size_t index = 0; index < parameterTypeKeys.size(); ++index)
        {
            const frontend::ParameterPassKind passKind =
                parameterPassKinds != nullptr && index < parameterPassKinds->size()
                ? (*parameterPassKinds)[index]
                : frontend::ParameterPassKind::Value;
            signatureTypes.push_back(
                debugParameterType(parameterTypeKeys[index], passKind, {}));
        }
        LLVMMetadataRef functionType = LLVMDIBuilderCreateSubroutineType(
            debugBuilder_, file, signatureTypes.data(),
            static_cast<unsigned>(signatureTypes.size()), LLVMDIFlagZero);
        if (functionType == nullptr) return nullptr;
        LLVMDIFlags flags = LLVMDIFlagArtificial;
        if (thunk)
            flags = static_cast<LLVMDIFlags>(flags | LLVMDIFlagThunk);
        return LLVMDIBuilderCreateFunction(
            debugBuilder_, file,
            displayName.data(), displayName.size(),
            linkageName.data(), linkageName.size(),
            file, position.line, functionType,
            localToUnit ? 1 : 0, 1, position.line, flags,
            optimizationLevel_ == LlvmOptimizationLevel::O0 ? 0 : 1);
    }

LLVMMetadataRef OopLlvmImplementation::debugParameterType(
        std::string_view typeKey,
        frontend::ParameterPassKind passKind,
        std::string_view parameterName)
{
        if (passKind == frontend::ParameterPassKind::Value || !typeKey.ends_with('*'))
            return debugNamedValueType(typeKey, parameterName);

        typeKey.remove_suffix(1);
        LLVMMetadataRef pointee = debugNamedValueType(typeKey, parameterName);
        return pointee != nullptr
            ? LLVMDIBuilderCreateReferenceType(
                debugBuilder_, LLVMDWARFTagReferenceType, pointee)
            : nullptr;
    }

LLVMMetadataRef OopLlvmImplementation::createGeneratedDebugSubprogram(
        const frontend::ProjectOopMirFunction& origin,
        std::string_view role,
        std::string_view linkageName,
        bool localToUnit)
{
        if (origin.namespaceFunctionOrdinal.has_value())
        {
            if (debugBuilder_ == nullptr || origin.body == nullptr ||
                origin.body->syntax == nullptr)
            {
                return nullptr;
            }
            const std::string displayName = (origin.debugDisplayName.empty()
                ? origin.mangledName
                : origin.debugDisplayName) + " [generated: " + std::string(role) + "]";
            return createSynthesizedDebugSubprogram(
                origin.body->syntax->range(),
                displayName,
                linkageName,
                origin.referenceReturn ? origin.returnTypeKey + "*" : origin.returnTypeKey,
                origin.parameterTypeKeys,
                localToUnit,
                true,
                &origin.parameterPassKinds);
        }
        if (debugBuilder_ == nullptr || memberModel_ == nullptr ||
            origin.memberOrdinal >= memberModel_->members().size() ||
            origin.declaringTypeOrdinal >= nameLookup_.namedTypes().size())
            return nullptr;
        const frontend::ClassMemberSymbol& member = memberModel_->members()[origin.memberOrdinal];
        SourceRange range = member.range;
        if (origin.isLambda && origin.body != nullptr && origin.body->syntax != nullptr &&
            origin.body->syntax->range().isValid())
            range = origin.body->syntax->range();
        std::string displayName =
            nameLookup_.namedTypes()[origin.declaringTypeOrdinal].identity.fullyQualifiedName() +
            "." + member.name + " [generated: " + std::string(role) + "]";
        return createSynthesizedDebugSubprogram(
            range, displayName, linkageName, origin.returnTypeKey,
            origin.parameterTypeKeys, localToUnit, true,
            &origin.parameterPassKinds);
    }

LLVMMetadataRef OopLlvmImplementation::debugSubprogramLocation(SourceRange range, LLVMMetadataRef subprogram)
{
        if (subprogram == nullptr || sourceManager_ == nullptr || !range.isValid())
            return nullptr;
        const source::SourcePosition position = sourceManager_->getPosition(range.begin());
        return LLVMDIBuilderCreateDebugLocation(
            context_, position.line, position.column, subprogram, nullptr);
    }

LLVMMetadataRef OopLlvmImplementation::debugSubprogramLocation(
        const frontend::ProjectOopMirFunction& origin,
        LLVMMetadataRef subprogram)
{
        if (origin.namespaceFunctionOrdinal.has_value())
        {
            if (subprogram == nullptr || sourceManager_ == nullptr ||
                origin.body == nullptr || origin.body->syntax == nullptr)
            {
                return nullptr;
            }
            return debugSubprogramLocation(origin.body->syntax->range(), subprogram);
        }
        if (subprogram == nullptr || sourceManager_ == nullptr || memberModel_ == nullptr ||
            origin.memberOrdinal >= memberModel_->members().size())
            return nullptr;
        SourceRange range = memberModel_->members()[origin.memberOrdinal].range;
        if (origin.isLambda && origin.body != nullptr && origin.body->syntax != nullptr &&
            origin.body->syntax->range().isValid())
            range = origin.body->syntax->range();
        return debugSubprogramLocation(range, subprogram);
    }

LLVMMetadataRef OopLlvmImplementation::debugLocation(const frontend::ProjectOopMirNode* node)
{
        if (debugBuilder_ == nullptr || sourceManager_ == nullptr ||
            currentDebugSubprogram_ == nullptr || node == nullptr || node->syntax == nullptr)
            return nullptr;
        const SourceRange range = node->syntax->range();
        if (!range.isValid()) return nullptr;
        const source::SourcePosition position = sourceManager_->getPosition(range.begin());
        LLVMMetadataRef scope = currentDebugScope_ != nullptr
            ? currentDebugScope_
            : currentDebugSubprogram_;
        const std::uint32_t fileKey = range.begin().fileId().value();
        if (fileKey != currentDebugFileKey_)
        {
            if (LLVMMetadataRef file = debugFile(range.begin()); file != nullptr)
            {
                scope = LLVMDIBuilderCreateLexicalBlockFile(
                    debugBuilder_, scope, file, 0);
            }
        }
        return LLVMDIBuilderCreateDebugLocation(
            context_, position.line, position.column, scope, nullptr);
    }

LLVMMetadataRef OopLlvmImplementation::createDebugLexicalScope(
        const frontend::ProjectOopMirNode* node,
        LLVMMetadataRef parent)
{
        if (debugInformation_ != DebugInformationKind::Full || debugBuilder_ == nullptr ||
            sourceManager_ == nullptr || node == nullptr || node->syntax == nullptr ||
            parent == nullptr || !node->syntax->range().isValid())
        {
            return nullptr;
        }
        const SourceRange range = node->syntax->range();
        LLVMMetadataRef file = debugFile(range.begin());
        if (file == nullptr) return nullptr;
        const source::SourcePosition position = sourceManager_->getPosition(range.begin());
        return LLVMDIBuilderCreateLexicalBlock(
            debugBuilder_, parent, file, position.line, position.column);
    }

const frontend::NamedTypeDeclaration* OopLlvmImplementation::debugNamedType(std::string_view typeKey) const noexcept
{
        if (!typeKey.starts_with("type:")) return nullptr;
        const std::string_view key = typeKey.substr(5);
        const std::optional<std::size_t> ordinal = nameLookup_.typeOrdinalForIdentity(key);
        if (ordinal.has_value()) return nameLookup_.typeForOrdinal(*ordinal);
        if (nameLookup_.hasTypeIndex()) return nullptr;
        for (const frontend::NamedTypeDeclaration& type : nameLookup_.namedTypes())
        {
            if (type.identity.keyView() == key) return &type;
        }
        return nullptr;
    }

std::string OopLlvmImplementation::debugNamedDisplayName(const frontend::NamedTypeDeclaration& named) const
{
        constexpr std::string_view specializationPrefix = "stage44-specialization:";
        if (named.syntax == nullptr ||
            !named.syntax->auxiliaryText().starts_with(specializationPrefix))
            return named.identity.fullyQualifiedName();

        std::string display =
            named.syntax->auxiliaryText().substr(specializationPrefix.size());
        if (const std::size_t metadata = display.find(";nsc-meta:"); metadata != std::string::npos)
            display.resize(metadata);
        // A specialization key starts with the project/module identity.  CodeView already scopes
        // this compile unit to that module, so publish the NativeSharp namespace/type spelling.
        if (const std::size_t module = display.find("::"); module != std::string::npos)
            display.erase(0, module + 2);
        // `Box`1<int>` is the canonical arity-bearing key.  The arity is identity machinery, not
        // source syntax; the PDB display name is the closed spelling users wrote: `Box<int>`.
        for (std::size_t tick = display.find('`'); tick != std::string::npos;
             tick = display.find('`', tick))
        {
            std::size_t end = tick + 1;
            while (end < display.size() && display[end] >= '0' && display[end] <= '9') ++end;
            display.erase(tick, end - tick);
        }
        return display;
    }

LLVMDIFlags OopLlvmImplementation::debugMemberFlags(frontend::ClassMemberAccessibility accessibility) noexcept
{
        switch (accessibility)
        {
            case frontend::ClassMemberAccessibility::Private: return LLVMDIFlagPrivate;
            case frontend::ClassMemberAccessibility::Protected: return LLVMDIFlagProtected;
            case frontend::ClassMemberAccessibility::Public: return LLVMDIFlagPublic;
            case frontend::ClassMemberAccessibility::Internal: return LLVMDIFlagZero;
        }
        return LLVMDIFlagZero;
    }

LLVMMetadataRef OopLlvmImplementation::debugClassPointee(
        std::string_view typeKey,
        const frontend::NamedTypeDeclaration& named,
        const frontend::ProjectOopMirClassLayout& layout)
{
        const std::string cacheKey(typeKey);
        if (const auto cached = debugPointeeTypes_.find(cacheKey);
            cached != debugPointeeTypes_.end())
            return cached->second;

        const std::string name = debugNamedDisplayName(named);
        LLVMMetadataRef file = debugFile(named.range.begin());
        const unsigned line = named.range.isValid()
            ? sourceManager_->getPosition(named.range.begin()).line
            : 0;
        const std::string uniqueId = "nativesharp:class:" + named.identity.key();
        LLVMMetadataRef temporary = LLVMDIBuilderCreateReplaceableCompositeType(
            debugBuilder_, LLVMDWARFTagClassType, name.data(), name.size(),
            debugCompileUnit_, file, line, 0, layout.size * 8,
            static_cast<std::uint32_t>(layout.alignment * 8), LLVMDIFlagZero,
            uniqueId.data(), uniqueId.size());
        if (temporary == nullptr) return nullptr;
        debugPointeeTypes_[cacheKey] = temporary;

        LLVMMetadataRef pointer = LLVMDIBuilderCreatePointerType(
            debugBuilder_, temporary, 64, 64, 0, name.data(), name.size());
        if (pointer == nullptr) return nullptr;
        debugTypes_[cacheKey] = pointer;

        LLVMMetadataRef derivedFrom = nullptr;
        if (layout.baseTypeOrdinal.has_value() &&
            *layout.baseTypeOrdinal < nameLookup_.namedTypes().size())
        {
            const frontend::NamedTypeDeclaration& base =
                nameLookup_.namedTypes()[*layout.baseTypeOrdinal];
            const std::string baseKey = "type:" + base.identity.key();
            (void)debugType(baseKey);
            if (const auto found = debugPointeeTypes_.find(std::string(baseKey));
                found != debugPointeeTypes_.end())
                derivedFrom = found->second;
        }

        std::vector<LLVMMetadataRef> members;
        members.reserve(layout.fields.size() + (derivedFrom != nullptr ? 1U : 0U));
        if (derivedFrom != nullptr)
        {
            LLVMMetadataRef inheritance = LLVMDIBuilderCreateInheritance(
                debugBuilder_, temporary, derivedFrom, 0, 0, LLVMDIFlagPublic);
            if (inheritance != nullptr) members.push_back(inheritance);
        }
        appendDebugClassMembers(layout, file, line, temporary, members);

        LLVMMetadataRef complete = LLVMDIBuilderCreateClassType(
            debugBuilder_, debugCompileUnit_, name.data(), name.size(), file, line,
            layout.size * 8, static_cast<std::uint32_t>(layout.alignment * 8), 0,
            LLVMDIFlagZero, derivedFrom, members.data(),
            static_cast<unsigned>(members.size()), nullptr, nullptr,
            uniqueId.data(), uniqueId.size());
        if (complete == nullptr) return nullptr;
        LLVMMetadataReplaceAllUsesWith(temporary, complete);
        debugPointeeTypes_[cacheKey] = complete;
        return complete;
    }

void OopLlvmImplementation::appendDebugClassMembers(
        const frontend::ProjectOopMirClassLayout& layout,
        LLVMMetadataRef file,
        unsigned line,
        LLVMMetadataRef temporary,
        std::vector<LLVMMetadataRef>& members)
{
        for (const frontend::ObjectFieldLayout& field : layout.fields)
        {
            std::string fieldName = "field" + std::to_string(field.memberOrdinal);
            LLVMMetadataRef fieldFile = file;
            unsigned fieldLine = line;
            LLVMDIFlags fieldFlags = LLVMDIFlagZero;
            if (memberModel_ != nullptr && field.memberOrdinal < memberModel_->members().size())
            {
                const frontend::ClassMemberSymbol& member =
                    memberModel_->members()[field.memberOrdinal];
                fieldName = member.name;
                fieldFlags = debugMemberFlags(member.accessibility);
                if (member.range.isValid())
                {
                    fieldFile = debugFile(member.range.begin());
                    fieldLine = sourceManager_->getPosition(member.range.begin()).line;
                }
            }
            LLVMMetadataRef fieldType = debugNamedValueType(field.typeKey, fieldName);
            if (fieldType == nullptr)
                fieldType = LLVMDIBuilderCreateUnspecifiedType(
                    debugBuilder_, field.typeKey.data(), field.typeKey.size());
            LLVMMetadataRef member = LLVMDIBuilderCreateMemberType(
                debugBuilder_, temporary, fieldName.data(), fieldName.size(), fieldFile,
                fieldLine, field.size * 8,
                static_cast<std::uint32_t>(field.alignment * 8), field.offset * 8,
                fieldFlags, fieldType);
            if (member != nullptr) members.push_back(member);
        }
    }

LLVMMetadataRef OopLlvmImplementation::debugHeaderReference(
        std::string_view cacheKeyView,
        std::string_view displayName,
        std::string_view elementTypeKey)
{
        const std::string cacheKey(cacheKeyView);
        const auto cached = debugTypes_.find(cacheKey);
        if (cached != debugTypes_.end()) return cached->second;

        const std::string uniqueId = "nativesharp:header:" + cacheKey;
        LLVMMetadataRef temporary = LLVMDIBuilderCreateReplaceableCompositeType(
            debugBuilder_, LLVMDWARFTagStructureType, displayName.data(), displayName.size(),
            debugCompileUnit_, nullptr, 0, 0, 64, 64, LLVMDIFlagZero,
            uniqueId.data(), uniqueId.size());
        if (temporary == nullptr) return nullptr;
        LLVMMetadataRef pointer = LLVMDIBuilderCreatePointerType(
            debugBuilder_, temporary, 64, 64, 0, displayName.data(), displayName.size());
        if (pointer == nullptr) return nullptr;
        debugTypes_[cacheKey] = pointer;
        debugPointeeTypes_[cacheKey] = temporary;

        LLVMMetadataRef lengthType = debugType("builtin:long");
        LLVMMetadataRef elementType = debugType(elementTypeKey);
        if (elementType == nullptr)
            elementType = LLVMDIBuilderCreateUnspecifiedType(
                debugBuilder_, elementTypeKey.data(), elementTypeKey.size());
        LLVMMetadataRef subrange = LLVMDIBuilderGetOrCreateSubrange(debugBuilder_, 0, 0);
        LLVMMetadataRef flexibleArray = LLVMDIBuilderCreateArrayType(
            debugBuilder_, 0, 8, elementType, &subrange, 1);
        const std::size_t rank =
            frontend::ProjectTypeConversion::nativeArrayRank(cacheKeyView);
        std::vector<LLVMMetadataRef> members;
        members.push_back(LLVMDIBuilderCreateMemberType(
            debugBuilder_, temporary, "Length", 6, nullptr, 0, 64, 64, 0,
            LLVMDIFlagPublic, lengthType));
        if (rank > 1)
        {
            for (std::size_t dimension = 0; dimension < rank; ++dimension)
            {
                const std::string name = "Dimension" + std::to_string(dimension);
                members.push_back(LLVMDIBuilderCreateMemberType(
                    debugBuilder_, temporary, name.data(), name.size(), nullptr, 0, 64, 64,
                    64U * (1U + dimension), LLVMDIFlagPublic, lengthType));
            }
            for (std::size_t dimension = 0; dimension < rank; ++dimension)
            {
                const std::string name = "Stride" + std::to_string(dimension);
                members.push_back(LLVMDIBuilderCreateMemberType(
                    debugBuilder_, temporary, name.data(), name.size(), nullptr, 0, 64, 64,
                    64U * (1U + rank + dimension), LLVMDIFlagPublic, lengthType));
            }
        }
        const std::uint64_t dataOffsetBits = rank > 1
            ? 64U * (1U + 2U * rank)
            : 64U;
        members.push_back(LLVMDIBuilderCreateMemberType(
            debugBuilder_, temporary, "Data", 4, nullptr, 0, 0, 8, dataOffsetBits,
            LLVMDIFlagPublic, flexibleArray));
        LLVMMetadataRef complete = LLVMDIBuilderCreateStructType(
            debugBuilder_, debugCompileUnit_, displayName.data(), displayName.size(),
            nullptr, 0, dataOffsetBits, 64, LLVMDIFlagZero, nullptr, members.data(),
            static_cast<unsigned>(members.size()), 0, nullptr,
            uniqueId.data(), uniqueId.size());
        if (complete == nullptr) return nullptr;
        LLVMMetadataReplaceAllUsesWith(temporary, complete);
        debugPointeeTypes_[cacheKey] = complete;
        return pointer;
    }

bool OopLlvmImplementation::debugNameLooksLikeNativeHandle(std::string_view name) const
{
        std::string lower(name);
        std::transform(lower.begin(), lower.end(), lower.begin(),
            [](unsigned char value) { return static_cast<char>(std::tolower(value)); });
        return lower == "handle" || lower == "handle_" ||
            lower.ends_with("handle") || lower.ends_with("handle_");
    }

LLVMMetadataRef OopLlvmImplementation::debugNativeHandleType()
{
        static constexpr std::string_view cacheKey = "debug:native-handle";
        if (const auto cached = debugTypes_.find(std::string(cacheKey));
            cached != debugTypes_.end())
            return cached->second;
        static constexpr std::string_view pointeeName = "NativeSharp.Win32.HANDLE";
        static constexpr std::string_view pointerName = "HANDLE";
        static constexpr std::string_view uniqueId = "nativesharp:debug:win32-handle";
        LLVMMetadataRef pointee = LLVMDIBuilderCreateStructType(
            debugBuilder_, debugCompileUnit_, pointeeName.data(), pointeeName.size(),
            nullptr, 0, 0, 8, LLVMDIFlagArtificial, nullptr, nullptr, 0, 0, nullptr,
            uniqueId.data(), uniqueId.size());
        LLVMMetadataRef type = pointee != nullptr
            ? LLVMDIBuilderCreatePointerType(
                debugBuilder_, pointee, 64, 64, 0, pointerName.data(), pointerName.size())
            : nullptr;
        if (type != nullptr) debugTypes_.emplace(std::string(cacheKey), type);
        return type;
    }

LLVMMetadataRef OopLlvmImplementation::debugNamedValueType(std::string_view typeKey, std::string_view name)
{
        if ((typeKey == "builtin:nint" || typeKey == "builtin:nuint") &&
            debugNameLooksLikeNativeHandle(name))
        {
            if (LLVMMetadataRef handle = debugNativeHandleType(); handle != nullptr)
                return handle;
        }
        return debugType(typeKey);
    }

LLVMMetadataRef OopLlvmImplementation::debugVariableLengthArrayType(
        std::string_view elementTypeKey,
        std::size_t rank)
{
        const std::string cacheKey = "vla:" + std::string(elementTypeKey) + ":" +
            std::to_string(rank);
        if (const auto cached = debugTypes_.find(cacheKey); cached != debugTypes_.end())
            return cached->second;
        LLVMMetadataRef element = debugType(elementTypeKey);
        if (element == nullptr) return nullptr;
        const std::size_t actualRank = std::max<std::size_t>(1, rank);
        for (std::size_t dimension = 0; dimension < actualRank; ++dimension)
        {
            LLVMMetadataRef subrange = LLVMDIBuilderGetOrCreateSubrange(debugBuilder_, 0, 0);
            element = subrange != nullptr
                ? LLVMDIBuilderCreateArrayType(debugBuilder_, 0, 8, element, &subrange, 1)
                : nullptr;
            if (element == nullptr) return nullptr;
        }
        LLVMMetadataRef pointer = LLVMDIBuilderCreatePointerType(
            debugBuilder_, element, 64, 64, 0, nullptr, 0);
        if (pointer != nullptr) debugTypes_.emplace(cacheKey, pointer);
        return pointer;
    }

LLVMMetadataRef OopLlvmImplementation::debugPlacementType(std::string_view typeKey)
{
        std::string baseKey(typeKey);
        if (!baseKey.empty() && baseKey.back() == '*') baseKey.pop_back();
        const frontend::NamedTypeDeclaration* named = debugNamedType(baseKey);
        if (named == nullptr) return debugType(typeKey);
        const std::string cacheKey = "placement:" + baseKey;
        if (const auto cached = debugTypes_.find(cacheKey); cached != debugTypes_.end())
            return cached->second;

        std::uint64_t size = 0;
        std::uint32_t alignment = 8;
        LLVMMetadataRef base = debugPlacementBase(baseKey, *named, size, alignment);
        if (base == nullptr) return debugType(typeKey);

        const std::string name = "placement " + debugNamedDisplayName(*named);
        const std::string uniqueId = "nativesharp:debug:" + cacheKey;
        LLVMMetadataRef temporary = LLVMDIBuilderCreateReplaceableCompositeType(
            debugBuilder_, LLVMDWARFTagStructureType, name.data(), name.size(),
            debugCompileUnit_, debugFile(named->range.begin()), 0, 0, size, alignment,
            LLVMDIFlagArtificial, uniqueId.data(), uniqueId.size());
        if (temporary == nullptr) return debugType(typeKey);

        LLVMMetadataRef complete = nullptr;
        if (named->kind == frontend::NamedTypeDeclarationKind::Class)
        {
            LLVMMetadataRef inheritance = LLVMDIBuilderCreateInheritance(
                debugBuilder_, temporary, base, 0, 0, LLVMDIFlagPublic);
            LLVMMetadataRef elements[] = {inheritance};
            complete = inheritance != nullptr
                ? LLVMDIBuilderCreateClassType(
                    debugBuilder_, debugCompileUnit_, name.data(), name.size(),
                    debugFile(named->range.begin()), 0, size, alignment, 0,
                    LLVMDIFlagArtificial, base, elements, 1, nullptr, nullptr,
                    uniqueId.data(), uniqueId.size())
                : nullptr;
        }
        else
        {
            static constexpr std::string_view valueName = "Value";
            LLVMMetadataRef member = LLVMDIBuilderCreateMemberType(
                debugBuilder_, temporary, valueName.data(), valueName.size(),
                debugFile(named->range.begin()), 0, size, alignment, 0,
                static_cast<LLVMDIFlags>(LLVMDIFlagPublic | LLVMDIFlagArtificial), base);
            LLVMMetadataRef elements[] = {member};
            complete = member != nullptr
                ? LLVMDIBuilderCreateStructType(
                    debugBuilder_, debugCompileUnit_, name.data(), name.size(),
                    debugFile(named->range.begin()), 0, size, alignment,
                    LLVMDIFlagArtificial, nullptr, elements, 1, 0, nullptr,
                    uniqueId.data(), uniqueId.size())
                : nullptr;
        }
        if (complete == nullptr) return debugType(typeKey);
        LLVMMetadataReplaceAllUsesWith(temporary, complete);
        LLVMMetadataRef pointer = LLVMDIBuilderCreatePointerType(
            debugBuilder_, complete, 64, 64, 0, name.data(), name.size());
        if (pointer != nullptr) debugTypes_.emplace(cacheKey, pointer);
        return pointer != nullptr ? pointer : debugType(typeKey);
    }

LLVMMetadataRef OopLlvmImplementation::debugPlacementBase(
        std::string_view baseKey,
        const frontend::NamedTypeDeclaration& named,
        std::uint64_t& size,
        std::uint32_t& alignment)
{
        LLVMMetadataRef physical = debugType(baseKey);
        LLVMMetadataRef base = physical;
        if (named.kind == frontend::NamedTypeDeclarationKind::Class)
        {
            if (const auto found = debugPointeeTypes_.find(std::string(baseKey));
                found != debugPointeeTypes_.end())
                base = found->second;
            if (const frontend::ProjectOopMirClassLayout* layout = findClassLayout(mirModel_, baseKey);
                layout != nullptr)
            {
                size = layout->size * 8;
                alignment = static_cast<std::uint32_t>(layout->alignment * 8);
            }
        }
        else if (const frontend::ProjectOopMirStructLayout* layout = findStructLayout(mirModel_, baseKey);
                 layout != nullptr)
        {
            size = layout->size * 8;
            alignment = static_cast<std::uint32_t>(layout->alignment * 8);
        }
        return base;
    }

LLVMMetadataRef OopLlvmImplementation::debugDelegateFunctionPointer(
        std::string_view typeKey,
        const frontend::NamedTypeDeclaration& named)
{
        const std::string cacheKey = "delegate-function:" + std::string(typeKey);
        if (const auto cached = debugTypes_.find(cacheKey); cached != debugTypes_.end())
            return cached->second;
        if (memberModel_ == nullptr || named.syntax == nullptr) return debugType("builtin:void*");

        std::vector<LLVMMetadataRef> signatureTypes;
        signatureTypes.push_back(debugType(
            memberModel_->canonicalTypeKey(named.syntax->typeSyntax(), nameLookup_)));
        for (const ast::AstNode* parameter : named.syntax->children())
        {
            if (parameter == nullptr ||
                parameter->kind() != ast::AstNodeKind::ParameterDeclaration)
                continue;
            const std::string parameterType =
                memberModel_->canonicalTypeKey(parameter->typeSyntax(), nameLookup_);
            signatureTypes.push_back(parameterType == typeKey
                ? debugType("builtin:void*")
                : debugType(parameterType));
        }
        LLVMMetadataRef functionType = LLVMDIBuilderCreateSubroutineType(
            debugBuilder_, debugFile(named.range.begin()), signatureTypes.data(),
            static_cast<unsigned>(signatureTypes.size()), LLVMDIFlagZero);
        const std::string name = debugNamedDisplayName(named);
        LLVMMetadataRef pointer = functionType != nullptr
            ? LLVMDIBuilderCreatePointerType(
                debugBuilder_, functionType, 64, 64, 0, name.data(), name.size())
            : nullptr;
        if (pointer != nullptr) debugTypes_.emplace(cacheKey, pointer);
        return pointer;
    }

LLVMMetadataRef OopLlvmImplementation::debugDelegateReference(
        std::string_view typeKey,
        const frontend::NamedTypeDeclaration& named)
{
        const std::string cacheKey(typeKey);
        const auto cached = debugTypes_.find(cacheKey);
        if (cached != debugTypes_.end()) return cached->second;
        const std::string name = debugNamedDisplayName(named);
        const std::string cellName = name + ".delegate-cell";
        const std::string uniqueId = "nativesharp:delegate:" + named.identity.key();
        LLVMMetadataRef temporary = LLVMDIBuilderCreateReplaceableCompositeType(
            debugBuilder_, LLVMDWARFTagStructureType, cellName.data(), cellName.size(),
            debugCompileUnit_, debugFile(named.range.begin()), 0, 0, 256, 64,
            LLVMDIFlagArtificial, uniqueId.data(), uniqueId.size());
        if (temporary == nullptr) return nullptr;
        LLVMMetadataRef pointerType = debugType("builtin:void*");
        LLVMMetadataRef targetType = debugDelegateFunctionPointer(typeKey, named);
        if (targetType == nullptr) targetType = pointerType;
        LLVMMetadataRef fields[] = {
            LLVMDIBuilderCreateMemberType(debugBuilder_, temporary, "Code", 4, nullptr, 0,
                64, 64, 0, LLVMDIFlagPublic, targetType),
            LLVMDIBuilderCreateMemberType(debugBuilder_, temporary, "Receiver", 8, nullptr, 0,
                64, 64, 64, LLVMDIFlagPublic, pointerType),
            LLVMDIBuilderCreateMemberType(debugBuilder_, temporary, "ContextDestroy", 14, nullptr, 0,
                64, 64, 128, LLVMDIFlagPublic, pointerType),
            LLVMDIBuilderCreateMemberType(debugBuilder_, temporary, "CellFree", 8, nullptr, 0,
                64, 64, 192, LLVMDIFlagPublic, pointerType)};
        LLVMMetadataRef complete = LLVMDIBuilderCreateStructType(
            debugBuilder_, debugCompileUnit_, cellName.data(), cellName.size(), nullptr, 0,
            256, 64, LLVMDIFlagArtificial, nullptr, fields, 4, 0, nullptr,
            uniqueId.data(), uniqueId.size());
        if (temporary == nullptr || complete == nullptr) return nullptr;
        LLVMMetadataRef pointer = LLVMDIBuilderCreatePointerType(
            debugBuilder_, complete, 64, 64, 0, name.data(), name.size());
        if (pointer == nullptr) return nullptr;
        LLVMMetadataReplaceAllUsesWith(temporary, complete);
        debugTypes_[cacheKey] = pointer;
        debugPointeeTypes_[cacheKey] = complete;
        return pointer;
    }

std::optional<std::pair<std::string_view, std::uint64_t>> OopLlvmImplementation::debugSimdShape(
        std::string_view typeKey) const noexcept
{
        const frontend::NamedTypeDeclaration* named = debugNamedType(typeKey);
        if (named == nullptr) return std::nullopt;
        const std::string name = debugNamedDisplayName(*named);
        if (name == "NativeSharp.Core.Numerics.Float32x2") return {{"builtin:float", 2}};
        if (name == "NativeSharp.Core.Numerics.Float32x3") return {{"builtin:float", 3}};
        if (name == "NativeSharp.Core.Numerics.Float32x4") return {{"builtin:float", 4}};
        if (name == "NativeSharp.Core.Numerics.Float32x8") return {{"builtin:float", 8}};
        if (name == "NativeSharp.Core.Numerics.Float64x4") return {{"builtin:double", 4}};
        if (name == "NativeSharp.Core.Numerics.Int32x4") return {{"builtin:int", 4}};
        if (name == "NativeSharp.Core.Numerics.Int32x8") return {{"builtin:int", 8}};
        if (name == "NativeSharp.Core.Numerics.Int64x2") return {{"builtin:long", 2}};
        if (name == "NativeSharp.Core.Numerics.Int64x4") return {{"builtin:long", 4}};
        return std::nullopt;
    }

LLVMMetadataRef OopLlvmImplementation::debugBasicType(std::string_view typeKey)
{
        const auto basic = [&](std::string_view name, std::uint64_t bits,
                               LLVMDWARFTypeEncoding encoding)
        {
            return LLVMDIBuilderCreateBasicType(
                debugBuilder_, name.data(), name.size(), bits, encoding, LLVMDIFlagZero);
        };
        if (typeKey == "builtin:bool") return basic("bool", 8, LLVMDWARFTypeEncodingBoolean);
        if (typeKey == "builtin:sbyte") return basic("sbyte", 8, LLVMDWARFTypeEncodingSignedChar);
        if (typeKey == "builtin:byte") return basic("byte", 8, LLVMDWARFTypeEncodingUnsignedChar);
        if (typeKey == "builtin:char") return basic("char", 8, LLVMDWARFTypeEncodingUnsignedChar);
        if (typeKey == "builtin:short") return basic("short", 16, LLVMDWARFTypeEncodingSigned);
        if (typeKey == "builtin:ushort") return basic("ushort", 16, LLVMDWARFTypeEncodingUnsigned);
        if (typeKey == "builtin:int") return basic("int", 32, LLVMDWARFTypeEncodingSigned);
        if (typeKey == "builtin:uint") return basic("uint", 32, LLVMDWARFTypeEncodingUnsigned);
        if (typeKey == "builtin:long" || typeKey == "builtin:nint")
            return basic(typeKey == "builtin:long" ? "long" : "nint", 64,
                         LLVMDWARFTypeEncodingSigned);
        if (typeKey == "builtin:ulong" || typeKey == "builtin:nuint")
            return basic(typeKey == "builtin:ulong" ? "ulong" : "nuint", 64,
                         LLVMDWARFTypeEncodingUnsigned);
        if (typeKey == "builtin:i128") return basic("i128", 128, LLVMDWARFTypeEncodingSigned);
        if (typeKey == "builtin:u128") return basic("u128", 128, LLVMDWARFTypeEncodingUnsigned);
        if (typeKey == "builtin:float") return basic("float", 32, LLVMDWARFTypeEncodingFloat);
        if (typeKey == "builtin:double") return basic("double", 64, LLVMDWARFTypeEncodingFloat);
        // NS304-T16-F05: decimal is a 128-bit carrier (coefficient 96 + scale + sign); the debug
        // type is an unsigned 128-bit basic type so the CodeView/PDB names it "decimal".
        if (typeKey == "builtin:decimal")
            return basic("decimal", 128, LLVMDWARFTypeEncodingUnsigned);
        return nullptr;
    }

LLVMMetadataRef OopLlvmImplementation::debugPointerType(std::string_view typeKey)
{
        const std::string_view pointeeKey = typeKey.substr(0, typeKey.size() - 1);
        LLVMMetadataRef pointee = pointeeKey == "builtin:void"
            ? LLVMDIBuilderCreateUnspecifiedType(debugBuilder_, "void", 4)
            : debugType(pointeeKey);
        const std::string name(typeKey);
        return LLVMDIBuilderCreatePointerType(
            debugBuilder_, pointee, 64, 64, 0, name.data(), name.size());
    }

LLVMMetadataRef OopLlvmImplementation::debugEnumType(
        std::string_view typeKey,
        const frontend::NamedTypeDeclaration& named,
        const frontend::builtins::IntegerTypeInfo& enumInfo)
{
        (void)typeKey;
        std::vector<LLVMMetadataRef> enumerators;
        if (memberModel_ != nullptr)
        {
            const std::size_t typeOrdinal =
                static_cast<std::size_t>(&named - nameLookup_.namedTypes().data());
            const auto appendEnumerator = [&](const frontend::ClassMemberSymbol& member)
            {
                if (!member.isEnumConstant || member.declaringTypeOrdinal != typeOrdinal) return;
                LLVMMetadataRef enumerator = LLVMDIBuilderCreateEnumerator(
                    debugBuilder_, member.name.data(), member.name.size(),
                    member.enumConstantValue, enumInfo.isSigned ? 0 : 1);
                if (enumerator != nullptr) enumerators.push_back(enumerator);
            };
            if (memberModel_->hasMembersDeclaredByTypeIndex())
            {
                for (const std::size_t memberOrdinal :
                     memberModel_->membersDeclaredByType(typeOrdinal))
                {
                    if (memberOrdinal < memberModel_->members().size())
                        appendEnumerator(memberModel_->members()[memberOrdinal]);
                }
            }
            else
            {
                for (const frontend::ClassMemberSymbol& member : memberModel_->members())
                    appendEnumerator(member);
            }
        }
        const std::string name = debugNamedDisplayName(named);
        const std::string underlyingName = name + ".underlying";
        LLVMMetadataRef underlying = LLVMDIBuilderCreateBasicType(
            debugBuilder_, underlyingName.data(), underlyingName.size(), enumInfo.width,
            enumInfo.isSigned ? LLVMDWARFTypeEncodingSigned : LLVMDWARFTypeEncodingUnsigned,
            LLVMDIFlagZero);
        LLVMMetadataRef file = debugFile(named.range.begin());
        const unsigned line = named.range.isValid()
            ? sourceManager_->getPosition(named.range.begin()).line : 0;
        return LLVMDIBuilderCreateEnumerationType(
            debugBuilder_, debugCompileUnit_, name.data(), name.size(), file, line,
            enumInfo.width, enumInfo.width, enumerators.data(),
            static_cast<unsigned>(enumerators.size()), underlying);
    }

std::vector<LLVMMetadataRef> OopLlvmImplementation::debugStructMembers(
        std::string_view typeKey,
        const frontend::ProjectOopMirStructLayout& layout,
        LLVMMetadataRef file,
        unsigned line,
        LLVMMetadataRef temporary)
{
        std::vector<LLVMMetadataRef> members;
        members.reserve(layout.fields.size() + 1U);
        for (const frontend::ObjectFieldLayout& field : layout.fields)
        {
            std::string fieldName = "field" + std::to_string(field.memberOrdinal);
            LLVMMetadataRef fieldFile = file;
            unsigned fieldLine = line;
            LLVMDIFlags fieldFlags = LLVMDIFlagZero;
            if (memberModel_ != nullptr && field.memberOrdinal < memberModel_->members().size())
            {
                const frontend::ClassMemberSymbol& member = memberModel_->members()[field.memberOrdinal];
                fieldName = member.name;
                fieldFlags = debugMemberFlags(member.accessibility);
                if (member.range.isValid())
                {
                    fieldFile = debugFile(member.range.begin());
                    fieldLine = sourceManager_->getPosition(member.range.begin()).line;
                }
            }
            LLVMMetadataRef fieldType = debugNamedValueType(field.typeKey, fieldName);
            if (fieldType == nullptr)
                fieldType = LLVMDIBuilderCreateUnspecifiedType(
                    debugBuilder_, field.typeKey.data(), field.typeKey.size());
            LLVMMetadataRef member = LLVMDIBuilderCreateMemberType(
                debugBuilder_, temporary, fieldName.data(), fieldName.size(), fieldFile,
                fieldLine, field.size * 8, static_cast<std::uint32_t>(field.alignment * 8),
                field.offset * 8, fieldFlags, fieldType);
            if (member != nullptr) members.push_back(member);
        }
        // Item 379. Keep source field names (X/Y/Z/W) and add an overlapping native
        // vector view so VS/WinDbg can inspect lanes and vector registers directly.
        if (const auto simd = debugSimdShape(typeKey); simd.has_value())
        {
            LLVMMetadataRef elementType = debugType(simd->first);
            LLVMMetadataRef subrange = LLVMDIBuilderGetOrCreateSubrange(
                debugBuilder_, 0, static_cast<std::int64_t>(simd->second));
            LLVMMetadataRef lanes = elementType != nullptr && subrange != nullptr
                ? LLVMDIBuilderCreateVectorType(
                    debugBuilder_, layout.size * 8,
                    static_cast<std::uint32_t>(layout.alignment * 8), elementType, &subrange, 1)
                : nullptr;
            static constexpr std::string_view laneName = "Lanes";
            LLVMMetadataRef laneMember = lanes != nullptr
                ? LLVMDIBuilderCreateMemberType(
                    debugBuilder_, temporary, laneName.data(), laneName.size(), file, line,
                    layout.size * 8, static_cast<std::uint32_t>(layout.alignment * 8), 0,
                    static_cast<LLVMDIFlags>(LLVMDIFlagPublic | LLVMDIFlagArtificial), lanes)
                : nullptr;
            if (laneMember != nullptr) members.push_back(laneMember);
        }
        return members;
    }

LLVMMetadataRef OopLlvmImplementation::debugStructType(
        std::string_view typeKey,
        const frontend::NamedTypeDeclaration& named,
        const frontend::ProjectOopMirStructLayout& layout,
        std::string_view cacheKey)
{
        const std::string name = debugNamedDisplayName(named);
        LLVMMetadataRef file = debugFile(named.range.begin());
        const unsigned line = named.range.isValid()
            ? sourceManager_->getPosition(named.range.begin()).line : 0;
        const std::string uniqueId = "nativesharp:" + named.identity.key();
        LLVMMetadataRef temporary = LLVMDIBuilderCreateReplaceableCompositeType(
            debugBuilder_, LLVMDWARFTagStructureType, name.data(), name.size(),
            debugCompileUnit_, file, line, 0, layout.size * 8,
            static_cast<std::uint32_t>(layout.alignment * 8), LLVMDIFlagZero,
            uniqueId.data(), uniqueId.size());
        if (temporary == nullptr) return nullptr;
        debugTypes_.emplace(std::string(cacheKey), temporary);
        std::vector<LLVMMetadataRef> members = debugStructMembers(
            typeKey, layout, file, line, temporary);
        LLVMMetadataRef complete = layout.isUnion
            ? LLVMDIBuilderCreateUnionType(
                debugBuilder_, debugCompileUnit_, name.data(), name.size(), file, line,
                layout.size * 8, static_cast<std::uint32_t>(layout.alignment * 8),
                LLVMDIFlagZero, members.data(), static_cast<unsigned>(members.size()),
                0, uniqueId.data(), uniqueId.size())
            : LLVMDIBuilderCreateStructType(
                debugBuilder_, debugCompileUnit_, name.data(), name.size(), file, line,
                layout.size * 8, static_cast<std::uint32_t>(layout.alignment * 8),
                LLVMDIFlagZero, nullptr, members.data(), static_cast<unsigned>(members.size()),
                0, nullptr, uniqueId.data(), uniqueId.size());
        if (complete == nullptr) return nullptr;
        LLVMMetadataReplaceAllUsesWith(temporary, complete);
        debugTypes_[std::string(cacheKey)] = complete;
        return complete;
    }

LLVMMetadataRef OopLlvmImplementation::debugType(std::string_view typeKey)
{
        if (debugBuilder_ == nullptr || typeKey == "builtin:void") return nullptr;
        const std::string cacheKey(typeKey);
        const auto cached = debugTypes_.find(cacheKey);
        if (cached != debugTypes_.end()) return cached->second;

        LLVMMetadataRef type = debugBasicType(typeKey);
        if (type != nullptr)
        {
        }
        else if (!typeKey.empty() && typeKey.back() == '*')
        {
            type = debugPointerType(typeKey);
        }
        else if (const frontend::NamedTypeDeclaration* named = debugNamedType(typeKey);
                 named != nullptr)
        {
            const std::string name = debugNamedDisplayName(*named);
            if (const frontend::builtins::IntegerTypeInfo* enumInfo = findEnumUnderlyingInfo(nameLookup_, typeKey);
                enumInfo != nullptr)
            {
                type = debugEnumType(typeKey, *named, *enumInfo);
            }
            else if (const frontend::ProjectOopMirStructLayout* layout = findStructLayout(mirModel_, typeKey);
                     layout != nullptr)
            {
                type = debugStructType(typeKey, *named, *layout, cacheKey);
            }
            else if (named->kind == frontend::NamedTypeDeclarationKind::Class)
            {
                if (const frontend::ProjectOopMirClassLayout* layout = findClassLayout(mirModel_, typeKey);
                    layout != nullptr)
                {
                    LLVMMetadataRef pointee = debugClassPointee(typeKey, *named, *layout);
                    const auto pointer = debugTypes_.find(cacheKey);
                    type = pointee != nullptr && pointer != debugTypes_.end()
                        ? pointer->second : nullptr;
                }
            }
            else if (named->kind == frontend::NamedTypeDeclarationKind::Delegate)
            {
                type = named->syntax != nullptr &&
                        named->syntax->hasModifier(lexer::TokenKind::KwExtern)
                    ? debugDelegateFunctionPointer(typeKey, *named)
                    : debugDelegateReference(typeKey, *named);
            }
            else
            {
                LLVMMetadataRef pointee = LLVMDIBuilderCreateUnspecifiedType(
                    debugBuilder_, name.data(), name.size());
                type = LLVMDIBuilderCreatePointerType(
                    debugBuilder_, pointee, 64, 64, 0, name.data(), name.size());
            }
        }
        else
        {
            const std::string name = typeKey == "builtin:string"
                ? "string"
                : std::string(typeKey);
            if (typeKey == "builtin:string" || typeKey == "string-literal")
                return debugHeaderReference(typeKey, name, "builtin:char");
            if (frontend::ProjectTypeConversion::isNativeArray(typeKey))
            {
                const std::optional<std::string> element =
                    frontend::ProjectTypeConversion::nativeArrayElementType(typeKey);
                return debugHeaderReference(
                    typeKey, name, element.has_value() ? std::string_view(*element)
                                                      : std::string_view("builtin:void"));
            }
            LLVMMetadataRef pointee = LLVMDIBuilderCreateUnspecifiedType(
                debugBuilder_, name.data(), name.size());
            if (typeKey == "builtin:string" || typeKey == "string-literal" ||
                frontend::ProjectTypeConversion::isNativeArray(typeKey) ||
                frontend::ProjectTypeConversion::isNativeCoreReference(typeKey))
            {
                type = LLVMDIBuilderCreatePointerType(
                    debugBuilder_, pointee, 64, 64, 0, name.data(), name.size());
            }
            else
            {
                type = pointee;
            }
        }
        if (type != nullptr) debugTypes_.emplace(cacheKey, type);
        return type;
    }

LLVMMetadataRef OopLlvmImplementation::debugOwnershipType(
        std::string_view typeKey,
        sema::OwnershipKind ownership,
        bool automaticDestruction,
        SourceRange range)
{
        LLVMMetadataRef physical = debugType(typeKey);
        if (automaticDestruction) ownership = sema::OwnershipKind::Owned;
        if (physical == nullptr || ownership == sema::OwnershipKind::None) return physical;

        const std::string qualifier = automaticDestruction
            ? "raii"
            : std::string(sema::ownershipKindName(ownership));
        const std::string cacheKey = "ownership:" + qualifier + ":" + std::string(typeKey);
        if (const auto cached = debugTypes_.find(cacheKey); cached != debugTypes_.end())
            return cached->second;

        const frontend::NamedTypeDeclaration* named = debugNamedType(typeKey);
        const std::string name = qualifier + " " +
            (named != nullptr ? debugNamedDisplayName(*named) : std::string(typeKey));
        LLVMMetadataRef file = range.isValid() ? debugFile(range.begin()) : nullptr;
        const unsigned line = range.isValid() && sourceManager_ != nullptr
            ? sourceManager_->getPosition(range.begin()).line
            : 0;

        LLVMMetadataRef qualifiedPointee = nullptr;
        if (named != nullptr && named->kind == frontend::NamedTypeDeclarationKind::Class)
        {
            const frontend::ProjectOopMirClassLayout* layout = findClassLayout(mirModel_, typeKey);
            const auto base = debugPointeeTypes_.find(std::string(typeKey));
            if (layout != nullptr && base != debugPointeeTypes_.end())
            {
                const std::string uniqueId = "nativesharp:ownership:" + cacheKey;
                LLVMMetadataRef temporary = LLVMDIBuilderCreateReplaceableCompositeType(
                    debugBuilder_, LLVMDWARFTagClassType, name.data(), name.size(),
                    debugCompileUnit_, file, line, 0, layout->size * 8,
                    static_cast<std::uint32_t>(layout->alignment * 8), LLVMDIFlagArtificial,
                    uniqueId.data(), uniqueId.size());
                LLVMMetadataRef inheritance = temporary != nullptr
                    ? LLVMDIBuilderCreateInheritance(
                        debugBuilder_, temporary, base->second, 0, 0, LLVMDIFlagPublic)
                    : nullptr;
                LLVMMetadataRef elements[] = {inheritance};
                qualifiedPointee = temporary != nullptr && inheritance != nullptr
                    ? LLVMDIBuilderCreateClassType(
                        debugBuilder_, debugCompileUnit_, name.data(), name.size(), file, line,
                        layout->size * 8, static_cast<std::uint32_t>(layout->alignment * 8), 0,
                        LLVMDIFlagArtificial, base->second, elements, 1, nullptr, nullptr,
                        uniqueId.data(), uniqueId.size())
                    : nullptr;
                if (temporary != nullptr && qualifiedPointee != nullptr)
                    LLVMMetadataReplaceAllUsesWith(temporary, qualifiedPointee);
            }
        }
        if (qualifiedPointee == nullptr)
        {
            const std::string uniqueId = "nativesharp:ownership:" + cacheKey;
            qualifiedPointee = LLVMDIBuilderCreateStructType(
                debugBuilder_, debugCompileUnit_, name.data(), name.size(), file, line,
                64, 64, LLVMDIFlagArtificial, nullptr, nullptr, 0, 0, nullptr,
                uniqueId.data(), uniqueId.size());
        }
        LLVMMetadataRef qualified = qualifiedPointee != nullptr
            ? LLVMDIBuilderCreatePointerType(
                debugBuilder_, qualifiedPointee, 64, 64, 0, name.data(), name.size())
            : nullptr;
        if (qualified != nullptr) debugTypes_.emplace(cacheKey, qualified);
        return qualified != nullptr ? qualified : physical;
    }

bool OopLlvmImplementation::emitDebugParameter(
        const frontend::ProjectOopMirFunction& function,
        std::size_t index,
        LLVMValueRef storage,
        std::string& error)
{
        return emitParameterDebugInfo(
            debugInformation_, debugBuilder_, context_, builder_, currentDebugSubprogram_,
            memberModel_, sourceManager_,
            [&](SourceLocation location) { return debugFile(location); },
            [&](std::string_view typeKey, frontend::ParameterPassKind passKind,
                std::string_view name) {
                return debugParameterType(typeKey, passKind, name);
            },
            [&](std::string_view typeKey, std::string_view name) {
                return debugNamedValueType(typeKey, name);
            },
            [&](std::string_view typeKey, sema::OwnershipKind ownership,
                bool automaticDestruction, SourceRange range) {
                return debugOwnershipType(typeKey, ownership, automaticDestruction, range);
            },
            function, index, storage, error);
    }

bool OopLlvmImplementation::emitDebugCatchParameter(
        const frontend::ProjectOopMirNode* node,
        LLVMValueRef storage,
        std::size_t handlerIndex,
        std::string& error)
{
        return emitCatchParameterDebugInfo(
            debugInformation_, debugBuilder_, context_, builder_, currentDebugSubprogram_,
            currentDebugScope_, currentMirFunction_, sourceManager_, debugDeclaredLocals_,
            [&](SourceLocation location) { return debugFile(location); },
            [&](std::string_view typeKey, std::string_view name) {
                return debugNamedValueType(typeKey, name);
            },
            node, storage, handlerIndex, error);
    }

bool OopLlvmImplementation::emitDebugThis(
        const frontend::ProjectOopMirFunction& function,
        std::string& error)
{
        return emitThisDebugInfo(
            debugInformation_, debugBuilder_, currentDebugSubprogram_, thisValue_, context_,
            builder_, nameLookup_, memberModel_, sourceManager_,
            [&](SourceLocation location) { return debugFile(location); },
            [&](std::string_view typeKey) { return debugType(typeKey); },
            [&](std::string_view typeKey) { return isStructValue(typeKey); },
            [&](std::string_view typeKey, std::string& err) { return llvmType(typeKey, err); },
            function, error);
    }

bool OopLlvmImplementation::emitDebugLocal(
        const frontend::ProjectOopMirNode* node,
        LLVMValueRef storage,
        std::string& error)
{
        LLVMMetadataRef variableScope =
            pendingLocalDebugScope_ != nullptr ? pendingLocalDebugScope_ : currentDebugScope_;
        return emitLocalDebugInfo(
            debugInformation_, debugBuilder_, context_, builder_, currentDebugSubprogram_,
            variableScope, sourceManager_, debugDeclaredLocals_,
            [&](SourceLocation location) { return debugFile(location); },
            [&](std::string_view elementTypeKey, std::size_t rank) {
                return debugVariableLengthArrayType(elementTypeKey, rank);
            },
            [&](std::string_view typeKey) { return debugPlacementType(typeKey); },
            [&](std::string_view typeKey, std::string_view name) {
                return debugNamedValueType(typeKey, name);
            },
            [&](std::string_view typeKey, sema::OwnershipKind ownership,
                bool automaticDestruction, SourceRange range) {
                return debugOwnershipType(typeKey, ownership, automaticDestruction, range);
            },
            node, storage, error);
    }

bool OopLlvmImplementation::emitDebugGlobal(
        const frontend::ProjectOopMirStaticField& field,
        LLVMValueRef global,
        std::string& error)
{
        return emitGlobalDebugInfo(
            debugInformation_, debugBuilder_, debugCompileUnit_, context_, sourceManager_,
            [&](SourceLocation location) { return debugFile(location); },
            [&](std::string_view typeKey, std::string_view name) {
                return debugNamedValueType(typeKey, name);
            },
            field, global, error);
    }

}
