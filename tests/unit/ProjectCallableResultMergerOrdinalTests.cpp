#include "nativesharp/Frontend/ProjectCompilation.h"
#include "nativesharp/Frontend/ProjectMethodBodyModel.h"
#include "../../src/Frontend/Bodies/Support/NameBindingContextConflicts.h"
#include "../../src/Frontend/Bodies/Support/NameBindingCrossShardConflicts.h"
#include "../../src/Frontend/Bodies/Support/FnOnceCreationCandidateStorage.h"

#include "../support/TestContext.h"

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <set>
#include <string>
#include <string_view>
#include <unordered_set>
#include <vector>

namespace
{
void writeFile(const std::filesystem::path& path, std::string_view text)
{
    std::filesystem::create_directories(path.parent_path());
    std::ofstream stream(path, std::ios::binary);
    stream << text;
}
}

int main()
{
    using nativesharp::frontend::ProjectCompilation;
    using nativesharp::tests::TestContext;

    TestContext tests;
    {
        using nativesharp::frontend::ViewUnknownOrigin;
        using nativesharp::frontend::appendViewUnknownOrigin;
        std::vector<ViewUnknownOrigin> origins;
        for (std::size_t i = 0; i < 8; ++i) appendViewUnknownOrigin(origins, {i, i});
        appendViewUnknownOrigin(origins, {7, 7});
        tests.expect(origins.size() == 8, "duplicate uncertainty observations do not consume the bound");
        appendViewUnknownOrigin(origins, {8, 8});
        appendViewUnknownOrigin(origins, {9, 9});
        tests.expect(origins.size() == 1 && !origins.front().producerScopeOrdinal.has_value() &&
                         !origins.front().observedLocalCount.has_value(),
            "too many uncertainty observations collapse to unlocated Unknown");
    }
    std::vector<nativesharp::frontend::FnOnceCreationCandidate> boundedCandidates;
    bool reached = false;
    for (std::size_t i = 0; i < 3; ++i)
    {
        nativesharp::frontend::FnOnceCreationCandidate candidate;
        candidate.localOrdinal = i;
        nativesharp::frontend::detail::appendFnOnceCreationCandidate(boundedCandidates, reached, candidate, 2);
    }
    tests.expect(reached && boundedCandidates.size() == 2 && boundedCandidates[0].localOrdinal == 0 &&
        boundedCandidates[1].localOrdinal == 1, "candidate cap truncates and preserves earlier entries");
    bool mergedLimit = false;
    nativesharp::frontend::detail::mergeFnOnceCandidateLimit(mergedLimit, false);
    tests.expect(!mergedLimit, "unexhausted shard preserves false limit state");
    nativesharp::frontend::detail::mergeFnOnceCandidateLimit(mergedLimit, reached);
    nativesharp::frontend::detail::mergeFnOnceCandidateLimit(mergedLimit, false);
    tests.expect(mergedLimit, "candidate limit is sticky across merged shards");
    const std::filesystem::path root =
        std::filesystem::temp_directory_path() / "nativesharp_callable_result_merger_ordinals";
    std::error_code cleanupError;
    std::filesystem::remove_all(root, cleanupError);
    std::filesystem::create_directories(root);
    writeFile(
        root / "OrdinalMerge.nsproj",
        "<Project><PropertyGroup><Name>OrdinalMerge</Name></PropertyGroup>"
        "<ItemGroup><Compile Include=\"**/*.ns\" /></ItemGroup></Project>");
    writeFile(
        root / "Main.ns",
        "class OrdinalMerge { "
        "public static int First() { int first = 1; return first; } "
        "public static int Second() { int second = 2; return second; } "
        "public static int Third() { int third = 3; { int nested = 4; "
        "return third + nested; } } }\n");

    ProjectCompilation compilation;
    compilation.setCompilationJobs(1);
    std::string error;
    const bool compiled =
        compilation.compileProjectMethodBodies(root / "OrdinalMerge.nsproj", error);
    tests.expect(compiled, "multiple isolated callable shards compile successfully");
    tests.expect(error.empty(), "callable shard compilation reports no execution error");
    tests.expect(compilation.methodBodyModel() != nullptr,
        "callable shard compilation publishes a method-body model");

    if (compilation.methodBodyModel() != nullptr)
    {
        const auto& model = *compilation.methodBodyModel();
        tests.expect(!model.nameBindings().empty(), "model provides syntax for conflict controls");
        if (!model.nameBindings().empty())
        {
            auto first = model.nameBindings().front();
            first.analysisContextRecorded = true;
            first.recordedLambdaOrdinal = 2;
            auto second = first;
            std::vector<nativesharp::frontend::MethodNameBinding> observations{first, second};
            std::vector<unsigned char> eligible{1, 1};
            auto conflicts = nativesharp::frontend::detail::nameBindingContextConflicts(observations, eligible);
            tests.expect(!conflicts[0] && !conflicts[1], "same site and matching contexts remain compatible");
            observations[1].recordedLambdaOrdinal = 3;
            conflicts = nativesharp::frontend::detail::nameBindingContextConflicts(observations, eligible);
            tests.expect(conflicts[0] && conflicts[1], "different lambda contexts invalidate both visits");
            observations[1].recordedLambdaOrdinal.reset();
            conflicts = nativesharp::frontend::detail::nameBindingContextConflicts(observations, eligible);
            tests.expect(conflicts[0] && conflicts[1], "top-level and lambda visits invalidate both contexts");
            observations[1] = first;
            observations[1].analysisContextRecorded = false;
            conflicts = nativesharp::frontend::detail::nameBindingContextConflicts(observations, eligible);
            tests.expect(conflicts[0] && conflicts[1], "missing context invalidates both visits");
            observations[1] = first;
            eligible[1] = 0;
            conflicts = nativesharp::frontend::detail::nameBindingContextConflicts(observations, eligible);
            tests.expect(conflicts[0] && conflicts[1], "invalid visit invalidates matching canonical visit");
            eligible[1] = 1;
            observations[1].scopeOrdinal++;
            conflicts = nativesharp::frontend::detail::nameBindingContextConflicts(observations, eligible);
            tests.expect(conflicts[0] && conflicts[1], "different scopes invalidate both visits");
            observations[1] = first;
            observations[1].methodMemberOrdinal++;
            observations[1].recordedLambdaOrdinal = 3;
            conflicts = nativesharp::frontend::detail::nameBindingContextConflicts(observations, eligible);
            tests.expect(!conflicts[0] && !conflicts[1], "different callable identities do not conflict");
            observations[1] = first;
            observations[1].analysisContext = nativesharp::frontend::MethodNameBindingContext::Lambda;
            std::vector<nativesharp::frontend::MethodNameBinding> oldShard{first, first};
            auto unrelated = observations[1];
            unrelated.methodMemberOrdinal++;
            std::vector<nativesharp::frontend::MethodNameBinding> newShard{observations[1], unrelated};
            const auto* syntax = newShard[0].syntax;
            nativesharp::frontend::ProjectCallableBindingIndex canonicalIndex;
            canonicalIndex.record(first.methodMemberOrdinal, syntax, 0);
            canonicalIndex.record(first.methodMemberOrdinal, syntax, 1);
            tests.expect(canonicalIndex.latest(syntax, first.methodMemberOrdinal) == 1,
                "canonical precheck sees newest binding before cross-shard invalidation");
            nativesharp::frontend::detail::invalidateCrossShardNameContexts(oldShard, newShard);
            canonicalIndex.record(first.methodMemberOrdinal, syntax, oldShard.size());
            tests.expect(canonicalIndex.latest(syntax, first.methodMemberOrdinal) == oldShard.size(),
                "metadata invalidation preserves newest-binding-wins when next shard is indexed");
            tests.expect(oldShard[0].analysisContext == nativesharp::frontend::MethodNameBindingContext::Unknown &&
                oldShard[1].analysisContext == nativesharp::frontend::MethodNameBindingContext::Unknown &&
                newShard[0].analysisContext == nativesharp::frontend::MethodNameBindingContext::Unknown &&
                !oldShard[0].analysisContextRecorded && !newShard[0].analysisContextRecorded &&
                newShard[0].syntax == syntax && newShard[1].analysisContextRecorded,
                "cross-shard duplicates invalidate metadata without altering unrelated owners or syntax");
        }
        tests.expect(model.scopes().size() >= 4 && model.locals().size() == 4,
            "fixture exercises several global scope and local bases");
        bool localsPointToOwningScopes = true;
        for (const auto& local : model.locals())
        {
            if (local.scopeOrdinal >= model.scopes().size() ||
                model.scopes()[local.scopeOrdinal].methodMemberOrdinal != local.methodMemberOrdinal)
            {
                localsPointToOwningScopes = false;
                break;
            }
        }
        tests.expect(localsPointToOwningScopes,
            "each merged local keeps the exact global ordinal of its owning scope");

        bool scopesPointToOwnedLocals = true;
        for (std::size_t scopeOrdinal = 0; scopeOrdinal < model.scopes().size(); ++scopeOrdinal)
        {
            const auto& scope = model.scopes()[scopeOrdinal];
            for (const std::size_t localOrdinal : scope.localOrdinals)
            {
                if (localOrdinal >= model.locals().size() ||
                    model.locals()[localOrdinal].scopeOrdinal != scopeOrdinal)
                {
                    scopesPointToOwnedLocals = false;
                    break;
                }
            }
            if (!scopesPointToOwnedLocals) break;
        }
        tests.expect(scopesPointToOwnedLocals,
            "each merged scope and local preserve their bidirectional ordinal invariant");
    }

    // Inspect provenance after callable shards have been merged, not merely
    // the result of analyzing each callable in isolation.
    writeFile(root / "Main.ns",
        "public borrowed struct View<lifetime a> { "
        "private borrowed[a] int* target; "
        "public View(ref[a] int first, ref[a] int second) { unsafe(\"view\") { target = &second; } } "
        "public ref[this] int Get() { unsafe(\"view\") { return ref *target; } } } "
        "public struct Owner { public int Value; ~Owner() { } } "
        "class IdentityMerge { "
        "public static void Write(View input, ref View output) { output = input; } "
        "public static void WriteOut(View input, out View output) { output = input; } "
        "public static int First(ref int parameter) { Owner owner; owner.Value = 1; "
        "View view = new View(ref owner.Value, ref parameter); View copy = view; "
        "View assigned = default(View); assigned = copy; "
        "View output = default(View); Write(copy, ref output); "
        "{ ref View indirect = ref output; Write(copy, ref indirect); } "
        "View output2 = default(View); WriteOut(copy, out output2); return output.Get(); } "
        "public static int Second(ref int parameter) { Owner owner; owner.Value = 2; "
        "View view = new View(ref owner.Value, ref parameter); View copy = view; "
        "View assigned = default(View); assigned = copy; "
        "View output = default(View); Write(copy, ref output); "
        "{ ref View indirect = ref output; Write(copy, ref indirect); } "
        "View output2 = default(View); WriteOut(copy, out output2); return output.Get(); } "
        "public static int Shadow(ref int parameter) { Owner owner; owner.Value = 3; "
        "View held = new View(ref owner.Value, ref parameter); View dest = default(View); "
        "{ Owner owner; owner.Value = 4; int observed = owner.Value; } "
        "Write(held, ref dest); WriteOut(held, out dest); "
        "bool choose = true; return choose ? held.Get() : held.Get(); } "
        "public static int Many(ref int parameter) { Owner owner; owner.Value = 3; "
        "View many = new View(ref owner.Value, ref parameter); "
        "{ many = new View(ref owner.Value, ref parameter); } "
        "{ many = new View(ref owner.Value, ref parameter); } "
        "{ many = new View(ref owner.Value, ref parameter); } "
        "{ many = new View(ref owner.Value, ref parameter); } "
        "{ many = new View(ref owner.Value, ref parameter); } "
        "{ many = new View(ref owner.Value, ref parameter); } "
        "{ many = new View(ref owner.Value, ref parameter); } "
        "{ many = new View(ref owner.Value, ref parameter); } "
        "{ many = new View(ref owner.Value, ref parameter); } "
        "View copiedMany = many; View outputMany = default(View); "
        "Write(copiedMany, ref outputMany); return outputMany.Get(); } }\n");
    ProjectCompilation identityCompilation;
    identityCompilation.setCompilationJobs(1);
    error.clear();
    const bool identityCompiled = identityCompilation.compileProjectMethodBodies(root / "OrdinalMerge.nsproj", error);
    tests.expect(identityCompiled && error.empty(), "view provenance fixture compiles through callable shards");
    if (identityCompilation.methodBodyModel() != nullptr)
    {
        const auto& locals = identityCompilation.methodBodyModel()->locals();
        std::size_t viewsChecked = 0;
        for (const auto& local : locals)
        {
            if (local.name != "view") continue;
            bool ownerMatches = false;
            bool parameterMatches = false;
            for (const auto& source : local.viewSourceIdentities)
            {
                if (source.name == "owner" && source.kind == nativesharp::frontend::ViewSourceIdentityKind::Local &&
                    source.ordinal < locals.size() && locals[source.ordinal].name == "owner" &&
                    locals[source.ordinal].methodMemberOrdinal == local.methodMemberOrdinal)
                    ownerMatches = true;
                if (source.name == "parameter" &&
                    source.kind == nativesharp::frontend::ViewSourceIdentityKind::Parameter && source.ordinal == 0)
                    parameterMatches = true;
            }
            tests.expect(ownerMatches, "merged view identity refers to its own callable's owner");
            tests.expect(parameterMatches, "merging preserves the callable-relative parameter ordinal");
            bool originsMatch = !local.viewUnknownOrigins.empty();
            for (const auto& origin : local.viewUnknownOrigins)
            {
                if (!origin.producerScopeOrdinal.has_value() || !origin.observedLocalCount.has_value() ||
                    *origin.producerScopeOrdinal >= identityCompilation.methodBodyModel()->scopes().size() ||
                    identityCompilation.methodBodyModel()->scopes()[*origin.producerScopeOrdinal].methodMemberOrdinal !=
                        local.methodMemberOrdinal || *origin.observedLocalCount > locals.size())
                    originsMatch = false;
            }
            tests.expect(originsMatch, "unknown producer observations remain in their callable after merge");
            const nativesharp::frontend::MethodLocalSymbol* copy = nullptr;
            const nativesharp::frontend::MethodLocalSymbol* output = nullptr;
            const nativesharp::frontend::MethodLocalSymbol* output2 = nullptr;
            const nativesharp::frontend::MethodLocalSymbol* assigned = nullptr;
            for (const auto& candidate : locals)
                if (candidate.methodMemberOrdinal == local.methodMemberOrdinal)
                {
                    if (candidate.name == "copy") copy = &candidate;
                    if (candidate.name == "output") output = &candidate;
                    if (candidate.name == "output2") output2 = &candidate;
                    if (candidate.name == "assigned") assigned = &candidate;
                }
            tests.expect(copy != nullptr && copy->viewUnknownOrigins == local.viewUnknownOrigins,
                "view copy preserves the producer observations exactly");
            bool outputPreservesInputs = output != nullptr && output->viewUnknownOrigins.size() >= 3;
            if (output != nullptr)
                for (const auto& origin : local.viewUnknownOrigins)
                    if (std::ranges::find(output->viewUnknownOrigins, origin) == output->viewUnknownOrigins.end())
                        outputPreservesInputs = false;
            tests.expect(outputPreservesInputs,
                "ref output and reference-alias output retain input and call uncertainty");
            bool outPreservesInputs = output2 != nullptr && output2->viewUnknownOrigins.size() >= 2;
            bool assignmentPreservesInputs = assigned != nullptr;
            for (const auto& origin : local.viewUnknownOrigins)
            {
                if (output2 != nullptr && std::ranges::find(output2->viewUnknownOrigins, origin) ==
                    output2->viewUnknownOrigins.end()) outPreservesInputs = false;
                if (assigned != nullptr && std::ranges::find(assigned->viewUnknownOrigins, origin) ==
                    assigned->viewUnknownOrigins.end()) assignmentPreservesInputs = false;
            }
            tests.expect(outPreservesInputs, "out output conservatively retains input and call uncertainty");
            tests.expect(assignmentPreservesInputs, "view assignment retains producer uncertainty");
            ++viewsChecked;
        }
        tests.expect(viewsChecked == 2, "both callable view identities were checked");
        bool collapsed = true;
        std::size_t collapsedChecked = 0;
        for (const auto& local : locals)
        {
            if (local.name != "many" && local.name != "copiedMany" && local.name != "outputMany") continue;
            ++collapsedChecked;
            if (local.viewUnknownOrigins.size() != 1 ||
                local.viewUnknownOrigins.front().producerScopeOrdinal.has_value() ||
                local.viewUnknownOrigins.front().observedLocalCount.has_value()) collapsed = false;
        }
        tests.expect(collapsedChecked == 3 && collapsed,
            "collapsed uncertainty survives shard merge, copying and ref output");

        // Pointer identity is meaningful only while this compilation and its AST live.
        // Unbound syntax is counted as unknown, never treated as a local or proof of safety.
        const auto* members = identityCompilation.memberModel();
        const auto* model = identityCompilation.methodBodyModel();
        const nativesharp::ast::AstNode* body = nullptr;
        std::size_t callable = 0;
        if (members != nullptr)
            for (std::size_t i = 0; i < members->members().size(); ++i)
                if (members->members()[i].name == "Shadow" && members->members()[i].syntax != nullptr)
                {
                    callable = i;
                    body = members->members()[i].syntax->first();
                    break;
                }
        tests.expect(body != nullptr, "shadow fixture exposes its callable AST");
        if (body != nullptr)
        {
            using nativesharp::ast::AstNode;
            using nativesharp::ast::AstNodeKind;
            using nativesharp::frontend::MethodNameBindingKind;
            std::vector<const AstNode*> pending{body};
            std::unordered_set<const AstNode*> visited;
            std::unordered_set<std::size_t> ownerOrdinals;
            std::size_t sourceBindings = 0, outputPassBindings = 0, unknownBindings = 0;
            bool validLocalLinks = true;
            while (!pending.empty())
            {
                const AstNode* node = pending.back();
                pending.pop_back();
                if (node == nullptr || !visited.insert(node).second) continue;
                if (node->kind() == AstNodeKind::IdentifierExpression)
                {
                    const auto* binding = model->nameBinding(node, callable);
                    if (binding == nullptr) ++unknownBindings;
                    else if (binding->kind == MethodNameBindingKind::Local)
                    {
                        if (!binding->localOrdinal.has_value() || *binding->localOrdinal >= locals.size() ||
                            locals[*binding->localOrdinal].methodMemberOrdinal != callable ||
                            locals[*binding->localOrdinal].name != node->text()) validLocalLinks = false;
                        else if (node->text() == "owner") ownerOrdinals.insert(*binding->localOrdinal);
                        else if (node->text() == "held") ++sourceBindings;
                    }
                }
                if (node->kind() == AstNodeKind::UnaryExpression && node->first() != nullptr &&
                    (node->auxiliaryText() == "pass:ref" || node->auxiliaryText() == "pass:out") &&
                    node->first()->kind() == AstNodeKind::IdentifierExpression && node->first()->text() == "dest")
                {
                    const auto* binding = model->nameBinding(node->first(), callable);
                    if (binding != nullptr && binding->kind == MethodNameBindingKind::Local &&
                        binding->localOrdinal.has_value() && *binding->localOrdinal < locals.size() &&
                        locals[*binding->localOrdinal].name == "dest" &&
                        locals[*binding->localOrdinal].methodMemberOrdinal == callable) ++outputPassBindings;
                }
                pending.push_back(node->first());
                pending.push_back(node->second());
                pending.push_back(node->third());
                for (const AstNode* child : node->children()) pending.push_back(child);
            }
            tests.expect(validLocalLinks, "AST local bindings point to locals of the same callable");
            tests.expect(ownerOrdinals.size() == 2, "homonymous owner uses resolve to two local ordinals");
            tests.expect(sourceBindings >= 4, "source bindings include both conditional branches and calls");
            tests.expect(outputPassBindings == 2, "ref and out arguments bind to the destination local");
            (void)unknownBindings; // Unknown nodes carry no completeness claim.
        }
    }

    writeFile(root / "Main.ns",
        "delegate int Read(); class Box { public int Value; public Box(int v) { Value = v; } } "
        "class LambdaMerge { "
        "public static int First() { raii Box first = new Box(21); Read outer = [move first] () => { "
        "raii Box second = new Box(21); Read inner = [move second] () => second.Value; "
        "int value = inner(); delete inner; return value; }; "
        "raii Box third = new Box(1); Read sibling = [move third] () => third.Value; int answer = outer(); return answer; } "
        "public static int Second() { raii Box first = new Box(21); Read outer = [move first] () => { "
        "raii Box second = new Box(21); Read inner = [move second] () => second.Value; "
        "int value = inner(); delete inner; return value; }; "
        "raii Box third = new Box(1); Read sibling = [move third] () => third.Value; int answer = outer(); return answer; } "
        "public static int Probe() { return 7; raii Box dead = new Box(1); "
        "Read ghost = [move dead] () => { raii Box innerDead = new Box(1); "
        "Read hidden = [move innerDead] () => innerDead.Value; int v = hidden(); delete hidden; return v + dead.Value; }; "
        "if (true) { raii Box frameDead = new Box(1); Read frameGhost = [move frameDead] () => frameDead.Value; } } }\n");
    for (const std::size_t jobs : {1U, 4U})
    {
        ProjectCompilation lambdaCompilation;
        lambdaCompilation.setCompilationJobs(jobs);
        error.clear();
        const bool lambdaCompiled = lambdaCompilation.compileProjectMethodBodies(root / "OrdinalMerge.nsproj", error);
        tests.expect(lambdaCompiled && error.empty(), "lambda context fixture compiles across shards");
        const auto* body = lambdaCompilation.methodBodyModel();
        if (!body) { tests.expect(false, "lambda context model exists"); continue; }
        std::size_t nested = 0;
        std::size_t sibling = 0;
        std::size_t outer = 0;
        std::size_t top = 0;
        std::size_t topOuter = 0;
        for (const auto& binding : body->nameBindings())
        {
            if (binding.analysisContext == nativesharp::frontend::MethodNameBindingContext::TopLevel)
            {
                ++top;
                if (binding.name == "outer" && body->nameBinding(binding.syntax, binding.methodMemberOrdinal) == &binding)
                    ++topOuter;
            }
            if (binding.analysisContext != nativesharp::frontend::MethodNameBindingContext::Lambda) continue;
            const nativesharp::frontend::MethodLambdaBinding* matched = nullptr;
            for (const auto& lambda : body->lambdaBindings())
                if (binding.recordedLambdaOrdinal == lambda.lambdaOrdinal &&
                    binding.methodMemberOrdinal == lambda.ownerMemberOrdinal)
                { tests.expect(matched == nullptr, "lambda ordinal is unique"); matched = &lambda; }
            tests.expect(matched != nullptr, "context ordinal points to lambda in same callable");
            if (!matched || body->nameBinding(binding.syntax, binding.methodMemberOrdinal) != &binding) continue;
            if (binding.name == "inner" && !matched->parentLambdaOrdinal) ++outer;
            if (binding.name == "second" && matched->parentLambdaOrdinal) ++nested;
            if (binding.name == "third" && !matched->parentLambdaOrdinal) ++sibling;
        }
        tests.expect(top > 0 && topOuter >= 2 && outer >= 2 && nested >= 2 && sibling >= 2,
            "canonical bindings retain top-level, outer, nested and sibling lambda contexts in both callables");
        std::size_t candidates = 0;
        std::size_t nestedCandidates = 0;
        std::size_t probeCandidates = 0;
        std::size_t nestedProbeUnknown = 0;
        std::size_t frameProbeUnknown = 0;
        std::set<std::size_t> innerOwners;
        std::set<std::size_t> siblingOwners;
        for (const auto& candidate : body->fnOnceCreationCandidates())
        {
            tests.expect(candidate.status == nativesharp::frontend::FnOnceCandidateStatus::Unknown,
                "creation site is always Unknown, including probes");
            const bool correct = candidate.localOrdinal < body->locals().size() &&
                candidate.scopeOrdinal < body->scopes().size() && candidate.declarationSyntax &&
                candidate.initializerSyntax == candidate.declarationSyntax->first() &&
                body->locals()[candidate.localOrdinal].syntax == candidate.declarationSyntax &&
                body->locals()[candidate.localOrdinal].scopeOrdinal == candidate.scopeOrdinal &&
                body->locals()[candidate.localOrdinal].methodMemberOrdinal == candidate.methodMemberOrdinal &&
                body->locals()[candidate.localOrdinal].lambdaOrdinal == candidate.parentLambdaOrdinal;
            tests.expect(correct, "candidate local, scope and parent-lambda ordinals survive shard merge");
            if (candidate.parentLambdaOrdinal) ++nestedCandidates;
            if (candidate.declarationSyntax && candidate.declarationSyntax->text() == "ghost" &&
                candidate.visit == nativesharp::frontend::FnOnceCandidateVisit::DuringDiscardedStatementProbe) ++probeCandidates;
            if (candidate.declarationSyntax && candidate.declarationSyntax->text() == "hidden" &&
                candidate.visit == nativesharp::frontend::FnOnceCandidateVisit::Unknown) ++nestedProbeUnknown;
            if (candidate.declarationSyntax && candidate.declarationSyntax->text() == "frameGhost" &&
                candidate.visit == nativesharp::frontend::FnOnceCandidateVisit::Unknown) ++frameProbeUnknown;
            if (candidate.declarationSyntax && candidate.declarationSyntax->text() == "inner")
                innerOwners.insert(candidate.methodMemberOrdinal);
            if (candidate.declarationSyntax && candidate.declarationSyntax->text() == "sibling")
                siblingOwners.insert(candidate.methodMemberOrdinal);
            ++candidates;
        }
        tests.expect(candidates == 7 && nestedCandidates == 3 && probeCandidates == 1 &&
            nestedProbeUnknown == 1 && frameProbeUnknown == 1 &&
            innerOwners.size() == 2 && innerOwners == siblingOwners &&
            !body->fnOnceCreationCandidateLimitReached(),
            "normal, nested and unreachable-probe sites remain Unknown under bounded collection");
    }
    writeFile(root / "Main.ns", "class ErrorMerge { public static int Broken() { "
        "int known = 1; int used = known; return missing; } }\n");
    ProjectCompilation errorCompilation;
    errorCompilation.setCompilationJobs(1);
    error.clear();
    (void)errorCompilation.compileProjectMethodBodies(root / "OrdinalMerge.nsproj", error);
    tests.expect(errorCompilation.methodBodyModel() != nullptr, "error fixture exposes method-body model");
    if (const auto* errorBody = errorCompilation.methodBodyModel())
    {
        bool checked = false;
        for (const auto& binding : errorBody->nameBindings())
            if (binding.name == "known")
            {
                checked = true;
                tests.expect(binding.analysisContext == nativesharp::frontend::MethodNameBindingContext::Unknown,
                    "failed callable retains unknown binding context");
            }
        tests.expect(checked, "error fixture includes a recorded binding");
    }
    std::filesystem::remove_all(root, cleanupError);
    return tests.result();
}
