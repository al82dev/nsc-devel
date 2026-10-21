#include "MemberCompletionResolver.h"
#include "nativesharp/AST/AstNode.h"
#include "nativesharp/AST/GenericParameterSyntax.h"
#include "nativesharp/AST/LifetimeOutlivesConstraint.h"
#include "nativesharp/AST/LifetimeParameterSyntax.h"

#include "nativesharp/Frontend/ProjectInheritanceModel.h"
#include "nativesharp/Frontend/ProjectInterfaceModel.h"
#include "nativesharp/Frontend/ProjectConversionClassifier.h"
#include "nativesharp/Frontend/ProjectMemberAccessibility.h"
#include "nativesharp/Frontend/ProjectMemberModel.h"
#include "nativesharp/Frontend/ProjectMethodBodyModel.h"
#include "nativesharp/Frontend/ProjectNameLookup.h"
#include "nativesharp/Frontend/ProjectNamespaceFunctionModel.h"
#include "nativesharp/Frontend/ProjectSyntaxUnit.h"
#include "nativesharp/Frontend/ProjectTypeConversion.h"
#include "nativesharp/Language/CoroutineSemantics.h"
#include "nativesharp/Language/IterationSemantics.h"
#include "nativesharp/LanguageService/ProtocolTypes.h"
#include "nativesharp/Source/SourceFile.h"

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <limits>
#include <span>
#include <utility>

