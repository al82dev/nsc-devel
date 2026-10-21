#pragma once

#include "nativesharp/Basic/FileId.h"
#include "nativesharp/Basic/SourceRange.h"
#include "nativesharp/Frontend/ProjectCompilation.h"
#include "nativesharp/Frontend/ProjectIntegerConstant.h"
#include "nativesharp/LanguageService/MemberCompletionRequest.h"
#include "nativesharp/LanguageService/MemberCompletionResult.h"

#include <cstddef>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace nativesharp::ast
{
class AstNode;
class TypeSyntax;
}

namespace nativesharp::frontend
{
struct ClassMemberSymbol;
struct MethodParameterSymbol;
struct MethodExpressionTypeBinding;
struct NamespaceCompletionScope;
struct NamespaceFunctionSymbol;
struct ProjectImplicitConversion;
class ProjectSyntaxUnit;
}

namespace nativesharp::source
{
class SourceFile;
}

namespace nativesharp::language_service
{
class MemberCompletionResolver final
{
public:
    MemberCompletionResolver(
        const frontend::ProjectCompilation& compilation,
        const MemberCompletionRequest& request);

    MemberCompletionResult resolve();

private:
    bool cancellationRequested() const;
    bool locateDocument();
    bool locateOperator();
    bool locateReceiver();
    bool locateValueReceiver();
    bool locateTypeReceiver();
    bool locateNamespaceReceiver();
    void determineAccessContext(const frontend::MethodExpressionTypeBinding* binding);
    void appendTypeMembers();
    void appendMemberLevel(std::size_t typeOrdinal);
    void appendInterfaceMembers(std::size_t typeOrdinal);
    void appendDefaultInterfaceMembers(std::size_t typeOrdinal);
    void appendMember(std::size_t memberOrdinal);
    void appendProjectedMethod(
        const frontend::ClassMemberSymbol& member,
        std::size_t firstParameter,
        bool isStatic);
    void appendIntrinsicEntry(
        std::string name,
        MemberCompletionEntryKind kind,
        std::string resultType,
        std::string signature = {});
    void appendStringFacadeMembers();
    void appendIteratorFacadeMembers();
    void appendSynthesizedRootMembers();
    void appendExtensionMembers();
    bool extensionMatchesReceiver(
        const frontend::NamespaceFunctionSymbol& function) const;
    frontend::ProjectImplicitConversion classifyReceiverConversion(
        std::string_view targetTypeKey) const;
    bool typeIsAccessible(std::size_t typeOrdinal) const;
    const frontend::NamespaceCompletionScope* completionScope() const;
    std::vector<std::string> visibleExtensionNamespaces() const;
    std::optional<std::string> resolveNamespaceName(
        std::string written) const;
    bool namespaceExists(std::string_view namespaceName) const;
    void appendNestedTypes();
    void appendNamespaceMembers();
    void appendNamespaceTreeMembers(
        const std::unordered_set<std::size_t>& visibleProjects,
        const std::vector<std::string>& segments);
    void appendNamespaceFunctions(
        const std::unordered_set<std::size_t>& visibleProjects);
    void appendNamespaceStates(
        const std::unordered_set<std::size_t>& visibleProjects);
    std::unordered_set<std::size_t> visibleProjects() const;
    std::vector<std::string> namespaceSegments() const;
    bool memberIsAccessible(const frontend::ClassMemberSymbol& member) const;
    bool memberMatchesReceiver(const frontend::ClassMemberSymbol& member) const;
    bool prefixMatches(std::string_view name) const;
    std::string memberName(const frontend::ClassMemberSymbol& member) const;
    std::string readableType(
        const ast::TypeSyntax* syntax,
        std::string_view typeKey) const;
    std::string readableTypeKey(std::string_view typeKey) const;
    std::string readableParameter(
        const frontend::MethodParameterSymbol& parameter) const;
    std::string readableMemberResult(
        const frontend::ClassMemberSymbol& member) const;
    std::string readableNamespaceResult(
        const frontend::NamespaceFunctionSymbol& function) const;
    void appendCallableGenericParameters(
        const ast::AstNode* syntax,
        MemberCompletionOverload& overload) const;
    void appendCallableConstraints(
        const ast::AstNode* syntax,
        MemberCompletionOverload& overload) const;
    void appendCallableGenericSurface(
        const ast::AstNode* syntax,
        MemberCompletionOverload& overload) const;
    MemberCompletionOverload makeOverload(
        const frontend::ClassMemberSymbol& member,
        std::string_view visibleName) const;
    MemberCompletionOverload makeProjectedOverload(
        const frontend::ClassMemberSymbol& member,
        std::string_view visibleName,
        std::size_t firstParameter) const;
    MemberCompletionOverload makeNamespaceOverload(
        const frontend::NamespaceFunctionSymbol& function,
        std::size_t firstParameter = 0) const;
    std::optional<std::string> documentationFor(
        const frontend::ClassMemberSymbol& member) const;
    std::optional<std::string> documentationAt(
        FileId fileId,
        SourceRange range,
        bool precompiled) const;
    void appendModifiers(
        const frontend::ClassMemberSymbol& member,
        MemberCompletionOverload& overload) const;
    void sortResult();

    const frontend::ProjectCompilation& compilation_;
    const MemberCompletionRequest& request_;
    const frontend::ProjectSyntaxUnit* unit_ = nullptr;
    const source::SourceFile* file_ = nullptr;
    std::size_t operatorOffset_ = 0;
    std::size_t receiverEndOffset_ = 0;
    std::size_t caretOffset_ = 0;
    std::size_t accessingProjectOrdinal_ = 0;
    std::optional<std::size_t> accessingTypeOrdinal_;
    std::optional<std::size_t> receiverTypeOrdinal_;
    std::string receiverTypeKey_;
    std::optional<frontend::ProjectIntegerConstant> receiverIntegerConstant_;
    std::string namespaceName_;
    std::string effectivePrefix_;
    bool receiverAddressable_ = false;
    bool receiverReadonly_ = false;
    bool baseReceiver_ = false;
    bool staticReceiver_ = false;
    bool namespaceReceiver_ = false;
    bool enumerationComplete_ = true;
    std::unordered_set<std::string> hiddenNames_;
    std::unordered_set<std::string> synthesizedRootBlockedNames_;
    std::unordered_set<std::string> extensionBlockedNames_;
    std::unordered_map<std::string, std::size_t> entryByIdentity_;
    MemberCompletionResult result_;
};
}