namespace nativesharp::language_service
{
namespace
{
std::string completionPathKey(const std::filesystem::path& path)
{
    std::error_code error;
    std::filesystem::path absolute = std::filesystem::absolute(path, error);
    if (error) return {};
    std::string result = absolute.lexically_normal().generic_string();
    std::ranges::transform(result, result.begin(), [](unsigned char value) {
        return static_cast<char>(std::tolower(value));
    });
    return result;
}

bool identifierPart(char value) noexcept
{
    const unsigned char byte = static_cast<unsigned char>(value);
    return std::isalnum(byte) != 0 || value == '_';
}

std::string trimText(std::string_view text)
{
    while (!text.empty() && std::isspace(static_cast<unsigned char>(text.front())) != 0)
        text.remove_prefix(1);
    while (!text.empty() && std::isspace(static_cast<unsigned char>(text.back())) != 0)
        text.remove_suffix(1);
    return std::string(text);
}
std::string withOwnershipQualifier(
    std::string type,
    sema::OwnershipKind ownership)
{
    if (ownership == sema::OwnershipKind::None) return type;
    const std::string qualifier(sema::ownershipKindName(ownership));
    if (type == qualifier || type.starts_with(qualifier + " ") ||
        type.starts_with(qualifier + "[")) return type;
    return qualifier + " " + type;
}
}

MemberCompletionResolver::MemberCompletionResolver(
    const frontend::ProjectCompilation& compilation,
    const MemberCompletionRequest& request)
    : compilation_(compilation),
      request_(request)
{
}

bool MemberCompletionResolver::cancellationRequested() const
{
    return request_.cancellationCheck && request_.cancellationCheck();
}

bool MemberCompletionResolver::locateDocument()
{
    const std::string wanted = completionPathKey(request_.documentPath);
    for (const auto& candidate : compilation_.units())
    {
        if (cancellationRequested()) return false;
        if (candidate == nullptr || candidate->isPrecompiled()) continue;
        const source::SourceFile* source =
            compilation_.sourceManager().getFile(candidate->fileId());
        if (source == nullptr || completionPathKey(source->path()) != wanted) continue;
        unit_ = candidate.get();
        file_ = source;
        accessingProjectOrdinal_ = candidate->projectOrdinal();
        return true;
    }
    return false;
}

bool MemberCompletionResolver::locateOperator()
{
    const std::string_view text = file_->buffer();
    const auto requestedOperator =
        byteOffsetForUtf16Position(text, request_.operatorPosition);
    const auto requestedCaret = byteOffsetForUtf16Position(text, request_.caretPosition);
    if (!requestedOperator.has_value() || !requestedCaret.has_value()) return false;

    operatorOffset_ = *requestedOperator;
    caretOffset_ = *requestedCaret;
    if (operatorOffset_ >= text.size()) return false;
    std::size_t dotOffset = operatorOffset_;
    if (text[dotOffset] == '?' && dotOffset + 1 < text.size() && text[dotOffset + 1] == '.')
        ++dotOffset;
    if (text[dotOffset] != '.') return false;
    const bool nullConditional = dotOffset > 0 && text[dotOffset - 1] == '?';
    if (request_.trigger == MemberCompletionTriggerKind::NullConditionalDot && !nullConditional)
        return false;
    if (request_.trigger == MemberCompletionTriggerKind::Dot && nullConditional) return false;
    if (caretOffset_ < dotOffset + 1) return false;

    const std::string writtenPrefix(
        text.substr(dotOffset + 1, caretOffset_ - dotOffset - 1));
    if (!std::ranges::all_of(writtenPrefix, identifierPart)) return false;
    if (!request_.prefix.empty() && request_.prefix != writtenPrefix) return false;
    effectivePrefix_ = request_.prefix;
    receiverEndOffset_ = nullConditional ? dotOffset - 1 : dotOffset;
    while (receiverEndOffset_ > 0 &&
           std::isspace(static_cast<unsigned char>(text[receiverEndOffset_ - 1])) != 0)
    {
        --receiverEndOffset_;
    }
    return receiverEndOffset_ != 0;
}

void MemberCompletionResolver::determineAccessContext(
    const frontend::MethodExpressionTypeBinding* binding)
{
    if (binding == nullptr) return;
    const frontend::ProjectMemberModel* members = compilation_.memberModel();
    if (members == nullptr || binding->methodMemberOrdinal >= members->members().size()) return;
    accessingTypeOrdinal_ =
        members->members()[binding->methodMemberOrdinal].declaringTypeOrdinal;
}

bool MemberCompletionResolver::locateValueReceiver()
{
    const frontend::ProjectMethodBodyModel* bodies = compilation_.methodBodyModel();
    const frontend::ProjectMemberModel* members = compilation_.memberModel();
    if (bodies == nullptr || members == nullptr) return false;
    const frontend::MethodExpressionTypeBinding* best = nullptr;
    std::uint32_t bestBegin = 0;
    for (const frontend::MethodExpressionTypeBinding& binding : bodies->expressionTypes())
    {
        if (cancellationRequested()) return false;
        if (binding.syntax == nullptr || !binding.syntax->range().isValid()) continue;
        const SourceRange range = binding.syntax->range();
        if (range.begin().fileId() != unit_->fileId() ||
            range.end().offset() != receiverEndOffset_) continue;
        if (best == nullptr || range.begin().offset() >= bestBegin)
        {
            best = &binding;
            bestBegin = range.begin().offset();
        }
    }
    if (best == nullptr) return false;
    receiverTypeKey_ = best->typeKey;
    receiverIntegerConstant_ = best->integerConstant;
    receiverTypeOrdinal_ = members->typeOrdinalForTypeKey(receiverTypeKey_);
    receiverAddressable_ = sema::isPlaceCategory(best->valueCategory);
    receiverReadonly_ = best->pointerPointeeReadonly ||
        best->valueCategory == sema::ValueCategory::BorrowedValue;
    baseReceiver_ = best->syntax->kind() == ast::AstNodeKind::BaseExpression;
    determineAccessContext(best);
    return true;
}

bool MemberCompletionResolver::locateTypeReceiver()
{
    const frontend::ProjectNameLookup* names = compilation_.nameLookup();
    if (names == nullptr) return false;
    const frontend::ExpressionTypeNameBinding* best = nullptr;
    std::uint32_t bestBegin = 0;
    for (const frontend::ExpressionTypeNameBinding& binding : names->expressionTypeBindings())
    {
        if (cancellationRequested()) return false;
        if (binding.expression == nullptr || !binding.expression->range().isValid()) continue;
        const SourceRange range = binding.expression->range();
        if (range.begin().fileId() != unit_->fileId() ||
            range.end().offset() != receiverEndOffset_ ||
            binding.candidateTypeOrdinals.size() != 1) continue;
        if (best == nullptr || range.begin().offset() >= bestBegin)
        {
            best = &binding;
            bestBegin = range.begin().offset();
        }
    }
    if (best == nullptr) return false;
    receiverTypeOrdinal_ = best->candidateTypeOrdinals.front();
    if (*receiverTypeOrdinal_ < names->namedTypes().size())
        receiverTypeKey_ = "type:" + names->namedTypes()[*receiverTypeOrdinal_].identity.key();
    if (!best->enclosingTypeOrdinals.empty())
        accessingTypeOrdinal_ = best->enclosingTypeOrdinals.back();
    staticReceiver_ = true;
    return true;
}

bool MemberCompletionResolver::locateNamespaceReceiver()
{
    const std::string_view text = file_->buffer();
    std::size_t begin = receiverEndOffset_;
    while (begin > 0 && (identifierPart(text[begin - 1]) || text[begin - 1] == '.')) --begin;
    std::string written = std::string(text.substr(begin, receiverEndOffset_ - begin));
    if (written.empty() || written.front() == '.' || written.back() == '.')
        return false;
    const std::optional<std::string> resolved = resolveNamespaceName(std::move(written));
    if (!resolved.has_value()) return false;
    namespaceName_ = *resolved;
    namespaceReceiver_ = true;
    staticReceiver_ = true;
    return true;
}

bool MemberCompletionResolver::locateReceiver()
{
    if (locateValueReceiver()) return true;
    if (locateTypeReceiver()) return true;
    return locateNamespaceReceiver();
}

bool MemberCompletionResolver::prefixMatches(std::string_view name) const
{
    if (effectivePrefix_.empty()) return true;
    if (name.size() < effectivePrefix_.size()) return false;
    for (std::size_t index = 0; index < effectivePrefix_.size(); ++index)
    {
        const unsigned char left = static_cast<unsigned char>(name[index]);
        const unsigned char right = static_cast<unsigned char>(effectivePrefix_[index]);
        if (std::tolower(left) != std::tolower(right)) return false;
    }
    return true;
}

std::string MemberCompletionResolver::memberName(
    const frontend::ClassMemberSymbol& member) const
{
    if (member.isPropertyAccessor && !member.associatedName.empty()) return member.associatedName;
    return member.name;
}

bool MemberCompletionResolver::memberMatchesReceiver(
    const frontend::ClassMemberSymbol& member) const
{
    if (member.kind == frontend::ClassMemberKind::Constructor ||
        member.kind == frontend::ClassMemberKind::Destructor ||
        (member.isCompilerGenerated && !member.isAnonymousMemberPromotion) ||
        member.isEventBacking || member.isIndexerAccessor)
    {
        return false;
    }
    return staticReceiver_ ? member.isStatic : !member.isStatic;
}

bool MemberCompletionResolver::memberIsAccessible(
    const frontend::ClassMemberSymbol& member) const
{
    const frontend::ProjectNameLookup* names = compilation_.nameLookup();
    const frontend::ProjectInheritanceModel* inheritance = compilation_.inheritanceModel();
    if (names == nullptr || inheritance == nullptr) return false;
    const frontend::ProjectInterfaceModel* interfaces = compilation_.interfaceModel();
    if (receiverTypeOrdinal_.has_value() && interfaces != nullptr &&
        interfaces->isInterface(*receiverTypeOrdinal_))
    {
        return member.accessibility == frontend::ClassMemberAccessibility::Public;
    }
    if (accessingTypeOrdinal_.has_value())
    {
        const std::optional<std::size_t> protectedReceiver = baseReceiver_
            ? accessingTypeOrdinal_ : receiverTypeOrdinal_;
        return frontend::ProjectMemberAccessibility::isAccessible(
            member, *accessingTypeOrdinal_, protectedReceiver, *names, *inheritance);
    }
    if (member.accessibility == frontend::ClassMemberAccessibility::Public) return true;
    if (member.accessibility != frontend::ClassMemberAccessibility::Internal ||
        member.declaringTypeOrdinal >= names->namedTypes().size()) return false;
    return names->namedTypes()[member.declaringTypeOrdinal].projectOrdinal ==
        accessingProjectOrdinal_;
}

std::string MemberCompletionResolver::readableTypeKey(std::string_view typeKey) const
{
    if (typeKey.empty()) return {};
    if (typeKey == "void") return "void";
    if (typeKey == "string-literal") return "string";
    if (typeKey.starts_with("builtin:")) return std::string(typeKey.substr(8));
    const frontend::ProjectMemberModel* members = compilation_.memberModel();
    const frontend::ProjectNameLookup* names = compilation_.nameLookup();
    if (members != nullptr && names != nullptr)
    {
        const auto ordinal = members->typeOrdinalForTypeKey(typeKey);
        if (ordinal.has_value() && *ordinal < names->namedTypes().size())
            return names->namedTypes()[*ordinal].identity.fullyQualifiedName();
    }
    if (frontend::ProjectTypeConversion::isNativeString(typeKey)) return "string";
    if (frontend::ProjectTypeConversion::isNativeArray(typeKey))
    {
        const auto element = frontend::ProjectTypeConversion::nativeArrayElementType(typeKey);
        return element.has_value() ? readableTypeKey(*element) + "[]" : std::string{};
    }
    if (const auto element = frontend::ProjectMemberModel::fixedArrayElementTypeKey(typeKey);
        element.has_value())
    {
        const auto count = frontend::ProjectMemberModel::fixedArrayCount(typeKey);
        return count.has_value()
            ? readableTypeKey(*element) + "[" + std::to_string(*count) + "]"
            : std::string{};
    }
    if (const auto value = language::futureInnerTypeKey(typeKey); value.has_value())
    {
        std::string result = "Future<" + readableTypeKey(*value);
        if (const auto error = language::futureErrorTypeKey(typeKey); error.has_value())
            result.append(", " + readableTypeKey(*error));
        result.push_back('>');
        return result;
    }
    if (const auto item = language::asyncIteratorInnerTypeKey(typeKey); item.has_value())
    {
        std::string result = "AsyncIterator<" + readableTypeKey(*item);
        if (const auto error = language::asyncIteratorErrorTypeKey(typeKey);
            error.has_value()) result.append(", " + readableTypeKey(*error));
        result.push_back('>');
        return result;
    }
    if (const auto item = language::iteratorInnerTypeKey(typeKey); item.has_value())
    {
        std::string result = "Iterator<" + readableTypeKey(*item);
        if (const auto error = language::iteratorErrorTypeKey(typeKey); error.has_value())
            result.append(", " + readableTypeKey(*error));
        result.push_back('>');
        return result;
    }
    return {};
}

std::string MemberCompletionResolver::readableType(
    const ast::TypeSyntax* syntax,
    std::string_view typeKey) const
{
    if (syntax != nullptr && syntax->range().isValid())
    {
        const std::string written = trimText(compilation_.sourceManager().getText(syntax->range()));
        if (!written.empty()) return written;
    }
    return readableTypeKey(typeKey);
}

std::string MemberCompletionResolver::readableParameter(
    const frontend::MethodParameterSymbol& parameter) const
{
    std::string result;
    if (parameter.isParameterPack) result.append("params ");
    if (parameter.passKind != frontend::ParameterPassKind::Value)
    {
        result.append(frontend::parameterPassKindKeyword(parameter.passKind));
        std::string lifetime;
        if (parameter.syntax != nullptr)
            lifetime = parameter.syntax->lifetimeName();
        if (lifetime.empty() && !parameter.lifetimeName.starts_with("$"))
            lifetime = parameter.lifetimeName;
        if (!lifetime.empty()) result.append("[" + lifetime + "]");
        result.push_back(' ');
    }
    const std::string& typeKey = parameter.passKind == frontend::ParameterPassKind::Value
        ? parameter.typeKey : parameter.pointeeTypeKey;
    std::string type = withOwnershipQualifier(
        readableType(parameter.typeSyntax, typeKey), parameter.ownership);
    result.append(type);
    if (!result.empty() && !parameter.name.empty()) result.push_back(' ');
    result.append(parameter.name);
    if (parameter.defaultValueSyntax != nullptr &&
        parameter.defaultValueSyntax->range().isValid())
    {
        const std::string defaultValue = trimText(
            compilation_.sourceManager().getText(parameter.defaultValueSyntax->range()));
        if (!defaultValue.empty()) result.append(" = " + defaultValue);
    }
    return result;
}

std::string MemberCompletionResolver::readableMemberResult(
    const frontend::ClassMemberSymbol& member) const
{
    std::string result = withOwnershipQualifier(
        readableType(member.declaredTypeSyntax, member.declaredTypeKey),
        member.declaredOwnership);
    if (!member.isReferenceReturn) return result;
    std::string lifetime;
    if (member.syntax != nullptr) lifetime = member.syntax->lifetimeName();
    if (lifetime.empty() && !member.returnLifetimeName.starts_with("$"))
        lifetime = member.returnLifetimeName;
    return "ref" + (lifetime.empty() ? std::string{} : "[" + lifetime + "]") +
        " " + result;
}
std::string MemberCompletionResolver::readableNamespaceResult(
    const frontend::NamespaceFunctionSymbol& function) const
{
    std::string result = withOwnershipQualifier(
        readableType(function.declaredTypeSyntax, function.declaredTypeKey),
        function.declaredOwnership);
    if (!function.isReferenceReturn) return result;
    const ast::AstNode* syntax = function.definitionSyntax != nullptr
        ? function.definitionSyntax : function.declarationSyntax;
    std::string lifetime;
    if (syntax != nullptr) lifetime = syntax->lifetimeName();
    if (lifetime.empty() && !function.returnLifetimeName.starts_with("$"))
        lifetime = function.returnLifetimeName;
    return "ref" + (lifetime.empty() ? std::string{} : "[" + lifetime + "]") +
        " " + result;
}
void MemberCompletionResolver::appendCallableGenericParameters(
    const ast::AstNode* syntax,
    MemberCompletionOverload& overload) const
{
    if (syntax == nullptr) return;
    std::vector<std::pair<std::size_t, std::string>> parameters;
    std::size_t fallbackOrder = std::numeric_limits<std::size_t>::max() / 2U;
    for (const ast::LifetimeParameterSyntax& parameter : syntax->lifetimeParameters())
    {
        std::string text = parameter.range.isValid()
            ? trimText(compilation_.sourceManager().getText(parameter.range)) : std::string{};
        if (text.empty()) text = "lifetime " + parameter.name;
        parameters.emplace_back(
            parameter.range.isValid() ? parameter.range.begin().offset() : fallbackOrder++,
            std::move(text));
    }
    for (const ast::GenericParameterSyntax& parameter : syntax->genericParameterSyntaxes())
    {
        std::string text = parameter.range.isValid()
            ? trimText(compilation_.sourceManager().getText(parameter.range)) : parameter.name;
        parameters.emplace_back(
            parameter.range.isValid() ? parameter.range.begin().offset() : fallbackOrder++,
            std::move(text));
    }
    if (syntax->genericParameterSyntaxes().empty())
        for (const std::string& parameter : syntax->genericParameters())
            parameters.emplace_back(fallbackOrder++, parameter);
    std::ranges::sort(parameters, {}, &std::pair<std::size_t, std::string>::first);
    for (auto& [position, text] : parameters)
    {
        static_cast<void>(position);
        overload.genericParameters.push_back(std::move(text));
    }
}
void MemberCompletionResolver::appendCallableConstraints(
    const ast::AstNode* syntax,
    MemberCompletionOverload& overload) const
{
    if (syntax == nullptr) return;
    std::vector<std::pair<std::size_t, std::string>> constraints;
    std::size_t fallbackOrder = std::numeric_limits<std::size_t>::max() / 2U;
    for (const ast::GenericConstraintClause& constraint : syntax->genericConstraints())
    {
        const std::string text = constraint.range.isValid()
            ? trimText(compilation_.sourceManager().getText(constraint.range)) : std::string{};
        if (!text.empty()) constraints.emplace_back(
            constraint.range.isValid() ? constraint.range.begin().offset() : fallbackOrder++,
            text);
    }
    for (const ast::LifetimeOutlivesConstraint& constraint :
         syntax->lifetimeOutlivesConstraints())
    {
        std::string text =
            "where " + constraint.longer + " : " + constraint.shorter;
        constraints.emplace_back(
            constraint.range.isValid() ? constraint.range.begin().offset() : fallbackOrder++,
            std::move(text));
    }
    std::ranges::sort(constraints, {}, &std::pair<std::size_t, std::string>::first);
    for (auto& [position, text] : constraints)
    {
        static_cast<void>(position);
        overload.constraints.push_back(std::move(text));
    }
}
void MemberCompletionResolver::appendCallableGenericSurface(
    const ast::AstNode* syntax,
    MemberCompletionOverload& overload) const
{
    appendCallableGenericParameters(syntax, overload);
    appendCallableConstraints(syntax, overload);
}
void MemberCompletionResolver::appendModifiers(
    const frontend::ClassMemberSymbol& member,
    MemberCompletionOverload& overload) const
{
    if (member.isStatic) overload.modifiers.push_back("static");
    if (member.isReadonly) overload.modifiers.push_back("readonly");
    if (member.isReferenceReturn) overload.modifiers.push_back("ref");
    if (member.isVirtual) overload.modifiers.push_back("virtual");
    if (member.isOverride) overload.modifiers.push_back("override");
    if (member.isAbstract) overload.modifiers.push_back("abstract");
    if (member.isSealed) overload.modifiers.push_back("sealed");
    if (member.isUnsafe) overload.modifiers.push_back("unsafe");
    if (member.isVariadic) overload.modifiers.push_back("variadic");
}

MemberCompletionOverload MemberCompletionResolver::makeOverload(
    const frontend::ClassMemberSymbol& member,
    std::string_view visibleName) const
{
    return makeProjectedOverload(member, visibleName, 0);
}

MemberCompletionOverload MemberCompletionResolver::makeProjectedOverload(
    const frontend::ClassMemberSymbol& member,
    std::string_view visibleName,
    std::size_t firstParameter) const
{
    MemberCompletionOverload result;
    result.resultType = readableMemberResult(member);
    appendCallableGenericSurface(member.syntax, result);
    std::string signature = result.resultType;
    if (!signature.empty()) signature.push_back(' ');
    signature.append(visibleName);
    if (!result.genericParameters.empty())
    {
        signature.push_back('<');
        for (std::size_t index = 0; index < result.genericParameters.size(); ++index)
        {
            if (index != 0) signature.append(", ");
            signature.append(result.genericParameters[index]);
        }
        signature.push_back('>');
    }
    signature.push_back('(');
    for (std::size_t index = firstParameter; index < member.parameters.size(); ++index)
    {
        const frontend::MethodParameterSymbol& parameter = member.parameters[index];
        std::string text = readableParameter(parameter);
        result.parameters.push_back(text);
        if (index != firstParameter) signature.append(", ");
        signature.append(text);
    }
    if (member.isVariadic)
    {
        if (member.parameters.size() > firstParameter) signature.append(", ");
        signature.append("...");
    }
    signature.push_back(')');
    if (!member.declaredErrorTypeKey.empty())
    {
        signature.append(" throws ");
        signature.append(readableType(
            member.declaredErrorTypeSyntax, member.declaredErrorTypeKey));
    }
    for (const std::string& constraint : result.constraints)
    {
        signature.push_back(' ');
        signature.append(constraint);
    }
    result.signature = std::move(signature);
    appendModifiers(member, result);
    return result;
}

MemberCompletionOverload MemberCompletionResolver::makeNamespaceOverload(
    const frontend::NamespaceFunctionSymbol& function,
    std::size_t firstParameter) const
{
    MemberCompletionOverload result;
    result.resultType = readableNamespaceResult(function);
    const ast::AstNode* syntax = function.definitionSyntax != nullptr
        ? function.definitionSyntax : function.declarationSyntax;
    if (function.isReferenceReturn) result.modifiers.push_back("ref");
    if (syntax != nullptr && syntax->isUnsafe()) result.modifiers.push_back("unsafe");
    if (function.isStaticExtension) result.modifiers.push_back("static");
    if (function.isExtension) result.modifiers.push_back("extension");
    appendCallableGenericSurface(syntax, result);
    std::string signature = result.resultType;
    if (!signature.empty()) signature.push_back(' ');
    signature.append(function.name);
    if (!result.genericParameters.empty())
    {
        signature.push_back('<');
        for (std::size_t index = 0; index < result.genericParameters.size(); ++index)
        {
            if (index != 0) signature.append(", ");
            signature.append(result.genericParameters[index]);
        }
        signature.push_back('>');
    }
    signature.push_back('(');
    for (std::size_t index = firstParameter; index < function.parameters.size(); ++index)
    {
        const frontend::MethodParameterSymbol& parameter = function.parameters[index];
        std::string text = readableParameter(parameter);
        result.parameters.push_back(text);
        if (index != firstParameter) signature.append(", ");
        signature.append(text);
    }
    if (function.isVariadic)
    {
        if (function.parameters.size() > firstParameter) signature.append(", ");
        signature.append("...");
        result.modifiers.push_back("variadic");
    }
    signature.push_back(')');
    if (!function.declaredErrorTypeKey.empty())
    {
        signature.append(" throws ");
        signature.append(readableType(
            function.declaredErrorTypeSyntax, function.declaredErrorTypeKey));
    }
    for (const std::string& constraint : result.constraints)
    {
        signature.push_back(' ');
        signature.append(constraint);
    }
    result.signature = std::move(signature);
    return result;
}

std::optional<std::string> MemberCompletionResolver::documentationFor(
    const frontend::ClassMemberSymbol& member) const
{
    return documentationAt(member.fileId, member.range, member.isPrecompiled);
}

std::optional<std::string> MemberCompletionResolver::documentationAt(
    FileId fileId,
    SourceRange range,
    bool precompiled) const
{
    if (precompiled || !range.isValid()) return std::nullopt;
    const source::SourceFile* source =
        compilation_.sourceManager().getFile(fileId);
    if (source == nullptr || range.begin().offset() > source->buffer().size())
        return std::nullopt;
    const std::string_view buffer = source->buffer();
    std::size_t lineStart = buffer.rfind('\n', range.begin().offset());
    lineStart = lineStart == std::string_view::npos ? 0 : lineStart + 1;
    std::vector<std::string> lines;
    while (lineStart > 0)
    {
        const std::size_t lineEnd = lineStart - 1;
        const std::size_t previousEnd = lineEnd > 0 && buffer[lineEnd - 1] == '\r'
            ? lineEnd - 1 : lineEnd;
        const std::size_t previousNewline = buffer.rfind('\n', previousEnd == 0 ? 0 : previousEnd - 1);
        const std::size_t previousStart = previousNewline == std::string_view::npos
            ? 0 : previousNewline + 1;
        std::string_view line = buffer.substr(previousStart, previousEnd - previousStart);
        while (!line.empty() && std::isspace(static_cast<unsigned char>(line.front())) != 0)
            line.remove_prefix(1);
        if (!line.starts_with("///")) break;
        line.remove_prefix(3);
        if (!line.empty() && line.front() == ' ') line.remove_prefix(1);
        lines.emplace_back(line);
        if (previousStart == 0) break;
        lineStart = previousStart;
    }
    if (lines.empty()) return std::nullopt;
    std::string result;
    for (auto iterator = lines.rbegin(); iterator != lines.rend(); ++iterator)
    {
        if (!result.empty()) result.push_back('\n');
        result.append(*iterator);
    }
    return result;
}

void MemberCompletionResolver::appendMember(std::size_t memberOrdinal)
{
    const frontend::ProjectMemberModel* members = compilation_.memberModel();
    const frontend::ProjectNameLookup* names = compilation_.nameLookup();
    if (members == nullptr || names == nullptr || memberOrdinal >= members->members().size()) return;
    const frontend::ClassMemberSymbol& member = members->members()[memberOrdinal];
    if (!memberMatchesReceiver(member) || !memberIsAccessible(member)) return;
    const std::string visibleName = memberName(member);
    if (visibleName.empty() || !prefixMatches(visibleName)) return;

    MemberCompletionEntryKind kind = MemberCompletionEntryKind::Field;
    if (member.isPropertyAccessor) kind = MemberCompletionEntryKind::Property;
    else if (member.isEnumConstant) kind = MemberCompletionEntryKind::EnumMember;
    else if (member.isConst) kind = MemberCompletionEntryKind::Constant;
    else if (member.kind == frontend::ClassMemberKind::Method)
        kind = MemberCompletionEntryKind::Method;
    else if (member.kind == frontend::ClassMemberKind::Event || member.isEvent)
        kind = MemberCompletionEntryKind::Event;

    const std::string identity = std::to_string(static_cast<int>(kind)) + "\n" +
        visibleName + "\n" + (member.isStatic ? "1" : "0");
    auto found = entryByIdentity_.find(identity);
    if (found == entryByIdentity_.end())
    {
        MemberCompletionEntry entry;
        entry.insertText = visibleName;
        entry.name = visibleName;
        entry.kind = kind;
        entry.isStatic = member.isStatic;
        entry.documentation = documentationFor(member);
        if (member.declaringTypeOrdinal < names->namedTypes().size())
        {
            entry.declaringType =
                names->namedTypes()[member.declaringTypeOrdinal].identity.fullyQualifiedName();
        }
        entry.resultType = readableMemberResult(member);
        if (member.isPropertyAccessor && member.isSetter && !member.parameters.empty())
        {
            const frontend::MethodParameterSymbol& value = member.parameters.back();
            entry.resultType = readableType(value.typeSyntax, value.typeKey);
        }
        const std::size_t index = result_.entries.size();
        result_.entries.push_back(std::move(entry));
        found = entryByIdentity_.emplace(identity, index).first;
    }
    MemberCompletionEntry& entry = result_.entries[found->second];
    if (kind == MemberCompletionEntryKind::Method)
    {
        MemberCompletionOverload overload = makeOverload(member, visibleName);
        if (std::ranges::none_of(entry.overloads, [&](const MemberCompletionOverload& existing) {
                return existing.signature == overload.signature;
            })) entry.overloads.push_back(std::move(overload));
    }
    else if (entry.resultType.empty() || member.isGetter)
    {
        entry.resultType = readableMemberResult(member);
    }
}

void MemberCompletionResolver::appendProjectedMethod(
    const frontend::ClassMemberSymbol& member,
    std::size_t firstParameter,
    bool isStatic)
{
    if (!prefixMatches(member.name) || !memberIsAccessible(member)) return;
    extensionBlockedNames_.insert(member.name);
    const std::string identity = "2\n" + member.name + "\n" + (isStatic ? "1" : "0");
    auto found = entryByIdentity_.find(identity);
    if (found == entryByIdentity_.end())
    {
        MemberCompletionEntry entry;
        entry.insertText = member.name;
        entry.name = member.name;
        entry.kind = MemberCompletionEntryKind::Method;
        entry.isStatic = isStatic;
        entry.resultType = readableMemberResult(member);
        entry.documentation = documentationFor(member);
        const frontend::ProjectNameLookup* names = compilation_.nameLookup();
        if (names != nullptr && member.declaringTypeOrdinal < names->namedTypes().size())
        {
            entry.declaringType =
                names->namedTypes()[member.declaringTypeOrdinal].identity.fullyQualifiedName();
        }
        const std::size_t index = result_.entries.size();
        result_.entries.push_back(std::move(entry));
        found = entryByIdentity_.emplace(identity, index).first;
    }
    MemberCompletionOverload overload =
        makeProjectedOverload(member, member.name, firstParameter);
    MemberCompletionEntry& entry = result_.entries[found->second];
    if (std::ranges::none_of(entry.overloads, [&](const MemberCompletionOverload& existing) {
            return existing.signature == overload.signature;
        })) entry.overloads.push_back(std::move(overload));
}

void MemberCompletionResolver::appendIntrinsicEntry(
    std::string name,
    MemberCompletionEntryKind kind,
    std::string resultType,
    std::string signature)
{
    if (!prefixMatches(name)) return;
    extensionBlockedNames_.insert(name);
    const std::string identity = std::to_string(static_cast<int>(kind)) + "\n" + name + "\n0";
    if (entryByIdentity_.contains(identity)) return;
    MemberCompletionEntry entry;
    entry.insertText = name;
    entry.name = name;
    entry.kind = kind;
    entry.declaringType = readableTypeKey(receiverTypeKey_);
    entry.resultType = resultType;
    if (!signature.empty())
    {
        MemberCompletionOverload overload;
        overload.signature = std::move(signature);
        overload.resultType = resultType;
        entry.overloads.push_back(std::move(overload));
    }
    entryByIdentity_.emplace(identity, result_.entries.size());
    result_.entries.push_back(std::move(entry));
}

void MemberCompletionResolver::appendStringFacadeMembers()
{
    if (staticReceiver_) return;
    const bool stringReceiver =
        frontend::ProjectTypeConversion::isNativeString(receiverTypeKey_) ||
        receiverTypeKey_ == "string-literal";
    const bool primitiveReceiver = receiverTypeKey_ == "builtin:bool" ||
        frontend::ProjectTypeConversion::isNumeric(receiverTypeKey_);
    if (!stringReceiver && !primitiveReceiver) return;
    const frontend::ProjectNameLookup* names = compilation_.nameLookup();
    const frontend::ProjectMemberModel* members = compilation_.memberModel();
    if (names == nullptr || members == nullptr) return;
    if (stringReceiver)
        appendIntrinsicEntry(
            "Length", MemberCompletionEntryKind::Property, "int");
    for (std::size_t typeOrdinal = 0; typeOrdinal < names->namedTypes().size(); ++typeOrdinal)
    {
        if (names->namedTypes()[typeOrdinal].identity.fullyQualifiedName() !=
            "NativeSharp.Core.String") continue;
        for (const std::size_t memberOrdinal : members->membersOfType(typeOrdinal))
        {
            if (cancellationRequested()) return;
            if (memberOrdinal >= members->members().size()) continue;
            const frontend::ClassMemberSymbol& member = members->members()[memberOrdinal];
            if (member.kind != frontend::ClassMemberKind::Method || !member.isStatic ||
                member.parameters.empty()) continue;
            if (!stringReceiver && member.name != "ToString") continue;
            const frontend::MethodParameterSymbol& receiver = member.parameters.front();
            const frontend::ProjectImplicitConversion conversion =
                classifyReceiverConversion(receiver.typeKey);
            if (conversion.exists()) appendProjectedMethod(member, 1, false);
        }
        return;
    }
}

void MemberCompletionResolver::appendIteratorFacadeMembers()
{
    if (staticReceiver_) return;
    const auto item = language::iteratorInnerTypeKey(receiverTypeKey_);
    const auto asyncItem = language::asyncIteratorInnerTypeKey(receiverTypeKey_);
    if (!item.has_value() && !asyncItem.has_value()) return;
    const std::string itemType = readableTypeKey(item.has_value() ? *item : *asyncItem);
    appendIntrinsicEntry(
        "Current", MemberCompletionEntryKind::Property, itemType);
    appendIntrinsicEntry(
        "Drop", MemberCompletionEntryKind::Method, "void", "void Drop()");
    if (asyncItem.has_value())
    {
        std::string futureType = "Future<bool";
        if (const auto error = language::asyncIteratorErrorTypeKey(receiverTypeKey_);
            error.has_value())
        {
            const std::string readableError = readableTypeKey(*error);
            if (!readableError.empty()) futureType.append(", " + readableError);
        }
        futureType.push_back('>');
        appendIntrinsicEntry(
            "MoveNextAsync", MemberCompletionEntryKind::Method,
            futureType, futureType + " MoveNextAsync()");
        appendIntrinsicEntry(
            "Cancel", MemberCompletionEntryKind::Method, "void", "void Cancel()");
        return;
    }
    appendIntrinsicEntry(
        "MoveNext", MemberCompletionEntryKind::Method, "bool", "bool MoveNext()");
    std::string iteratorType = "Iterator<" + itemType;
    if (const auto error = language::iteratorErrorTypeKey(receiverTypeKey_);
        error.has_value())
    {
        const std::string readableError = readableTypeKey(*error);
        if (!readableError.empty()) iteratorType.append(", " + readableError);
    }
    iteratorType.push_back('>');
    appendIntrinsicEntry(
        "GetEnumerator", MemberCompletionEntryKind::Method,
        iteratorType, iteratorType + " GetEnumerator()");
}

void MemberCompletionResolver::appendSynthesizedRootMembers()
{
    if (staticReceiver_ || !receiverTypeOrdinal_.has_value()) return;
    const frontend::ProjectNameLookup* names = compilation_.nameLookup();
    const frontend::ProjectMethodBodyModel* bodies = compilation_.methodBodyModel();
    if (names == nullptr || bodies == nullptr ||
        *receiverTypeOrdinal_ >= names->namedTypes().size()) return;
    const frontend::NamedTypeDeclarationKind kind =
        names->namedTypes()[*receiverTypeOrdinal_].kind;
    const auto appendMethod = [&](std::string name, std::string resultType,
                                  std::string signature)
    {
        if (synthesizedRootBlockedNames_.contains(name)) return;
        extensionBlockedNames_.insert(name);
        appendIntrinsicEntry(
            std::move(name), MemberCompletionEntryKind::Method,
            std::move(resultType), std::move(signature));
    };
    if (kind == frontend::NamedTypeDeclarationKind::Enum)
    {
        appendMethod("ToString", "string", "string ToString()");
        return;
    }
    if (kind != frontend::NamedTypeDeclarationKind::Struct) return;
    const std::string receiverType = readableTypeKey(receiverTypeKey_);
    if (bodies->aggregateAllowsSynthesizedMember(receiverTypeKey_, "ToString"))
        appendMethod("ToString", "string", "string ToString()");
    if (bodies->aggregateAllowsSynthesizedMember(receiverTypeKey_, "Equals"))
        appendMethod(
            "Equals", "bool", "bool Equals(" + receiverType + " other)");
    if (bodies->aggregateAllowsSynthesizedMember(receiverTypeKey_, "GetHashCode"))
        appendMethod("GetHashCode", "int", "int GetHashCode()");
    if (bodies->aggregateAllowsSynthesizedMember(receiverTypeKey_, "Clone"))
        appendMethod("Clone", receiverType, receiverType + " Clone()");
}

const frontend::NamespaceCompletionScope* MemberCompletionResolver::completionScope() const
{
    const frontend::ProjectNamespaceFunctionModel* functions =
        compilation_.namespaceFunctionModel();
    if (functions == nullptr || unit_ == nullptr) return nullptr;
    const frontend::NamespaceCompletionScope* best = nullptr;
    std::uint32_t bestSize = std::numeric_limits<std::uint32_t>::max();
    for (const frontend::NamespaceCompletionScope& scope : functions->completionScopes())
    {
        if (cancellationRequested()) return nullptr;
        if (!scope.range.isValid() || scope.fileId != unit_->fileId() ||
            scope.range.begin().offset() > receiverEndOffset_ ||
            scope.range.end().offset() < receiverEndOffset_) continue;
        const std::uint32_t size =
            scope.range.end().offset() - scope.range.begin().offset();
        if (best == nullptr || size < bestSize)
        {
            best = &scope;
            bestSize = size;
        }
    }
    return best;
}

std::vector<std::string> MemberCompletionResolver::visibleExtensionNamespaces() const
{
    std::vector<std::string> result;
    const frontend::NamespaceCompletionScope* best = completionScope();
    if (best == nullptr) return result;
    result.push_back(best->lexicalNamespace);
    for (const std::string& imported : best->importedNamespaces)
        if (std::ranges::find(result, imported) == result.end()) result.push_back(imported);
    return result;
}

bool MemberCompletionResolver::namespaceExists(std::string_view namespaceName) const
{
    const frontend::ProjectNameLookup* names = compilation_.nameLookup();
    if (names == nullptr) return false;
    std::vector<std::string> segments;
    std::size_t begin = 0;
    while (begin <= namespaceName.size())
    {
        const std::size_t end = namespaceName.find('.', begin);
        segments.emplace_back(namespaceName.substr(begin, end - begin));
        if (end == std::string_view::npos) break;
        begin = end + 1;
    }
    const std::unordered_set<std::size_t> projects = visibleProjects();
    for (const auto& module : names->modules())
    {
        if (cancellationRequested()) return false;
        if (module != nullptr && projects.contains(module->projectOrdinal()) &&
            module->globalNamespace().findNamespace(segments) != nullptr) return true;
    }
    return false;
}

std::optional<std::string> MemberCompletionResolver::resolveNamespaceName(
    std::string written) const
{
    const frontend::NamespaceCompletionScope* scope = completionScope();
    if (scope == nullptr) return std::nullopt;
    const std::size_t firstDot = written.find('.');
    const std::string first = written.substr(0, firstDot);
    const auto alias = scope->aliases.find(first);
    if (alias != scope->aliases.end())
    {
        written = alias->second +
            (firstDot == std::string::npos ? std::string{} : written.substr(firstDot));
        return namespaceExists(written)
            ? std::optional<std::string>(std::move(written)) : std::nullopt;
    }
    std::string lexical = scope->lexicalNamespace;
    while (true)
    {
        const std::string candidate = lexical.empty() ? written : lexical + "." + written;
        if (namespaceExists(candidate)) return candidate;
        const std::size_t separator = lexical.find_last_of('.');
        if (separator == std::string::npos)
        {
            if (lexical.empty()) break;
            lexical.clear();
        }
        else
        {
            lexical.resize(separator);
        }
    }
    return std::nullopt;
}

bool MemberCompletionResolver::extensionMatchesReceiver(
    const frontend::NamespaceFunctionSymbol& function) const
{
    if (staticReceiver_)
        return function.isStaticExtension && function.parameters.empty();
    if (function.isStaticExtension || function.parameters.empty()) return false;
    const frontend::MethodParameterSymbol& receiver = function.parameters.front();
    const std::string& target = receiver.passKind == frontend::ParameterPassKind::Value
        ? receiver.typeKey : receiver.pointeeTypeKey;
    const frontend::ProjectImplicitConversion conversion =
        classifyReceiverConversion(target);
    if (!conversion.exists()) return false;
    if (receiver.passKind != frontend::ParameterPassKind::Value &&
        conversion.kind != frontend::ProjectConversionKind::Identity) return false;
    if (receiver.passKind == frontend::ParameterPassKind::Value) return true;
    return receiverAddressable_ &&
        (receiver.passKind != frontend::ParameterPassKind::Ref || !receiverReadonly_);
}

frontend::ProjectImplicitConversion MemberCompletionResolver::classifyReceiverConversion(
    std::string_view targetTypeKey) const
{
    const frontend::ProjectInheritanceModel* inheritance = compilation_.inheritanceModel();
    if (inheritance == nullptr) return {};
    const frontend::ProjectConversionClassifier classifier(
        *inheritance, compilation_.interfaceModel(), compilation_.memberModel());
    return classifier.classifyImplicit(
        receiverTypeKey_, targetTypeKey, receiverIntegerConstant_);
}

bool MemberCompletionResolver::typeIsAccessible(std::size_t typeOrdinal) const
{
    const frontend::ProjectNameLookup* names = compilation_.nameLookup();
    const frontend::ProjectInheritanceModel* inheritance = compilation_.inheritanceModel();
    if (names == nullptr || inheritance == nullptr ||
        typeOrdinal >= names->namedTypes().size()) return false;
    if (!names->typeIsVisibleFromProject(accessingProjectOrdinal_, typeOrdinal)) return false;
    std::vector<std::size_t> enclosingTypes;
    std::optional<std::size_t> current = accessingTypeOrdinal_;
    while (current.has_value() && *current < names->namedTypes().size())
    {
        enclosingTypes.push_back(*current);
        current = names->namedTypes()[*current].containingTypeOrdinal;
    }
    return names->nestedTypeIsVisibleFrom(typeOrdinal, enclosingTypes) &&
        inheritance->protectedNestedTypeIsAccessibleFrom(
            typeOrdinal, enclosingTypes);
}

void MemberCompletionResolver::appendExtensionMembers()
{
    const frontend::ProjectNameLookup* names = compilation_.nameLookup();
    const frontend::ProjectNamespaceFunctionModel* functions =
        compilation_.namespaceFunctionModel();
    if (names == nullptr || functions == nullptr || accessingProjectOrdinal_ >= names->modules().size())
        return;
    const std::unordered_set<std::size_t> projects = visibleProjects();
    const std::vector<std::string> namespaces = visibleExtensionNamespaces();
    for (const frontend::NamespaceFunctionSymbol& function : functions->functions())
    {
        if (!function.isExtension || !extensionMatchesReceiver(function) ||
            extensionBlockedNames_.contains(function.name) ||
            !prefixMatches(function.name) ||
            !projects.contains(function.projectOrdinal) ||
            std::ranges::find(namespaces, function.namespaceName) == namespaces.end()) continue;
        if (function.accessibility != frontend::NamespaceFunctionAccessibility::Public &&
            function.projectOrdinal != accessingProjectOrdinal_) continue;
        const std::string identity = "2\n" + function.name + "\n" +
            (staticReceiver_ ? "1" : "0");
        auto found = entryByIdentity_.find(identity);
        if (found == entryByIdentity_.end())
        {
            MemberCompletionEntry entry;
            entry.insertText = function.name;
            entry.name = function.name;
            entry.kind = MemberCompletionEntryKind::Method;
            entry.isStatic = staticReceiver_;
            entry.declaringType = function.namespaceName;
            entry.resultType = readableNamespaceResult(function);
            entry.documentation =
                documentationAt(function.fileId, function.range, function.isPrecompiled);
            const std::size_t index = result_.entries.size();
            result_.entries.push_back(std::move(entry));
            found = entryByIdentity_.emplace(identity, index).first;
        }
        const std::size_t firstParameter = function.isStaticExtension ? 0 : 1;
        MemberCompletionOverload overload = makeNamespaceOverload(function, firstParameter);
        MemberCompletionEntry& entry = result_.entries[found->second];
        if (std::ranges::none_of(entry.overloads, [&](const MemberCompletionOverload& existing) {
                return existing.signature == overload.signature;
            })) entry.overloads.push_back(std::move(overload));
        if (cancellationRequested()) return;
    }
}

void MemberCompletionResolver::appendMemberLevel(std::size_t typeOrdinal)
{
    const frontend::ProjectMemberModel* members = compilation_.memberModel();
    if (members == nullptr) return;
    const std::span<const std::size_t> declared = members->membersOfType(typeOrdinal);
    const std::span<const std::size_t> promoted =
        members->promotedMembersOfType(typeOrdinal);
    std::vector<std::size_t> ordinals;
    ordinals.reserve(declared.size() + promoted.size());
    ordinals.insert(ordinals.end(), declared.begin(), declared.end());
    ordinals.insert(ordinals.end(), promoted.begin(), promoted.end());
    std::unordered_set<std::string> levelNames;
    for (const std::size_t ordinal : ordinals)
    {
        if (ordinal >= members->members().size()) continue;
        const frontend::ClassMemberSymbol& member = members->members()[ordinal];
        const std::string name = memberName(member);
        if (hiddenNames_.contains(name)) continue;
        levelNames.insert(name);
        if (!memberMatchesReceiver(member)) continue;
        synthesizedRootBlockedNames_.insert(name);
        extensionBlockedNames_.insert(name);
        appendMember(ordinal);
        if (cancellationRequested()) return;
    }
    hiddenNames_.insert(levelNames.begin(), levelNames.end());
}

void MemberCompletionResolver::appendInterfaceMembers(std::size_t typeOrdinal)
{
    const frontend::ProjectInterfaceModel* interfaces = compilation_.interfaceModel();
    const frontend::ProjectMemberModel* members = compilation_.memberModel();
    if (interfaces == nullptr || members == nullptr) return;
    const frontend::InterfaceTypeInfo* info = interfaces->interfaceInfo(typeOrdinal);
    if (info == nullptr) return;
    for (const std::size_t ordinal : info->effectiveMethodMemberOrdinals)
    {
        if (ordinal < members->members().size() &&
            memberMatchesReceiver(members->members()[ordinal]))
        {
            extensionBlockedNames_.insert(memberName(members->members()[ordinal]));
        }
        appendMember(ordinal);
        if (cancellationRequested()) return;
    }
    for (const std::size_t ordinal : info->requiredStaticMemberOrdinals)
    {
        if (ordinal < members->members().size() &&
            memberMatchesReceiver(members->members()[ordinal]))
        {
            extensionBlockedNames_.insert(memberName(members->members()[ordinal]));
        }
        appendMember(ordinal);
        if (cancellationRequested()) return;
    }
}

void MemberCompletionResolver::appendDefaultInterfaceMembers(std::size_t typeOrdinal)
{
    const frontend::ProjectInterfaceModel* interfaces = compilation_.interfaceModel();
    const frontend::ProjectMemberModel* members = compilation_.memberModel();
    if (interfaces == nullptr || members == nullptr) return;
    for (const frontend::ClassInterfaceTablePlan& plan : interfaces->tablePlans())
    {
        if (plan.classTypeOrdinal != typeOrdinal) continue;
        for (const std::optional<std::size_t> implementation :
             plan.implementationMemberOrdinals)
        {
            if (!implementation.has_value() ||
                *implementation >= members->members().size()) continue;
            const frontend::ClassMemberSymbol& member =
                members->members()[*implementation];
            const std::string name = memberName(member);
            if (hiddenNames_.contains(name) || !memberMatchesReceiver(member)) continue;
            extensionBlockedNames_.insert(name);
            appendMember(*implementation);
            if (cancellationRequested()) return;
        }
    }
}

void MemberCompletionResolver::appendNestedTypes()
{
    if (!staticReceiver_ || !receiverTypeOrdinal_.has_value()) return;
    const frontend::ProjectNameLookup* names = compilation_.nameLookup();
    if (names == nullptr) return;
    for (std::size_t ordinal = 0; ordinal < names->namedTypes().size(); ++ordinal)
    {
        if (cancellationRequested()) return;
        const frontend::NamedTypeDeclaration& type = names->namedTypes()[ordinal];
        if (type.containingTypeOrdinal != receiverTypeOrdinal_ ||
            !prefixMatches(type.identity.name())) continue;
        if (!typeIsAccessible(ordinal)) continue;
        MemberCompletionEntry entry;
        entry.insertText = type.identity.name();
        entry.name = type.identity.name();
        entry.kind = MemberCompletionEntryKind::Type;
        entry.isStatic = true;
        entry.declaringType = names->namedTypes()[*receiverTypeOrdinal_].identity.fullyQualifiedName();
        entry.resultType = type.identity.fullyQualifiedName();
        entry.documentation = documentationAt(type.fileId, type.range, type.isPrecompiled);
        result_.entries.push_back(std::move(entry));
    }
}

void MemberCompletionResolver::appendTypeMembers()
{
    const frontend::ProjectInheritanceModel* inheritance = compilation_.inheritanceModel();
    if (compilation_.nameLookup() == nullptr || compilation_.memberModel() == nullptr ||
        inheritance == nullptr || compilation_.interfaceModel() == nullptr ||
        compilation_.namespaceFunctionModel() == nullptr)
    {
        enumerationComplete_ = false;
        return;
    }
    if (!receiverTypeOrdinal_.has_value())
    {
        const bool arrayReceiver =
            frontend::ProjectTypeConversion::isNativeArray(receiverTypeKey_);
        const bool fixedArrayReceiver =
            frontend::ProjectMemberModel::fixedArrayCount(receiverTypeKey_).has_value();
        if (!staticReceiver_ && (arrayReceiver || fixedArrayReceiver))
            appendIntrinsicEntry(
                "Length", MemberCompletionEntryKind::Property, "int");
        appendStringFacadeMembers();
        appendIteratorFacadeMembers();
        appendExtensionMembers();
        const bool supported = arrayReceiver || fixedArrayReceiver ||
            frontend::ProjectTypeConversion::isNativeString(receiverTypeKey_) ||
            receiverTypeKey_ == "string-literal" || receiverTypeKey_ == "builtin:bool" ||
            frontend::ProjectTypeConversion::isNumeric(receiverTypeKey_) ||
            language::iteratorInnerTypeKey(receiverTypeKey_).has_value() ||
            language::asyncIteratorInnerTypeKey(receiverTypeKey_).has_value();
        enumerationComplete_ = supported;
        return;
    }
    std::optional<std::size_t> current = receiverTypeOrdinal_;
    while (current.has_value())
    {
        appendMemberLevel(*current);
        if (cancellationRequested()) return;
        current = inheritance != nullptr ? inheritance->baseTypeOrdinal(*current) : std::nullopt;
    }
    appendInterfaceMembers(*receiverTypeOrdinal_);
    appendDefaultInterfaceMembers(*receiverTypeOrdinal_);
    appendSynthesizedRootMembers();
    appendNestedTypes();
    appendExtensionMembers();
}

void MemberCompletionResolver::appendNamespaceMembers()
{
    const frontend::ProjectNameLookup* names = compilation_.nameLookup();
    if (names == nullptr || compilation_.inheritanceModel() == nullptr ||
        compilation_.namespaceFunctionModel() == nullptr ||
        accessingProjectOrdinal_ >= names->modules().size() ||
        names->modules()[accessingProjectOrdinal_] == nullptr)
    {
        enumerationComplete_ = false;
        return;
    }
    const std::unordered_set<std::size_t> projects = visibleProjects();
    appendNamespaceTreeMembers(projects, namespaceSegments());
    appendNamespaceFunctions(projects);
    appendNamespaceStates(projects);
}

std::unordered_set<std::size_t> MemberCompletionResolver::visibleProjects() const
{
    std::unordered_set<std::size_t> result;
    const frontend::ProjectNameLookup* names = compilation_.nameLookup();
    if (names == nullptr || accessingProjectOrdinal_ >= names->modules().size()) return result;
    if (names->modules()[accessingProjectOrdinal_] == nullptr) return result;
    const frontend::ModuleDeclarationSpace& sourceModule =
        *names->modules()[accessingProjectOrdinal_];
    result.insert(accessingProjectOrdinal_);
    result.insert(
        sourceModule.visibleProjectOrdinals().begin(),
        sourceModule.visibleProjectOrdinals().end());
    return result;
}

std::vector<std::string> MemberCompletionResolver::namespaceSegments() const
{
    std::vector<std::string> segments;
    std::size_t segmentBegin = 0;
    while (segmentBegin <= namespaceName_.size())
    {
        const std::size_t end = namespaceName_.find('.', segmentBegin);
        segments.push_back(namespaceName_.substr(segmentBegin, end - segmentBegin));
        if (end == std::string::npos) break;
        segmentBegin = end + 1;
    }
    return segments;
}

void MemberCompletionResolver::appendNamespaceTreeMembers(
    const std::unordered_set<std::size_t>& projects,
    const std::vector<std::string>& segments)
{
    const frontend::ProjectNameLookup* names = compilation_.nameLookup();
    if (names == nullptr) return;
    for (const auto& module : names->modules())
    {
        if (cancellationRequested()) return;
        if (module == nullptr || !projects.contains(module->projectOrdinal())) continue;
        const frontend::NamespaceSymbol* symbol =
            module->globalNamespace().findNamespace(segments);
        if (symbol == nullptr) continue;
        for (const auto& child : symbol->children())
        {
            if (cancellationRequested()) return;
            if (child == nullptr || !prefixMatches(child->name())) continue;
            const std::string identity = "namespace\n" + std::string(child->name());
            if (entryByIdentity_.contains(identity)) continue;
            entryByIdentity_.emplace(identity, result_.entries.size());
            result_.entries.push_back(MemberCompletionEntry{
                std::string(child->name()), std::string(child->name()),
                MemberCompletionEntryKind::Namespace, true, namespaceName_,
                std::string(child->fullName()), {}, {}});
        }
        for (const std::size_t ordinal : symbol->typeOrdinals())
        {
            if (cancellationRequested()) return;
            if (ordinal >= names->namedTypes().size()) continue;
            const frontend::NamedTypeDeclaration& type = names->namedTypes()[ordinal];
            if (!prefixMatches(type.identity.name())) continue;
            if (!typeIsAccessible(ordinal)) continue;
            const std::string identity = "type\n" + type.identity.fullyQualifiedName();
            if (entryByIdentity_.contains(identity)) continue;
            entryByIdentity_.emplace(identity, result_.entries.size());
            MemberCompletionEntry entry;
            entry.insertText = type.identity.name();
            entry.name = type.identity.name();
            entry.kind = MemberCompletionEntryKind::Type;
            entry.isStatic = true;
            entry.declaringType = namespaceName_;
            entry.resultType = type.identity.fullyQualifiedName();
            entry.documentation = documentationAt(type.fileId, type.range, type.isPrecompiled);
            result_.entries.push_back(std::move(entry));
        }
    }
}

void MemberCompletionResolver::appendNamespaceFunctions(
    const std::unordered_set<std::size_t>& projects)
{
    const frontend::ProjectNamespaceFunctionModel* functions =
        compilation_.namespaceFunctionModel();
    if (functions == nullptr) return;
    for (const frontend::NamespaceFunctionSymbol& function : functions->functions())
    {
        if (cancellationRequested()) return;
        if (function.namespaceName != namespaceName_ || !prefixMatches(function.name) ||
            !projects.contains(function.projectOrdinal)) continue;
        if (function.accessibility != frontend::NamespaceFunctionAccessibility::Public &&
            function.projectOrdinal != accessingProjectOrdinal_) continue;
        const std::string identity = "function\n" + function.name;
        auto found = entryByIdentity_.find(identity);
        if (found == entryByIdentity_.end())
        {
            MemberCompletionEntry entry;
            entry.insertText = function.name;
            entry.name = function.name;
            entry.kind = MemberCompletionEntryKind::Function;
            entry.isStatic = true;
            entry.declaringType = namespaceName_;
            entry.resultType = readableNamespaceResult(function);
            entry.documentation =
                documentationAt(function.fileId, function.range, function.isPrecompiled);
            const std::size_t index = result_.entries.size();
            result_.entries.push_back(std::move(entry));
            found = entryByIdentity_.emplace(identity, index).first;
        }
        MemberCompletionEntry& entry = result_.entries[found->second];
        MemberCompletionOverload overload = makeNamespaceOverload(function);
        if (std::ranges::none_of(entry.overloads, [&](const MemberCompletionOverload& existing) {
                return existing.signature == overload.signature;
            })) entry.overloads.push_back(std::move(overload));
    }
}

void MemberCompletionResolver::appendNamespaceStates(
    const std::unordered_set<std::size_t>& projects)
{
    const frontend::ProjectNamespaceFunctionModel* functions =
        compilation_.namespaceFunctionModel();
    if (functions == nullptr) return;
    for (const frontend::NamespaceStateSymbol& state : functions->states())
    {
        if (cancellationRequested()) return;
        if (state.namespaceName != namespaceName_ || !prefixMatches(state.name) ||
            !projects.contains(state.projectOrdinal)) continue;
        if (state.accessibility != frontend::NamespaceFunctionAccessibility::Public &&
            state.projectOrdinal != accessingProjectOrdinal_) continue;
        MemberCompletionEntry entry;
        entry.insertText = state.name;
        entry.name = state.name;
        entry.kind = state.isConst ? MemberCompletionEntryKind::Constant
                                   : MemberCompletionEntryKind::Variable;
        entry.isStatic = true;
        entry.declaringType = namespaceName_;
        entry.resultType = readableType(state.declaredTypeSyntax, state.declaredTypeKey);
        entry.documentation = documentationAt(state.fileId, state.range, state.isPrecompiled);
        result_.entries.push_back(std::move(entry));
    }
}

void MemberCompletionResolver::sortResult()
{
    for (MemberCompletionEntry& entry : result_.entries)
    {
        std::ranges::sort(entry.overloads, [](const MemberCompletionOverload& left,
                                             const MemberCompletionOverload& right) {
            return left.signature < right.signature;
        });
    }
    std::ranges::sort(result_.entries, [](const MemberCompletionEntry& left,
                                         const MemberCompletionEntry& right) {
        if (left.name != right.name) return left.name < right.name;
        if (left.kind != right.kind)
            return static_cast<int>(left.kind) < static_cast<int>(right.kind);
        return left.declaringType < right.declaringType;
    });
}

MemberCompletionResult MemberCompletionResolver::resolve()
{
    if (cancellationRequested())
    {
        result_.status = MemberCompletionStatus::Cancelled;
        return result_;
    }
    if (!locateDocument())
    {
        result_.status = cancellationRequested()
            ? MemberCompletionStatus::Cancelled
            : MemberCompletionStatus::Unavailable;
        return result_;
    }
    if (!locateOperator())
    {
        result_.status = cancellationRequested()
            ? MemberCompletionStatus::Cancelled
            : MemberCompletionStatus::InvalidPosition;
        return result_;
    }
    if (!locateReceiver())
    {
        result_.status = cancellationRequested()
            ? MemberCompletionStatus::Cancelled
            : MemberCompletionStatus::Incomplete;
        return result_;
    }
    if (namespaceReceiver_) appendNamespaceMembers();
    else appendTypeMembers();
    if (cancellationRequested())
    {
        result_.entries.clear();
        result_.status = MemberCompletionStatus::Cancelled;
        return result_;
    }
    sortResult();
    if (cancellationRequested())
    {
        result_.entries.clear();
        result_.status = MemberCompletionStatus::Cancelled;
        return result_;
    }
    result_.status = enumerationComplete_
        ? MemberCompletionStatus::Complete
        : MemberCompletionStatus::Incomplete;
    return result_;
}
}
