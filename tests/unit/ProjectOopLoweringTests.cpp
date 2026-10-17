#include "nativesharp/Frontend/ProjectCompilation.h"
#include "nativesharp/Frontend/ProjectOopHirModel.h"
#include "nativesharp/Frontend/ProjectOopHirModelDumper.h"
#include "nativesharp/Frontend/ProjectOopMirModel.h"
#include "../../src/Frontend/HIR/HirLoweringSupport.h"
#include "nativesharp/Frontend/ProjectOopMirModelDumper.h"
#include "nativesharp/Diagnostics/DiagnosticEngine.h"
#include "nativesharp/LLVM/CoffObjectVerifier.h"
#include "nativesharp/LLVM/CompilationArtifactCache.h"
#include "nativesharp/LLVM/LlvmModule.h"
#include "nativesharp/LLVM/LlvmOptimizationLevel.h"
#include "nativesharp/LLVM/LlvmOptimizer.h"
#include "nativesharp/LLVM/LlvmTargetMachine.h"
#include "nativesharp/LLVM/ProjectOopCodePartition.h"
#include "nativesharp/LLVM/ProjectOopLlvmLowerer.h"
#include "nativesharp/LLVM/ProjectOopObjectGenerator.h"
#include "nativesharp/Linker/WindowsLibraryBuilder.h"

#include "../support/TestContext.h"

#include <algorithm>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <memory>
#include <sstream>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace nativesharp::frontend
{
struct ProjectOopHirModelTestAccess final
{
    static bool configureAggregateTypes(
        ProjectNameLookup& nameLookup,
        ProjectInheritanceModel& inheritanceModel,
        std::size_t depth,
        std::vector<std::string>& typeKeys)
    {
        typeKeys.reserve(depth);
        nameLookup.namedTypes_.reserve(depth);
        for (std::size_t ordinal = 0; ordinal < depth; ++ordinal)
        {
            NamedTypeDeclaration declaration{
                compilation::TypeIdentity(
                    "AggregateDepth", {}, {}, "S" + std::to_string(ordinal)),
                NamedTypeDeclarationKind::Struct,
                NamedTypeAccessibility::Internal,
                std::nullopt,
                false,
                false,
                false,
                false,
                false,
                0U,
                0U,
                FileId{},
                SourceRange{},
                nullptr,
                {}};
            nameLookup.namedTypes_.push_back(std::move(declaration));
            typeKeys.push_back(
                "type:" + nameLookup.namedTypes_.back().identity.key());
        }
        nameLookup.built_ = true;

        diagnostics::DiagnosticEngine diagnostics;
        return inheritanceModel.build(nameLookup, diagnostics);
    }

    static void configureAggregateLayouts(
        ProjectObjectLayoutModel& objectLayoutModel,
        const std::vector<std::string>& typeKeys)
    {
        const std::size_t depth = typeKeys.size();
        objectLayoutModel.structLayouts_.resize(depth);
        objectLayoutModel.structLayoutIndexByType_.resize(depth);
        std::uint64_t childSize = 4U;
        std::uint64_t childAlignment = 4U;
        for (std::size_t remaining = depth; remaining > 0; --remaining)
        {
            const std::size_t ordinal = remaining - 1U;
            StructValueLayout& layout = objectLayoutModel.structLayouts_[ordinal];
            layout.typeOrdinal = ordinal;
            layout.typeKey = typeKeys[ordinal];
            layout.alignment = 8U;
            layout.minimumAlignment = 8U;
            layout.hasExplicitFieldOffsets = true;

            ObjectFieldLayout field;
            field.memberOrdinal = ordinal;
            field.offset = 8U;
            field.size = childSize;
            field.alignment = childAlignment;
            field.typeKey = ordinal + 1U < depth
                ? typeKeys[ordinal + 1U]
                : "builtin:int";
            layout.fields.push_back(std::move(field));
            layout.size = (8U + childSize + 7U) & ~std::uint64_t{7U};
            childSize = layout.size;
            childAlignment = layout.alignment;
            objectLayoutModel.structLayoutIndexByType_[ordinal] = ordinal;
        }
        objectLayoutModel.built_ = true;
    }

    static bool configureAggregateLeafChain(
        ProjectOopHirModel& model,
        ProjectNameLookup& nameLookup,
        ProjectInheritanceModel& inheritanceModel,
        ProjectObjectLayoutModel& objectLayoutModel,
        std::size_t depth,
        std::vector<std::string>& typeKeys)
    {
        if (!configureAggregateTypes(nameLookup, inheritanceModel, depth, typeKeys))
        {
            return false;
        }
        configureAggregateLayouts(objectLayoutModel, typeKeys);

        model.nameLookup_ = &nameLookup;
        model.inheritanceModel_ = &inheritanceModel;
        model.objectLayoutModel_ = &objectLayoutModel;
        return true;
    }

    static bool appendAggregateLeaves(
        ProjectOopHirModel& model,
        const std::string& typeKey,
        std::uint64_t baseOffset,
        const ast::AstNode* syntax,
        std::vector<std::unique_ptr<ProjectOopHirNode>>& leaves,
        std::string& error)
    {
        return model.appendAggregateLeaves(
            typeKey, baseOffset, syntax, leaves, error);
    }

    static void setAggregateRootFields(
        ProjectObjectLayoutModel& objectLayoutModel,
        std::vector<ObjectFieldLayout> fields)
    {
        objectLayoutModel.structLayouts_.front().fields = std::move(fields);
    }

    static void configureAggregateElementBindings(
        ProjectOopHirModel& model,
        ProjectMethodBodyModel& methodBodyModel,
        const std::vector<ast::AstNode*>& aggregates,
        const ast::AstNode* literal,
        const std::vector<std::string>& typeKeys,
        const MethodExpressionTypeBinding& literalType)
    {
        methodBodyModel.aggregateInitializerBindings_.reserve(aggregates.size());
        for (std::size_t ordinal = 0; ordinal < aggregates.size(); ++ordinal)
        {
            MethodAggregateInitializerEntryBinding entry;
            entry.syntax = ordinal + 1U < aggregates.size()
                ? aggregates[ordinal + 1U]
                : literal;
            entry.targetElementOrdinals.push_back(0U);

            MethodAggregateInitializerBinding binding;
            binding.syntax = aggregates[ordinal];
            binding.targetTypeKey = typeKeys[ordinal];
            binding.entries.push_back(std::move(entry));
            methodBodyModel.aggregateInitializerBindingBySyntax_[aggregates[ordinal]] =
                methodBodyModel.aggregateInitializerBindings_.size();
            methodBodyModel.aggregateInitializerBindings_.push_back(std::move(binding));
        }
        model.methodBodyModel_ = &methodBodyModel;
        model.expressionTypeBySyntax_[literal] = &literalType;
    }

    static bool appendAggregateElements(
        ProjectOopHirModel& model,
        const ast::AstNode* syntax,
        const std::string& targetTypeKey,
        std::uint64_t baseOffset,
        std::vector<std::unique_ptr<ProjectOopHirNode>>& elements,
        std::string& error)
    {
        return model.appendAggregateElements(
            syntax, targetTypeKey, baseOffset, 0U, elements, error);
    }

    static std::unique_ptr<ProjectOopHirNode> lowerBlock(
        ProjectOopHirModel& model,
        const ast::AstNode* syntax,
        std::string& error)
    {
        return model.lowerBlock(syntax, 0U, error);
    }

    static std::unique_ptr<ProjectOopHirNode> lowerDefaultArgument(
        ProjectOopHirModel& model,
        const ast::AstNode* syntax,
        std::string_view parameterTypeKey,
        std::string& error)
    {
        return model.lowerDefaultArgument(syntax, parameterTypeKey, error);
    }

    static void assignYieldStates(
        ProjectOopHirModel& model,
        ProjectOopHirNode* root,
        std::size_t& nextYieldState)
    {
        model.assignYieldStates(root, nextYieldState);
    }

    static void rewriteGeneratorThrows(
        ProjectOopHirModel& model,
        ProjectOopHirNode* root,
        std::string_view errorTypeKey,
        std::string_view iteratorKey)
    {
        model.rewriteGeneratorThrows(root, errorTypeKey, iteratorKey);
    }

    static std::unique_ptr<ProjectOopHirNode> rewriteAsyncPollReturns(
        ProjectOopHirModel& model,
        std::unique_ptr<ProjectOopHirNode> body,
        std::size_t selfParameterOrdinal,
        std::string_view resultTypeKey,
        std::string_view futureKey,
        std::string& error,
        bool topLevel)
    {
        return model.rewriteAsyncPollReturns(
            std::move(body),
            selfParameterOrdinal,
            resultTypeKey,
            futureKey,
            error,
            topLevel);
    }

    static std::vector<std::size_t> collectAwaitedMemberOrdinals(
        ProjectOopHirModel& model,
        const ast::AstNode* root,
        const ast::AstNode* callSyntax,
        const MethodCallBinding& binding)
    {
        model.callBySyntax_[callSyntax] = &binding;
        return model.collectAwaitedMemberOrdinals(root);
    }

    static std::unique_ptr<ProjectOopHirNode> lowerExpression(
        ProjectOopHirModel& model,
        const ast::AstNode* root,
        const std::vector<std::unique_ptr<ast::AstNode>>& syntax,
        const MethodExpressionTypeBinding& binding,
        std::string& error)
    {
        for (const auto& node : syntax) model.expressionTypeBySyntax_[node.get()] = &binding;
        return model.lowerExpression(root, 0U, false, error);
    }

    static std::unique_ptr<ProjectOopHirNode> lowerFieldExpression(
        ProjectOopHirModel& model,
        const ast::AstNode* root,
        const std::vector<std::unique_ptr<ast::AstNode>>& syntax,
        const std::vector<MethodNameBinding>& names,
        const MethodExpressionTypeBinding& type,
        bool addressContext,
        std::string& error)
    {
        for (const auto& node : syntax)
        {
            model.expressionTypeBySyntax_[node.get()] = &type;
        }
        for (const MethodNameBinding& name : names)
        {
            model.nameBindingBySyntax_[name.syntax] = &name;
        }
        return model.lowerExpression(root, 0U, addressContext, error);
    }

    static std::unique_ptr<ProjectOopHirNode> lowerPropertyGetterExpression(
        ProjectOopHirModel& model,
        const ProjectMemberModel& memberModel,
        std::size_t callerMemberOrdinal,
        const ast::AstNode* root,
        const std::vector<std::unique_ptr<ast::AstNode>>& syntax,
        const std::vector<MethodCallBinding>& calls,
        const MethodExpressionTypeBinding& type,
        std::string& error)
    {
        model.memberModel_ = &memberModel;
        for (const auto& node : syntax) model.expressionTypeBySyntax_[node.get()] = &type;
        for (const MethodCallBinding& call : calls)
            model.callBySyntax_[call.syntax] = &call;
        return model.lowerExpression(root, callerMemberOrdinal, false, error);
    }

    static void setRuntimeTypeBinding(
        ProjectOopHirModel& model,
        const ast::AstNode* syntax,
        const MethodRuntimeTypeBinding& binding)
    {
        model.runtimeTypeBindingBySyntax_[syntax] = &binding;
    }

    static std::unique_ptr<ProjectOopHirNode> lowerStatement(
        ProjectOopHirModel& model,
        const ast::AstNode* statement,
        const std::vector<std::unique_ptr<ast::AstNode>>& syntax,
        const MethodExpressionTypeBinding& binding,
        std::string& error)
    {
        for (const auto& node : syntax) model.expressionTypeBySyntax_[node.get()] = &binding;
        return model.lowerStatement(statement, 0U, error);
    }

    static void setDumperBody(
        ProjectOopHirModel& model,
        std::unique_ptr<ProjectOopHirNode> body)
    {
        ProjectOopHirFunction function;
        function.isStatic = true;
        function.mangledName = "deep_hir_dump";
        function.returnTypeKey = "builtin:void";
        function.body = std::move(body);
        model.functions_.push_back(std::move(function));
    }

    static std::unique_ptr<ProjectOopHirNode> takeDumperBody(ProjectOopHirModel& model)
    {
        if (model.functions_.empty()) return nullptr;
        return std::move(model.functions_.front().body);
    }

    static void setDumperFieldInitializer(
        ProjectOopHirModel& model,
        std::unique_ptr<ProjectOopHirNode> value)
    {
        ProjectOopHirFieldInitializer initializer;
        initializer.value = std::move(value);
        model.fieldInitializers_.push_back(std::move(initializer));
    }

    static bool stackClassLocalUsesAreSafe(
        const ProjectOopHirNode* node,
        const ProjectOopHirNode* declaration,
        std::size_t localOrdinal)
    {
        return ProjectOopHirModel::stackClassLocalUsesAreSafe(
            node, declaration, localOrdinal);
    }

    static void collectLocalDeclarations(
        ProjectOopHirNode* node,
        std::vector<ProjectOopHirNode*>& declarations)
    {
        ProjectOopHirModel::collectLocalDeclarations(node, declarations);
    }

    static const ProjectOopHirNode* firstUnpromotedObjectConstruction(
        const ProjectOopHirNode* node)
    {
        return ProjectOopHirModel::firstUnpromotedObjectConstruction(node);
    }

    static void scanParallelOrdinals(
        const ProjectOopHirNode* node,
        std::size_t localFloor,
        std::size_t cleanupFloor,
        std::size_t& localCount,
        std::size_t& cleanupCount,
        std::size_t& regionCount)
    {
        ProjectOopHirModel::scanParallelOrdinals(
            node, localFloor, cleanupFloor, localCount, cleanupCount, regionCount);
    }

    static void applyParallelOrdinalOffsets(
        ProjectOopHirNode* node,
        std::size_t localFloor,
        std::size_t cleanupFloor,
        std::size_t localOffset,
        std::size_t cleanupOffset,
        std::size_t regionOffset)
    {
        ProjectOopHirModel::applyParallelOrdinalOffsets(
            node, localFloor, cleanupFloor, localOffset, cleanupOffset, regionOffset);
    }
};
}

namespace
{
using nativesharp::frontend::ProjectCompilation;
using nativesharp::frontend::ProjectOopHirModel;
using nativesharp::frontend::ProjectOopMirModel;
using nativesharp::frontend::ProjectOopMirNode;
using nativesharp::frontend::ProjectOopMirNodeKind;
using nativesharp::tests::TestContext;

void writeFile(const std::filesystem::path& path, std::string_view text)
{
    std::filesystem::create_directories(path.parent_path());
    std::ofstream stream(path, std::ios::binary);
    stream << text;
}

void writeProject(const std::filesystem::path& directory, std::string_view name)
{
    writeFile(
        directory / (std::string(name) + ".nsproj"),
        "<Project><PropertyGroup><Name>" + std::string(name) +
            "</Name></PropertyGroup><ItemGroup><Compile Include=\"**/*.ns\" /></ItemGroup></Project>");
}

std::size_t countMirKind(const ProjectOopMirNode* node, ProjectOopMirNodeKind kind)
{
    if (node == nullptr)
    {
        return 0;
    }
    std::size_t count = node->kind == kind ? 1u : 0u;
    for (const auto& child : node->children)
    {
        count += countMirKind(child.get(), kind);
    }
    return count;
}

const ProjectOopMirNode* findMirKind(const ProjectOopMirNode* node, ProjectOopMirNodeKind kind)
{
    if (node == nullptr)
    {
        return nullptr;
    }
    if (node->kind == kind)
    {
        return node;
    }
    for (const auto& child : node->children)
    {
        if (const ProjectOopMirNode* found = findMirKind(child.get(), kind); found != nullptr)
        {
            return found;
        }
    }
    return nullptr;
}

// EL GLOBAL QUE GUARDA UNA CADENA CONCRETA. Los nombres `@__nsc_str_N` son POSICIONALES: buscar
// `@__nsc_str_1` a mano ataria la prueba al orden en que hoy se emiten los literales, y ese orden
// no es el contrato. Se busca el global por su CONTENIDO y se devuelve el nombre que tenga.
std::string stringGlobalHolding(std::string_view ir, std::string_view content)
{
    const std::string needle = "c\"" + std::string(content) + "\\00\"";
    const std::size_t at = ir.find(needle);
    if (at == std::string_view::npos) return {};
    const std::size_t lineStart = ir.rfind('\n', at);
    const std::string_view line =
        ir.substr(lineStart == std::string_view::npos ? 0 : lineStart + 1);
    const std::size_t nameEnd = line.find(' ');
    if (line.empty() || line.front() != '@' || nameEnd == std::string_view::npos) return {};
    return std::string(line.substr(0, nameEnd));
}

// EL CUERPO DE UNA FUNCION, para no confundir lo que hace una con lo que hace otra. Buscar
// `ret ptr @X` en el modulo entero encontraria cualquier funcion que devolviera esa cadena.
std::string functionBody(std::string_view ir, std::string_view mangledFragment)
{
    const std::size_t at = ir.find(mangledFragment);
    if (at == std::string_view::npos) return {};
    const std::size_t open = ir.find('{', at);
    if (open == std::string_view::npos) return {};
    const std::size_t close = ir.find("\n}", open);
    if (close == std::string_view::npos) return {};
    return std::string(ir.substr(open, close - open));
}

// NSC-LIMIT-005 -- EL ENUM ESTRECHO CON SIGNO, FIJADO PARA QUE NO SE PUEDA DESHACER EN SILENCIO.
//
// EL DEFECTO QUE ESTO CONSERVA. `c38_enum_ancho` devolvia 42 a -O0 y 39 a -O2, y `b9` reprodujo
// el motivo: `ToString()` de un `enum : sbyte` con valor NEGATIVO devolvia la RESERVA --el nombre
// del tipo-- despues de optimizar. El IR sin optimizar era correcto y el modulo verificaba; lo que
// fallaba era la decision del optimizador sobre una constante negativa estrecha.
//
// EL ARREGLO, y por que hay que fijarlo: `lowerEnumName` promueve el receptor con `zext` a i64 y
// compara contra el PATRON DE BITS enmascarado al ancho del tipo subyacente. Con `sext` --que es
// el cambio de una letra que cualquiera haria «limpiando» un tipo con signo-- `-3` se convierte en
// `i64 -3` mientras el caso sigue siendo `i64 253`, no casa ninguno, y se elige la reserva. MEDIDO:
// con esa unica letra cambiada, esta misma sonda a O2 pasa de devolver el global de "MenosTres" a
// devolver el de la reserva. Es exactamente el fallo original.
//
// POR ESO SE COMPRUEBAN LAS DOS CAPAS. La forma del IR sola no bastaria --describe la
// implementacion de hoy-- y el resultado optimizado solo tampoco --pasaria por accidente si el
// plegado cambiara--. Juntas dicen que la eleccion es correcta Y por que lo es.
void testNarrowSignedEnumNameLowering(TestContext& tests, const std::filesystem::path& root)
{
    const auto directory = root / "NarrowSignedEnumName";
    writeProject(directory, "NarrowSignedEnumName");
    writeFile(directory / "Main.ns",
        "namespace Sonda; public class Prueba { "
        "public enum ConSigno : sbyte { MenosTres = -3, Cero = 0, Tope = 127 } "
        "public string Nombre() { return ConSigno.MenosTres.ToString(); } }");

    ProjectCompilation compilation;
    std::string error;
    const bool ok =
        compilation.compileProjectOopMir(directory / "NarrowSignedEnumName.nsproj", error);
    tests.expect(ok && compilation.oopMirModel() != nullptr,
        "a narrow signed enum with a negative enumerator compiles through project OOP MIR");
    if (!ok || compilation.oopMirModel() == nullptr) return;

    nativesharp::llvm_backend::ProjectOopLlvmLowerer lowerer;
    auto module = lowerer.lower(*compilation.nameLookup(), *compilation.oopMirModel());
    tests.expect(module != nullptr && lowerer.error().empty(),
        "the narrow signed enum lowers to LLVM without backend errors");
    if (module == nullptr) return;

    std::string verifyError;
    tests.expect(module->verify(verifyError),
        "the enum-name switch is verifier-clean, so no two cases collapsed to the same value");

    const std::string plain = module->text();
    const std::string body = functionBody(plain, "Prueba__Nombre");
    tests.expect(!body.empty(), "the enum-name function is found in the unoptimized module");

    // CAPA 1 -- LA FORMA. Promocion SIN SIGNO a 64 bits y un `switch`, no una cadena de `select`.
    tests.expect(body.find("zext i8 ") != std::string::npos &&
            body.find(" to i64") != std::string::npos,
        "the narrow receiver is promoted to i64 without sign");
    tests.expect(body.find("sext i8 ") == std::string::npos,
        "and never sign-extended: that is the one-letter change that reintroduces the defect");
    tests.expect(body.find("switch i64 ") != std::string::npos,
        "the enumerators are decided by a switch on the 64-bit bit pattern");
    tests.expect(body.find("select ") == std::string::npos,
        "and not by the select chain this replaced");

    // CAPA 2 -- EL PATRON DE BITS. -3 en ocho bits es 253, no -3. Si el caso se escribiera con el
    // numero fuente en vez de con sus bits, no casaria nunca con el receptor promovido sin signo.
    tests.expect(body.find("i64 253, label") != std::string::npos,
        "the negative enumerator is cased as its 8-bit pattern (253), not as -3");
    tests.expect(body.find("i64 -3, label") == std::string::npos,
        "the signed spelling of the same enumerator is not what the switch compares against");

    // CAPA 3 -- EL RESULTADO DESPUES DE OPTIMIZAR, que es donde el defecto se manifestaba. Con un
    // receptor constante, O2 pliega la funcion entera a un `ret` del global correcto. Que sea el
    // de "MenosTres" y no el de la reserva es la prueba de que la decision sobrevive al pipeline.
    std::string targetError;
    auto targetMachine = nativesharp::llvm_backend::LlvmTargetMachine::createWindowsX64(
        nativesharp::llvm_backend::LlvmOptimizationLevel::O2, targetError);
    tests.expect(targetMachine != nullptr, "the O2 target machine is available for the enum probe");
    if (targetMachine == nullptr) return;

    nativesharp::llvm_backend::LlvmOptimizer optimizer;
    std::string optimizationError;
    tests.expect(
        optimizer.optimize(*module, *targetMachine,
            nativesharp::llvm_backend::LlvmOptimizationLevel::O2, optimizationError),
        "the narrow signed enum module survives the O2 pipeline");

    const std::string optimized = module->text();
    const std::string chosen = stringGlobalHolding(optimized, "MenosTres");
    const std::string fallback = stringGlobalHolding(optimized, "Prueba+ConSigno");
    tests.expect(!chosen.empty() && (fallback.empty() || chosen != fallback),
        "the enumerator name survives and cannot alias the private type-name fallback");
    if (chosen.empty()) return;

    const std::string optimizedBody = functionBody(optimized, "Prueba__Nombre");
    tests.expect(optimizedBody.find("ret ptr " + chosen) != std::string::npos,
        "after O2 the constant receiver folds to the enumerator name");
    tests.expect(fallback.empty() || optimizedBody.find("ret ptr " + fallback) == std::string::npos,
        "and not to the type-name fallback, which may now be removed as a private unused literal");
}

void testEndToEndOopLowering(TestContext& tests, const std::filesystem::path& root)
{
    const auto directory = root / "EndToEnd";
    writeProject(directory, "Stage30Test");
    writeFile(directory / "A.ns",
        "namespace Demo; public partial class Counter { byte Tag; int Value; "
        "public Counter(int start) { this.Value = start; } "
        "public int Add(int delta) { this.Value = this.Value + delta; return this.Value; } "
        "public int SumTo(int n) { int i = 0; int total = 0; while (i < n) { total = total + i; i = i + 1; } return total; } }");
    writeFile(directory / "B.ns",
        "namespace Demo; public partial class Counter { "
        "public int AddTwice(int delta) { return Add(delta) + Add(delta); } "
        "public Counter Make(int start) { return new Counter(start); } "
        "public Counter MakeAfterAdd(int delta) { return new Counter(Add(delta)); } } "
        "public class Empty { } public class Factory { public Empty MakeEmpty() { return new Empty(); } }");

    ProjectCompilation compilation;
    std::string error;
    const bool ok = compilation.compileProjectOopMir(directory / "Stage30Test.nsproj", error);
    tests.expect(ok, "Stage 30 project compiles through OOP MIR");
    tests.expect(error.empty(), "Stage 30 OOP MIR build has no loader/lowering error");
    tests.expect(compilation.oopHirModel() != nullptr && compilation.oopMirModel() != nullptr,
        "ProjectCompilation retains both OOP HIR and OOP MIR models");
    if (!ok || compilation.oopHirModel() == nullptr || compilation.oopMirModel() == nullptr)
    {
        return;
    }

    const ProjectOopHirModel& hir = *compilation.oopHirModel();
    const ProjectOopMirModel& mir = *compilation.oopMirModel();
    tests.expect(hir.functions().size() == 9 && mir.functions().size() == 9,
        "seven written callables plus two publicable implicit constructors lower through both OOP IR layers");
    tests.expect(hir.functions()[0].mangledName.starts_with("__nsc_ctor_") &&
            hir.functions()[0].parameterTypeKeys.size() == 1,
        "constructor HIR has deterministic mangling and excludes implicit this from source parameter list");
    tests.expect(mir.functions()[0].constructor && mir.functions()[0].returnTypeKey == "builtin:void",
        "constructor MIR freezes void-return internal ABI");

    std::size_t fieldLoads = 0;
    std::size_t fieldAddresses = 0;
    std::size_t directCalls = 0;
    std::size_t constructions = 0;
    std::size_t loops = 0;
    for (const auto& function : mir.functions())
    {
        fieldLoads += countMirKind(function.body.get(), ProjectOopMirNodeKind::FieldLoad);
        fieldAddresses += countMirKind(function.body.get(), ProjectOopMirNodeKind::FieldAddress);
        directCalls += countMirKind(function.body.get(), ProjectOopMirNodeKind::DirectInstanceCall);
        constructions += countMirKind(function.body.get(), ProjectOopMirNodeKind::AllocateAndConstruct);
        loops += countMirKind(function.body.get(), ProjectOopMirNodeKind::While);
    }
    tests.expect(fieldLoads >= 2 && fieldAddresses >= 2,
        "field reads/writes survive into OOP MIR as explicit address/load operations");
    tests.expect(directCalls == 3, "three Add(delta) calls become direct instance-call MIR operations");
    tests.expect(constructions == 3, "two explicit and one implicit-default new expressions become allocation MIR operations");
    tests.expect(loops == 1, "structured while control flow survives into OOP MIR");

    const ProjectOopMirNode* fieldAddress = findMirKind(mir.functions()[0].body.get(), ProjectOopMirNodeKind::FieldAddress);
    tests.expect(fieldAddress != nullptr && fieldAddress->immediate == 12 && fieldAddress->alignment == 4,
        "MIR consumes the Macro A object header and resolves Counter.Value to byte offset twelve");

    const ProjectOopMirNode* explicitConstruction = nullptr;
    const ProjectOopMirNode* defaultConstruction = nullptr;
    for (const auto& function : mir.functions())
    {
        const ProjectOopMirNode* construction = findMirKind(function.body.get(), ProjectOopMirNodeKind::AllocateAndConstruct);
        if (construction != nullptr)
        {
            if (construction->calleeName.empty()) defaultConstruction = construction;
            else explicitConstruction = construction;
        }
    }
    tests.expect(explicitConstruction != nullptr && explicitConstruction->immediate == 16 &&
            explicitConstruction->zeroInitialize && explicitConstruction->allocationFailureReturnsNull,
        "explicit construction MIR consumes the header-inclusive object size plus zero-init/null-on-failure contract");
    tests.expect(defaultConstruction != nullptr && defaultConstruction->immediate == 8 && defaultConstruction->calleeName.empty(),
        "root implicit default construction allocates the descriptor-only object without synthesizing a constructor entry point");

    const std::string hirDump = nativesharp::frontend::ProjectOopHirModelDumper::dump(hir);
    const std::string mirDump = nativesharp::frontend::ProjectOopMirModelDumper::dump(mir);
    tests.expect(hirDump.find("abi=this:first") != std::string::npos &&
            hirDump.find("object-construction") != std::string::npos,
        "OOP HIR dump exposes internal ABI and semantic construction nodes");
    tests.expect(mirDump.find("offset=12 align=4") != std::string::npos &&
            mirDump.find("zero=true failure=null") != std::string::npos,
        "OOP MIR dump exposes header-adjusted field offsets and allocation contract");

    ProjectCompilation parallelCompilation;
    parallelCompilation.setCompilationJobs(4);
    std::string parallelError;
    const bool parallelOk = parallelCompilation.compileProjectOopMir(
        directory / "Stage30Test.nsproj", parallelError);
    tests.expect(parallelOk && parallelError.empty() && parallelCompilation.oopHirModel() != nullptr &&
            parallelCompilation.oopMirModel() != nullptr,
        "four-worker per-function HIR and MIR lowering succeeds on the same project");
    if (parallelCompilation.oopHirModel() != nullptr && parallelCompilation.oopMirModel() != nullptr)
    {
        const std::string parallelHirDump =
            nativesharp::frontend::ProjectOopHirModelDumper::dump(
                *parallelCompilation.oopHirModel());
        tests.expect(parallelHirDump == hirDump,
            "parallel per-function HIR is exactly equivalent to serial HIR");
        const std::string parallelMirDump =
            nativesharp::frontend::ProjectOopMirModelDumper::dump(
                *parallelCompilation.oopMirModel());
        tests.expect(parallelMirDump == mirDump,
            "parallel per-function MIR is exactly equivalent to serial MIR");

        nativesharp::llvm_backend::ProjectOopCodePartition serialPartition;
        nativesharp::llvm_backend::ProjectOopCodePartition parallelPartition;
        std::string partitionError;
        const bool serialPartitionOk = serialPartition.build(mir, 4, partitionError);
        const bool parallelPartitionOk = parallelPartition.build(
            *parallelCompilation.oopMirModel(), 4, partitionError);
        tests.expect(serialPartitionOk && parallelPartitionOk && partitionError.empty(),
            "module code partitioning accepts equivalent serial and parallel MIR");
        tests.expect(serialPartition.dump() == parallelPartition.dump(),
            "module code partitioning is deterministic across MIR scheduling modes");

        std::vector<bool> assigned(mir.functions().size(), false);
        bool validCoverage = true;
        for (const auto& unit : serialPartition.units())
        {
            validCoverage = validCoverage && !unit.stableKey.empty() &&
                !unit.functionOrdinals.empty();
            for (const std::size_t functionOrdinal : unit.functionOrdinals)
            {
                validCoverage = validCoverage && functionOrdinal < assigned.size() &&
                    !assigned[functionOrdinal] &&
                    serialPartition.unitForFunction(functionOrdinal) == unit.ordinal;
                if (functionOrdinal < assigned.size()) assigned[functionOrdinal] = true;
            }
        }
        for (std::size_t functionOrdinal = 0; functionOrdinal < mir.functions().size(); ++functionOrdinal)
        {
            const auto& function = mir.functions()[functionOrdinal];
            const bool shouldBeAssigned = !function.isImportedC && !function.isImportedNative;
            validCoverage = validCoverage && assigned[functionOrdinal] == shouldBeAssigned;
        }
        tests.expect(validCoverage,
            "every defined MIR function belongs to exactly one stable code unit and imports belong to none");

        nativesharp::llvm_backend::ProjectOopLlvmLowerer globalLowerer;
        auto globalModule = globalLowerer.lowerPartition(
            *compilation.nameLookup(), *compilation.memberModel(), mir,
            compilation.sourceManager(), nativesharp::llvm_backend::DebugInformationKind::None,
            nativesharp::llvm_backend::LlvmOptimizationLevel::O0, {}, true,
            "Stage30Test.globals");
        bool partitionModulesValid = globalModule != nullptr && globalLowerer.error().empty();
        std::string partitionFailure = globalLowerer.error();
        std::string partitionVerificationError;
        if (globalModule != nullptr && !globalModule->verify(partitionVerificationError))
        {
            partitionModulesValid = false;
            partitionFailure = partitionVerificationError;
        }
        for (const auto& unit : serialPartition.units())
        {
            nativesharp::llvm_backend::ProjectOopLlvmLowerer unitLowerer;
            auto unitModule = unitLowerer.lowerPartition(
                *compilation.nameLookup(), *compilation.memberModel(), mir,
                compilation.sourceManager(), nativesharp::llvm_backend::DebugInformationKind::None,
                nativesharp::llvm_backend::LlvmOptimizationLevel::O0,
                unit.functionOrdinals, false, "Stage30Test." + unit.stableKey);
            partitionModulesValid = partitionModulesValid && unitModule != nullptr &&
                unitLowerer.error().empty();
            if (unitModule == nullptr && partitionFailure.empty())
                partitionFailure = unitLowerer.error();
            if (unitModule != nullptr)
            {
                partitionVerificationError.clear();
                if (!unitModule->verify(partitionVerificationError))
                {
                    partitionModulesValid = false;
                    if (partitionFailure.empty()) partitionFailure = partitionVerificationError;
                }
            }
        }
        tests.expect(partitionModulesValid,
            "global and per-code-unit LLVM modules independently pass the verifier" +
                (partitionFailure.empty() ? std::string{} : ": " + partitionFailure));

        nativesharp::llvm_backend::ProjectOopLlvmLowerer debugGlobalLowerer;
        auto debugGlobalModule = debugGlobalLowerer.lowerPartition(
            *compilation.nameLookup(), *compilation.memberModel(), mir,
            compilation.sourceManager(), nativesharp::llvm_backend::DebugInformationKind::Full,
            nativesharp::llvm_backend::LlvmOptimizationLevel::O0, {}, true,
            "Stage30Test.debug.globals");
        bool partitionDebugValid = debugGlobalModule != nullptr &&
            debugGlobalLowerer.error().empty();
        std::string partitionDebugFailure = debugGlobalLowerer.error();
        for (const auto& unit : serialPartition.units())
        {
            nativesharp::llvm_backend::ProjectOopLlvmLowerer debugUnitLowerer;
            auto debugUnitModule = debugUnitLowerer.lowerPartition(
                *compilation.nameLookup(), *compilation.memberModel(), mir,
                compilation.sourceManager(), nativesharp::llvm_backend::DebugInformationKind::Full,
                nativesharp::llvm_backend::LlvmOptimizationLevel::O0,
                unit.functionOrdinals, false, "Stage30Test.debug." + unit.stableKey);
            partitionDebugValid = partitionDebugValid && debugUnitModule != nullptr &&
                debugUnitLowerer.error().empty();
            if (debugUnitModule == nullptr && partitionDebugFailure.empty())
                partitionDebugFailure = debugUnitLowerer.error();
        }
        tests.expect(partitionDebugValid,
            "partitioned debug metadata belongs only to defining modules and verifies cleanly" +
                (partitionDebugFailure.empty() ? std::string{} : ": " + partitionDebugFailure));

        nativesharp::llvm_backend::ProjectOopObjectGenerator objectGenerator;
        nativesharp::llvm_backend::CompilationArtifactCache artifactCache;
        artifactCache.setRoot(directory / "object-cache");
        artifactCache.setCodegenIdentity("project-oop-lowering-test");
        nativesharp::llvm_backend::CompilationArtifactCacheScope artifactCacheScope(artifactCache);
        std::string objectGenerationError;
        auto objectBundle = objectGenerator.generate(
            *compilation.nameLookup(), *compilation.memberModel(), mir,
            compilation.sourceManager(), nativesharp::llvm_backend::DebugInformationKind::None,
            nativesharp::llvm_backend::LlvmOptimizationLevel::O0, 4, {}, objectGenerationError);
        tests.expect(objectBundle != nullptr && objectGenerationError.empty() &&
                objectBundle->codeUnitCount() == serialPartition.units().size() &&
                objectBundle->objectFiles().size() == serialPartition.units().size() + 1 &&
                objectBundle->cacheHitCount() == 0 &&
                objectBundle->cacheMissCount() == objectBundle->objectFiles().size(),
            "four-worker code generation emits one verified coordinator object and one object per code unit");
        if (objectBundle != nullptr)
        {
            objectGenerationError.clear();
            auto cachedBundle = objectGenerator.generate(
                *compilation.nameLookup(), *compilation.memberModel(), mir,
                compilation.sourceManager(),
                nativesharp::llvm_backend::DebugInformationKind::None,
                nativesharp::llvm_backend::LlvmOptimizationLevel::O0, 4, {},
                objectGenerationError);
            tests.expect(cachedBundle != nullptr && objectGenerationError.empty() &&
                    cachedBundle->cacheHitCount() == cachedBundle->objectFiles().size() &&
                    cachedBundle->cacheMissCount() == 0,
                "second equivalent generation restores every verified code unit from content cache" +
                    (objectGenerationError.empty() ? std::string{} : ": " + objectGenerationError));

            objectGenerationError.clear();
            auto coldO0Bundle = objectGenerator.generate(
                *compilation.nameLookup(), *compilation.memberModel(), mir,
                compilation.sourceManager(),
                nativesharp::llvm_backend::DebugInformationKind::None,
                nativesharp::llvm_backend::LlvmOptimizationLevel::O0, 16, {},
                objectGenerationError);
            tests.expect(coldO0Bundle != nullptr && objectGenerationError.empty() &&
                    coldO0Bundle->codeUnitCount() <= 8 &&
                    coldO0Bundle->objectFiles().size() == coldO0Bundle->codeUnitCount() + 1,
                "O0 keeps the requested worker pool but caps cold LLVM code-unit granularity at eight" +
                    (objectGenerationError.empty() ? std::string{} : ": " + objectGenerationError));

            nativesharp::linker::WindowsLibraryBuilder partitionedBuilder;
            const std::vector<std::string> exports = {mir.functions()[1].mangledName};
            std::string linkError;
            const bool linked = partitionedBuilder.buildDynamicObjects(
                objectBundle->objectFiles(), objectBundle->combinedLlvmText(), exports,
                directory / "Stage30Partitioned.dll", directory / "Stage30Partitioned.lib",
                nativesharp::llvm_backend::LlvmOptimizationLevel::O0,
                nativesharp::llvm_backend::DebugInformationKind::None, linkError);
            tests.expect(linked && linkError.empty(),
                "partitioned code units link into one native DLL without duplicate or missing symbols" +
                    (linkError.empty() ? std::string{} : ": " + linkError));

            objectGenerationError.clear();
            auto optimizedBundle = objectGenerator.generate(
                *compilation.nameLookup(), *compilation.memberModel(), mir,
                compilation.sourceManager(),
                nativesharp::llvm_backend::DebugInformationKind::None,
                nativesharp::llvm_backend::LlvmOptimizationLevel::O2, 4, {},
                objectGenerationError);
            tests.expect(optimizedBundle != nullptr && objectGenerationError.empty() &&
                    optimizedBundle->usedParallelOptimization(),
                "O2 optimization runs as independent code-unit tasks" +
                    (objectGenerationError.empty() ? std::string{} : ": " + objectGenerationError));
            if (optimizedBundle != nullptr)
            {
                linkError.clear();
                const bool optimizedLinked = partitionedBuilder.buildDynamicObjects(
                    optimizedBundle->objectFiles(), optimizedBundle->combinedLlvmText(), exports,
                    directory / "Stage30PartitionedO2.dll",
                    directory / "Stage30PartitionedO2.lib",
                    nativesharp::llvm_backend::LlvmOptimizationLevel::O2,
                    nativesharp::llvm_backend::DebugInformationKind::None, linkError);
                tests.expect(optimizedLinked && linkError.empty(),
                    "parallel O2 code units retain all cross-unit and exported symbols at link" +
                        (linkError.empty() ? std::string{} : ": " + linkError));
            }

            objectGenerationError.clear();
            auto optimizedDebugBundle = objectGenerator.generate(
                *compilation.nameLookup(), *compilation.memberModel(), mir,
                compilation.sourceManager(),
                nativesharp::llvm_backend::DebugInformationKind::Full,
                nativesharp::llvm_backend::LlvmOptimizationLevel::O2, 4, {},
                objectGenerationError);
            tests.expect(optimizedDebugBundle != nullptr && objectGenerationError.empty() &&
                    optimizedDebugBundle->codeUnitCount() == 1 &&
                    optimizedDebugBundle->objectFiles().size() == 1 &&
                    !optimizedDebugBundle->usedParallelOptimization(),
                "full-debug optimized generation remains monolithic so LLVM retains inline sites" +
                    (objectGenerationError.empty() ? std::string{} : ": " + objectGenerationError));
        }
    }

    nativesharp::llvm_backend::ProjectOopLlvmLowerer lowerer;
    std::unique_ptr<nativesharp::llvm_backend::LlvmModule> module = lowerer.lower(
        *compilation.nameLookup(), mir);
    tests.expect(module != nullptr, "Stage 30 lowers project OOP MIR to an LLVM module");
    tests.expect(lowerer.error().empty(), "LLVM OOP lowering reports no internal error for valid project");
    if (module == nullptr)
    {
        return;
    }
    std::string verificationError;
    tests.expect(module->verify(verificationError), "LLVM verifier accepts Stage 30 OOP module");
    tests.expect(verificationError.empty(), "verified Stage 30 LLVM module has no verifier diagnostics");

    const std::string llvm = module->text();
    tests.expect(llvm.find("target triple = \"x86_64-pc-windows-msvc\"") != std::string::npos,
        "OOP LLVM module uses NativeSharp Windows x64 target triple");
    tests.expect(llvm.find("declare ptr @__nativesharp_alloc(i64)") != std::string::npos,
        "OOP LLVM module declares the existing NativeSharp allocation hook");
    tests.expect(llvm.find("declare void @__nativesharp_free(ptr)") != std::string::npos,
        "OOP LLVM module declares the existing NativeSharp deallocation hook");
    tests.expect(llvm.find("@llvm.memset.p0.i64") != std::string::npos,
        "OOP object initialization materializes zero-initialization with LLVM memset");
    tests.expect(llvm.find("getelementptr i8, ptr %this, i64 12") != std::string::npos,
        "field addressing lowers exactly to the explicit receiver plus the Macro A byte offset");
    // NSC-LIMIT-009. LA BUSQUEDA LLEVA `@\"` Y NO `@`, y no es un adorno: todo simbolo que sale
    // de una identidad lleva el `$3A$3A` del `::` del modulo, y LLVM entrecomilla cualquier
    // nombre con un byte que no sea alfanumerico, `-`, `.` o `_`. Lo impreso es
    // `@\"__nsc_...\"`. Escrito a mano sin la comilla encuentra CERO, y en una asercion de
    // ausencia eso seria un verde falso. Ver TypeIdentity.h e `irTextSymbolReference`.
    tests.expect(llvm.find("call void @\"__nsc_ctor_") != std::string::npos,
        "explicit object construction lowers selected constructor call after allocation");
    tests.expect(
        llvm.find("define weak_odr void @\"__nsc_ctor_Stage30Test$3A$3ADemo$2EEmpty__Empty") !=
            std::string::npos &&
        llvm.find("define weak_odr void @\"__nsc_ctor_Stage30Test$3A$3ADemo$2EFactory__Factory") !=
            std::string::npos,
        "compiler-synthesized implicit constructors are per-symbol ODR definitions");
    tests.expect(
        llvm.find("define weak_odr void @\"__nsc_ctor_Stage30Test$3A$3ADemo$2ECounter__Counter") ==
            std::string::npos,
        "written constructors remain strong definitions rather than hiding user ODR conflicts");
    tests.expect(llvm.find("call i32 @\"__nsc_method_") != std::string::npos,
        "instance method calls lower to direct internal-ABI calls with implicit this");
    tests.expect(llvm.find("while.cond") != std::string::npos && llvm.find("while.body") != std::string::npos,
        "structured OOP while lowers to explicit LLVM control-flow blocks");
    tests.expect(llvm.find("alloc.failed") != std::string::npos && llvm.find("object.init") != std::string::npos,
        "allocation failure is checked before zero-init/constructor execution");

    const std::size_t makeAfterStart = llvm.find(
        "define ptr @\"__nsc_method_Stage30Test$3A$3ADemo$2ECounter__MakeAfterAdd__builtin$3Aint");
    const std::size_t makeAfterEnd = makeAfterStart == std::string::npos ? std::string::npos : llvm.find("\n}", makeAfterStart);
    const std::size_t argumentCall = makeAfterStart == std::string::npos ? std::string::npos :
        llvm.find(
        "call i32 @\"__nsc_method_Stage30Test$3A$3ADemo$2ECounter__Add__builtin$3Aint",
        makeAfterStart);
    const std::size_t allocationCall = makeAfterStart == std::string::npos ? std::string::npos :
        llvm.find("call ptr @__nativesharp_alloc", makeAfterStart);
    tests.expect(
        makeAfterStart != std::string::npos && makeAfterEnd != std::string::npos &&
            argumentCall != std::string::npos && allocationCall != std::string::npos &&
            argumentCall < allocationCall && allocationCall < makeAfterEnd,
        "object-construction arguments are evaluated before allocation and dominate the success branch");

    std::string targetError;
    auto targetMachine = nativesharp::llvm_backend::LlvmTargetMachine::createWindowsX64(
        nativesharp::llvm_backend::LlvmOptimizationLevel::O2, targetError);
    tests.expect(targetMachine != nullptr && targetError.empty(), "Windows x64 target machine is available for OOP codegen");
    if (targetMachine == nullptr)
    {
        return;
    }
    tests.expect(targetMachine->configureModuleForTarget(*module, targetError),
        "target data layout is applied to verified OOP LLVM module");
    nativesharp::llvm_backend::LlvmOptimizer optimizer;
    tests.expect(optimizer.optimize(*module, *targetMachine, nativesharp::llvm_backend::LlvmOptimizationLevel::O2, targetError),
        "standard LLVM O2 pipeline accepts project OOP module");

    const auto objectPath = directory / "stage30.obj";
    tests.expect(targetMachine->emit(*module, objectPath, nativesharp::llvm_backend::LlvmCodeGenFileType::Object, targetError),
        "optimized project OOP LLVM module emits a COFF object");
    nativesharp::llvm_backend::CoffObjectVerifier objectVerifier;
    tests.expect(objectVerifier.verify(objectPath), "Stage 30 emitted object passes structural COFF verification");
    tests.expect(objectVerifier.info().machine == 0x8664 && objectVerifier.info().sectionCount > 0,
        "Stage 30 OOP object targets AMD64 and contains sections");
}

void testDeallocationContract(TestContext& tests, const std::filesystem::path& root)
{
    const auto directory = root / "Deallocation";
    writeProject(directory, "Deallocation");
    writeFile(directory / "Main.ns",
        "class Payload { public void Release(Payload value) { delete value; delete null; } }");

    ProjectCompilation compilation;
    std::string error;
    const bool ok = compilation.compileProjectOopMir(directory / "Deallocation.nsproj", error);
    tests.expect(ok, "class-reference delete lowers through Stage 30 OOP MIR");
    tests.expect(error.empty(), "valid class-reference delete has no project lowering error");
    if (!ok || compilation.oopHirModel() == nullptr || compilation.oopMirModel() == nullptr)
    {
        return;
    }

    const auto& hir = *compilation.oopHirModel();
    const auto& mir = *compilation.oopMirModel();
    tests.expect(hir.functions().size() == 2 && mir.functions().size() == 2,
        "deallocation fixture produces its written method plus the publicable implicit constructor");
    tests.expect(countMirKind(mir.functions().front().body.get(), ProjectOopMirNodeKind::Deallocate) == 2,
        "delete reference and delete null both become explicit OOP MIR deallocations");

    const std::string hirDump = nativesharp::frontend::ProjectOopHirModelDumper::dump(hir);
    const std::string mirDump = nativesharp::frontend::ProjectOopMirModelDumper::dump(mir);
    tests.expect(hirDump.find("deallocation type=builtin:void") != std::string::npos,
        "OOP HIR dump exposes deallocation as a first-class lifetime operation");
    tests.expect(mirDump.find("deallocate type=builtin:void") != std::string::npos,
        "OOP MIR dump preserves executable deallocation intent");

    nativesharp::llvm_backend::ProjectOopLlvmLowerer lowerer;
    auto module = lowerer.lower(*compilation.nameLookup(), mir);
    tests.expect(module != nullptr && lowerer.error().empty(),
        "class-reference deallocation lowers to verified LLVM");
    if (module != nullptr)
    {
        std::string verificationError;
        tests.expect(module->verify(verificationError) && verificationError.empty(),
            "LLVM verifier accepts class-reference deallocation");
        const std::string llvm = module->text();
        tests.expect(llvm.find("declare void @__nativesharp_free(ptr)") != std::string::npos,
            "Stage 30 LLVM declares the NativeSharp free hook");
        std::size_t calls = 0;
        std::size_t position = 0;
        while ((position = llvm.find("call void @__nativesharp_free(ptr", position)) != std::string::npos)
        {
            ++calls;
            ++position;
        }
        tests.expect(calls == 2,
            "delete reference and delete null each materialize one runtime free call; runtime free owns null no-op semantics");

        std::string targetError;
        auto targetMachine = nativesharp::llvm_backend::LlvmTargetMachine::createWindowsX64(
            nativesharp::llvm_backend::LlvmOptimizationLevel::O2, targetError);
        tests.expect(targetMachine != nullptr && targetError.empty(),
            "deallocation fixture has Windows x64 target machine");
        if (targetMachine != nullptr)
        {
            tests.expect(targetMachine->configureModuleForTarget(*module, targetError),
                "deallocation LLVM receives Windows x64 data layout");
            nativesharp::llvm_backend::LlvmOptimizer optimizer;
            tests.expect(optimizer.optimize(
                    *module, *targetMachine, nativesharp::llvm_backend::LlvmOptimizationLevel::O2, targetError),
                "deallocation LLVM survives O2 optimization");
            const auto objectPath = directory / "deallocation.obj";
            tests.expect(targetMachine->emit(
                    *module, objectPath, nativesharp::llvm_backend::LlvmCodeGenFileType::Object, targetError),
                "deallocation LLVM emits Windows x64 COFF");
            nativesharp::llvm_backend::CoffObjectVerifier objectVerifier;
            tests.expect(objectVerifier.verify(objectPath) && objectVerifier.info().machine == 0x8664,
                "deallocation object passes AMD64 COFF verification");
        }
    }

    const auto invalidDirectory = root / "DeleteArrayClass";
    writeProject(invalidDirectory, "DeleteArrayClass");
    writeFile(invalidDirectory / "Main.ns",
        "class Payload { public void Release(Payload value) { delete[] value; } }");
    ProjectCompilation invalidCompilation;
    error.clear();
    const bool invalidOk = invalidCompilation.compileProjectOopMir(
        invalidDirectory / "DeleteArrayClass.nsproj", error);
    tests.expect(!invalidOk, "Stage 41 still rejects delete[] on a plain class reference; only T[] is a dynamic-array reference");
    tests.expect(error.find("delete[] requires a native dynamic array reference") != std::string::npos,
        "plain class-reference delete[] boundary remains explicit after dynamic arrays were introduced");
}

void testAggregateCallableBoundary(TestContext& tests, const std::filesystem::path& root)
{
    const auto directory = root / "AggregateBoundary";
    writeProject(directory, "AggregateBoundary");
    writeFile(directory / "Main.ns",
        "struct Large { int A; int B; int Z; } class Box { public Large Echo(Large value) { return value; } }");
    ProjectCompilation compilation;
    std::string error;
    const bool ok = compilation.compileProjectOopMir(directory / "AggregateBoundary.nsproj", error);
    tests.expect(ok, "large aggregate-by-value callable remains semantically valid through project OOP MIR");
    if (!ok || compilation.oopMirModel() == nullptr)
    {
        return;
    }
    nativesharp::llvm_backend::ProjectOopLlvmLowerer lowerer;
    auto module = lowerer.lower(
        *compilation.nameLookup(), *compilation.oopMirModel());
    // HISTORY: this test used to assert the OPPOSITE -- that a 12-byte aggregate crossing a
    // callable boundary was REJECTED ("Stage 40 direct struct value ABI supports only 1, 2, 4
    // or 8-byte aggregates"), pinning the documented limitation so it could not silently turn
    // into a miscompile. The limitation is now implemented instead of documented: any aggregate
    // whose size is not 1/2/4/8 travels indirectly, exactly the way clang lowers it for
    // x86_64-pc-windows-msvc -- the parameter as a pointer to a caller-owned per-call copy, the
    // return through a leading sret pointer -- so the same fixture must now LOWER, and the IR
    // must show the indirect shape rather than any direct aggregate value in the signature.
    tests.expect(module != nullptr && lowerer.error().empty(),
        "a 12-byte aggregate-by-value callable now lowers through the Win64 indirect ABI (" +
            lowerer.error() + ")");
    if (module != nullptr)
    {
        std::string verifyError;
        tests.expect(module->verify(verifyError),
            "indirect-aggregate module verifies clean (" + verifyError + ")");
        const std::string llvm = module->text();
        const std::size_t echoAt = llvm.find("define ");
        tests.expect(echoAt != std::string::npos &&
                llvm.find("sret([12 x i8])") != std::string::npos,
            "the Large return travels through an sret([12 x i8]) slot, not a direct value");
    }
}

void testPreciseIncrementalInvalidation(TestContext& tests, const std::filesystem::path& root)
{
    const auto firstDirectory = root / "IncrementalFirst";
    const auto changedDirectory = root / "IncrementalChanged";
    writeProject(firstDirectory, "IncrementalCache");
    writeProject(changedDirectory, "IncrementalCache");
    const std::string commonPrefix = R"NS(
public class Work
{
    public static int Alpha() { return 1; }
    public static int Beta() { return 2; }
    public static int Gamma() { return 3; }
    public static int Delta() { return 4; }
    public static int Epsilon() { return 5; }
    public static int Zeta() { return 6; }
)NS";
    writeFile(firstDirectory / "Work.ns", commonPrefix +
        "    public static int Changed() { return 7; }\n}\n");
    writeFile(changedDirectory / "Work.ns", commonPrefix +
        "    public static int Changed() { return 700; }\n}\n");

    ProjectCompilation first;
    first.setCompilationJobs(4);
    ProjectCompilation changed;
    changed.setCompilationJobs(4);
    std::string error;
    const bool firstOk = first.compileProjectOopMir(
        firstDirectory / "IncrementalCache.nsproj", error);
    std::string changedError;
    const bool changedOk = changed.compileProjectOopMir(
        changedDirectory / "IncrementalCache.nsproj", changedError);
    tests.expect(firstOk && changedOk,
        "body-only incremental fixtures both produce OOP MIR (" + error + changedError + ")");
    if (!firstOk || !changedOk || first.oopMirModel() == nullptr ||
        changed.oopMirModel() == nullptr)
        return;

    nativesharp::llvm_backend::ProjectOopCodePartition firstPartition;
    nativesharp::llvm_backend::ProjectOopCodePartition changedPartition;
    error.clear();
    const bool firstPartitionOk = firstPartition.build(
        *first.oopMirModel(), 4, error,
        nativesharp::llvm_backend::ProjectOopCodePartitionPolicy::StableIncremental);
    changedError.clear();
    const bool changedPartitionOk = changedPartition.build(
        *changed.oopMirModel(), 4, changedError,
        nativesharp::llvm_backend::ProjectOopCodePartitionPolicy::StableIncremental);
    tests.expect(firstPartitionOk && changedPartitionOk,
        "stable incremental partitions build for both revisions (" + error + changedError + ")");

    std::unordered_map<std::string, std::size_t> firstBuckets;
    const auto firstFunctions = first.oopMirModel()->functions();
    for (std::size_t ordinal = 0; ordinal < firstFunctions.size(); ++ordinal)
    {
        const auto bucket = firstPartition.unitForFunction(ordinal);
        if (bucket.has_value()) firstBuckets.emplace(firstFunctions[ordinal].mangledName, *bucket);
    }
    bool stableAssignments = firstFunctions.size() == changed.oopMirModel()->functions().size();
    const auto changedFunctions = changed.oopMirModel()->functions();
    for (std::size_t ordinal = 0; stableAssignments && ordinal < changedFunctions.size(); ++ordinal)
    {
        const auto bucket = changedPartition.unitForFunction(ordinal);
        const auto expected = firstBuckets.find(changedFunctions[ordinal].mangledName);
        stableAssignments = bucket.has_value() && expected != firstBuckets.end() &&
            *bucket == expected->second;
    }
    tests.expect(stableAssignments,
        "body changes preserve every physical function-to-bucket assignment");

    nativesharp::llvm_backend::ProjectOopCodePartition cacheAwareColdPartition;
    error.clear();
    const bool cacheAwareColdOk = cacheAwareColdPartition.build(
        *first.oopMirModel(), 4, error,
        nativesharp::llvm_backend::ProjectOopCodePartitionPolicy::CacheAwareBalanced);
    const std::string coldPlanPayload = cacheAwareColdPartition.persistentPlanPayload();
    tests.expect(cacheAwareColdOk && !coldPlanPayload.empty(),
        "cache-aware cold partition creates a balanced persistent plan (" + error + ")");

    nativesharp::llvm_backend::ProjectOopCodePartition referenceBalancedPartition;
    error.clear();
    const bool referenceBalancedOk = referenceBalancedPartition.build(
        *first.oopMirModel(), 4, error,
        nativesharp::llvm_backend::ProjectOopCodePartitionPolicy::Balanced);
    tests.expect(referenceBalancedOk &&
            referenceBalancedPartition.dump() == cacheAwareColdPartition.dump(),
        "cache-aware cold partition preserves the deterministic LPT balance (" + error + ")");

    nativesharp::llvm_backend::ProjectOopCodePartition invalidPlanFallback;
    error.clear();
    const bool invalidFallbackOk = invalidPlanFallback.build(
        *first.oopMirModel(), 4, error,
        nativesharp::llvm_backend::ProjectOopCodePartitionPolicy::CacheAwareBalanced,
        "invalid partition payload");
    tests.expect(invalidFallbackOk &&
            invalidPlanFallback.dump() == cacheAwareColdPartition.dump(),
        "an invalid private plan falls back to the same deterministic cold balance (" +
            error + ")");

    nativesharp::llvm_backend::CompilationArtifactCache cache;
    cache.setRoot(root / "incremental-object-cache");
    cache.setCodegenIdentity("precise-invalidation-test");
    nativesharp::llvm_backend::CompilationArtifactCacheScope cacheScope(cache);
    nativesharp::llvm_backend::ProjectOopObjectGenerator generator;
    error.clear();
    auto firstBundle = generator.generate(
        *first.nameLookup(), *first.memberModel(), *first.oopMirModel(),
        first.sourceManager(), nativesharp::llvm_backend::DebugInformationKind::None,
        nativesharp::llvm_backend::LlvmOptimizationLevel::O0, 4, {}, error);
    tests.expect(firstBundle != nullptr && firstBundle->cacheHitCount() == 0 &&
            firstBundle->cacheMissCount() == firstBundle->objectFiles().size(),
        "cold incremental build emits and stores every code unit (" + error + ")");
    if (firstBundle == nullptr) return;

    const std::string planKey =
        nativesharp::llvm_backend::ProjectOopCodePartition::persistentPlanKey(
            *first.oopMirModel(), 4);
    const std::string changedPlanKey =
        nativesharp::llvm_backend::ProjectOopCodePartition::persistentPlanKey(
            *changed.oopMirModel(), 4);
    tests.expect(planKey == changedPlanKey,
        "a body-only edit preserves the private code-partition topology key");
    std::string restoredPlanPayload;
    bool restoredPlanHit = false;
    error.clear();
    const bool restoredPlanOk = cache.restoreCodePartitionPlan(
        planKey, restoredPlanPayload, restoredPlanHit, error);
    tests.expect(restoredPlanOk && restoredPlanHit && !restoredPlanPayload.empty(),
        "cold generation atomically publishes its private code-partition plan (" + error + ")");

    nativesharp::llvm_backend::ProjectOopCodePartition restoredChangedPartition;
    changedError.clear();
    const bool restoredChangedOk = restoredChangedPartition.build(
        *changed.oopMirModel(), 4, changedError,
        nativesharp::llvm_backend::ProjectOopCodePartitionPolicy::CacheAwareBalanced,
        restoredPlanPayload);
    bool restoredAssignments = restoredChangedOk;
    std::unordered_map<std::string, std::size_t> cacheAwareBuckets;
    for (std::size_t ordinal = 0; ordinal < firstFunctions.size(); ++ordinal)
    {
        const auto bucket = cacheAwareColdPartition.unitForFunction(ordinal);
        if (bucket.has_value())
            cacheAwareBuckets.emplace(firstFunctions[ordinal].mangledName, *bucket);
    }
    for (std::size_t ordinal = 0;
        restoredAssignments && ordinal < changedFunctions.size(); ++ordinal)
    {
        const auto bucket = restoredChangedPartition.unitForFunction(ordinal);
        const auto expected = cacheAwareBuckets.find(changedFunctions[ordinal].mangledName);
        restoredAssignments = bucket.has_value() && expected != cacheAwareBuckets.end() &&
            *bucket == expected->second;
    }
    tests.expect(restoredAssignments,
        "a body-only edit restores the exact cold-balanced function-to-unit plan (" +
            changedError + ")");

    changedError.clear();
    auto changedBundle = generator.generate(
        *changed.nameLookup(), *changed.memberModel(), *changed.oopMirModel(),
        changed.sourceManager(), nativesharp::llvm_backend::DebugInformationKind::None,
        nativesharp::llvm_backend::LlvmOptimizationLevel::O0, 4, {}, changedError);
    tests.expect(changedBundle != nullptr && changedBundle->cacheMissCount() == 1 &&
            changedBundle->cacheHitCount() + changedBundle->cacheMissCount() ==
                changedBundle->objectFiles().size(),
        "one body edit invalidates exactly its code bucket while reusing coordinator and peers (" +
            changedError + ")");
    if (changedBundle == nullptr) return;

    changedError.clear();
    auto repeatedBundle = generator.generate(
        *changed.nameLookup(), *changed.memberModel(), *changed.oopMirModel(),
        changed.sourceManager(), nativesharp::llvm_backend::DebugInformationKind::None,
        nativesharp::llvm_backend::LlvmOptimizationLevel::O0, 4, {}, changedError);
    tests.expect(repeatedBundle != nullptr && repeatedBundle->cacheMissCount() == 0 &&
            repeatedBundle->cacheHitCount() == repeatedBundle->objectFiles().size(),
        "unchanged incremental rebuild restores every unit after the precise miss (" +
            changedError + ")");

    std::size_t evictedEntries = 0;
    std::uintmax_t evictedBytes = 0;
    error.clear();
    const bool pruneOk = cache.prune(0, evictedEntries, evictedBytes, error);
    std::string prunedPlanPayload;
    bool prunedPlanHit = true;
    const bool prunedPlanRestoreOk = cache.restoreCodePartitionPlan(
        planKey, prunedPlanPayload, prunedPlanHit, error);
    tests.expect(pruneOk && evictedEntries != 0U && evictedBytes != 0U &&
            prunedPlanRestoreOk && !prunedPlanHit,
        "the artifact-cache byte budget evicts private partition plans with objects (" +
            error + ")");
}

void testPartitionStaticFieldReachability(TestContext& tests, const std::filesystem::path& root)
{
    const std::filesystem::path directory = root / "PartitionStaticFieldReachability";
    writeProject(directory, "PartitionStaticFieldReachability");
    writeFile(directory / "Fields.ns", R"NS(
class Catalog
{
    public static int Used = 41;
    public static int Unused = 99;
    public static int Read() { return Used; }
    public static int Other() { return Unused; }
}
)NS");

    ProjectCompilation compilation;
    std::string error;
    const bool built = compilation.compileProjectOopMir(
        directory / "PartitionStaticFieldReachability.nsproj", error);
    tests.expect(built && compilation.oopMirModel() != nullptr,
        "static-field reachability fixture compiles through MIR (" + error + ")");
    if (!built || compilation.oopMirModel() == nullptr) return;

    const ProjectOopMirModel& mir = *compilation.oopMirModel();
    nativesharp::llvm_backend::ProjectOopCodePartition partition;
    tests.expect(partition.build(mir, 2, error),
        "static-field reachability fixture partitions into code units (" + error + ")");

    std::optional<std::size_t> readOrdinal;
    for (std::size_t ordinal = 0; ordinal < mir.functions().size(); ++ordinal)
        if (mir.functions()[ordinal].mangledName.find("Catalog__Read") != std::string::npos)
            readOrdinal = ordinal;
    const auto readUnit = readOrdinal.has_value()
        ? partition.unitForFunction(*readOrdinal) : std::nullopt;
    tests.expect(readUnit.has_value(), "Catalog.Read belongs to one stable code unit");
    if (!readUnit.has_value() || *readUnit >= partition.units().size()) return;

    const auto findField = [&](std::string_view name)
        -> const nativesharp::frontend::ProjectOopMirStaticField*
    {
        for (const auto& field : mir.staticFields())
            if (field.name == name) return &field;
        return nullptr;
    };
    const auto* used = findField("Used");
    const auto* unused = findField("Unused");
    tests.expect(used != nullptr && unused != nullptr,
        "fixture exposes distinct used and unused static fields in MIR");
    if (used == nullptr || unused == nullptr) return;

    const auto& unit = partition.units()[*readUnit];
    nativesharp::llvm_backend::ProjectOopLlvmLowerer lowerer;
    auto module = lowerer.lower(nativesharp::llvm_backend::ProjectOopLlvmLoweringRequest::partition(
        *compilation.nameLookup(), *compilation.memberModel(), mir,
        compilation.sourceManager(), nativesharp::llvm_backend::DebugInformationKind::None,
        nativesharp::llvm_backend::LlvmOptimizationLevel::O0,
        unit.functionOrdinals, false, "PartitionStaticFieldReachability.read", {},
        unit.declarationOrdinals));
    tests.expect(module != nullptr && lowerer.error().empty(),
        "the Read code unit lowers with its reachable static-field closure (" +
            lowerer.error() + ")");
    if (module == nullptr) return;
    std::string verificationError;
    tests.expect(module->verify(verificationError),
        "the pruned static-field code unit remains verifier-clean (" + verificationError + ")");
    const std::string ir = module->text();
    tests.expect(ir.find(used->storageSymbol) != std::string::npos &&
            ir.find(unused->storageSymbol) == std::string::npos &&
            ir.find(unused->accessorName) == std::string::npos,
        "a code unit declares the static field it reads and omits the unrelated field");
}

void testDeepOptionalDefaultHir(TestContext& tests, const std::filesystem::path& root)
{
    constexpr std::size_t unaryDepth = 32768;
    std::vector<std::unique_ptr<nativesharp::ast::AstNode>> syntax;
    syntax.reserve(unaryDepth + 1U);
    syntax.push_back(std::make_unique<nativesharp::ast::AstNode>(
        nativesharp::ast::AstNodeKind::IntegerLiteralExpression,
        nativesharp::SourceRange{}));
    syntax.back()->setIntegerValue(1U);
    nativesharp::ast::AstNode* current = syntax.back().get();
    for (std::size_t index = 0; index < unaryDepth; ++index)
    {
        syntax.push_back(std::make_unique<nativesharp::ast::AstNode>(
            nativesharp::ast::AstNodeKind::UnaryExpression,
            nativesharp::SourceRange{}));
        syntax.back()->setTokenKind(nativesharp::lexer::TokenKind::Minus);
        syntax.back()->setFirst(current);
        current = syntax.back().get();
    }

    ProjectOopHirModel model;
    std::string error;
    std::unique_ptr<nativesharp::frontend::ProjectOopHirNode> lowered =
        nativesharp::frontend::ProjectOopHirModelTestAccess::lowerDefaultArgument(
            model, current, "builtin:int", error);
    tests.expect(lowered != nullptr && error.empty(),
        "a 32768-level unary optional default lowers without process-stack recursion (" +
            error + ")");

    std::size_t observedDepth = 0;
    const nativesharp::frontend::ProjectOopHirNode* cursor = lowered.get();
    while (cursor != nullptr &&
        cursor->kind == nativesharp::frontend::ProjectOopHirNodeKind::Unary)
    {
        ++observedDepth;
        cursor = cursor->children.size() == 1U ? cursor->children.front().get() : nullptr;
    }
    tests.expect(observedDepth == unaryDepth && cursor != nullptr &&
            cursor->kind == nativesharp::frontend::ProjectOopHirNodeKind::IntegerConstant &&
            cursor->integerValue == 1U,
        "the iterative optional-default lowering preserves all 32768 unary nodes and the leaf");

    // Do not let unique_ptr destruction itself become the recursive operation this test excludes.
    while (lowered != nullptr)
    {
        std::unique_ptr<nativesharp::frontend::ProjectOopHirNode> child;
        if (!lowered->children.empty()) child = std::move(lowered->children.front());
        lowered->children.clear();
        lowered = std::move(child);
    }

    const std::filesystem::path directory = root / "OptionalDefaultSourceContract";
    writeProject(directory, "OptionalDefaultSourceContract");
    writeFile(directory / "Main.ns",
        "namespace Deep; public class Probe { "
        "public int Value(int input = -1) { return input; } "
        "public int Run() { return Value(); } }");
    ProjectCompilation compilation;
    error.clear();
    const bool built = compilation.compileProjectOopHir(
        directory / "OptionalDefaultSourceContract.nsproj", error);
    tests.expect(built && compilation.oopHirModel() != nullptr,
        "the language-permitted unary optional default still reaches OOP HIR (" + error + ")");
    if (!built || compilation.oopHirModel() == nullptr) return;

    std::size_t unaryCount = 0;
    std::vector<const nativesharp::frontend::ProjectOopHirNode*> pending;
    for (const auto& function : compilation.oopHirModel()->functions())
        if (function.body != nullptr) pending.push_back(function.body.get());
    while (!pending.empty())
    {
        const auto* node = pending.back();
        pending.pop_back();
        unaryCount += node->kind == nativesharp::frontend::ProjectOopHirNodeKind::Unary ? 1U : 0U;
        for (const auto& child : node->children)
            if (child != nullptr) pending.push_back(child.get());
    }
    tests.expect(unaryCount == 1U,
        "the permitted -1 default preserves exactly one unary HIR node at its call site");
}

void testDeepYieldStateAssignment(TestContext& tests)
{
    constexpr std::size_t nestingDepth = 32768;
    auto root = std::make_unique<nativesharp::frontend::ProjectOopHirNode>();
    root->kind = nativesharp::frontend::ProjectOopHirNodeKind::Block;
    nativesharp::frontend::ProjectOopHirNode* cursor = root.get();
    for (std::size_t index = 0; index < nestingDepth; ++index)
    {
        auto yield = std::make_unique<nativesharp::frontend::ProjectOopHirNode>();
        yield->kind = nativesharp::frontend::ProjectOopHirNodeKind::YieldValue;
        cursor->children.push_back(std::move(yield));
        if (index + 1U != nestingDepth)
        {
            auto block = std::make_unique<nativesharp::frontend::ProjectOopHirNode>();
            block->kind = nativesharp::frontend::ProjectOopHirNodeKind::Block;
            cursor->children.push_back(std::move(block));
            cursor = cursor->children.back().get();
        }
    }

    ProjectOopHirModel model;
    std::size_t nextYieldState = 0;
    nativesharp::frontend::ProjectOopHirModelTestAccess::assignYieldStates(
        model, root.get(), nextYieldState);
    tests.expect(nextYieldState == nestingDepth,
        "iterative async yield numbering reaches all 32768 nested blocks");

    bool ordered = true;
    cursor = root.get();
    for (std::size_t expected = 1; expected <= nestingDepth; ++expected)
    {
        ordered = ordered && !cursor->children.empty() &&
            cursor->children.front()->kind ==
                nativesharp::frontend::ProjectOopHirNodeKind::YieldValue &&
            cursor->children.front()->integerValue == expected;
        if (expected != nestingDepth)
        {
            ordered = ordered && cursor->children.size() == 2U;
            cursor = cursor->children.back().get();
        }
    }
    tests.expect(ordered,
        "iterative async yield numbering preserves recursive preorder exactly");

    while (root != nullptr)
    {
        std::unique_ptr<nativesharp::frontend::ProjectOopHirNode> next;
        if (root->children.size() == 2U) next = std::move(root->children.back());
        root->children.clear();
        root = std::move(next);
    }
}

void testDeepGeneratorThrowRewrite(TestContext& tests)
{
    constexpr std::size_t nestingDepth = 32768;
    auto root = std::make_unique<nativesharp::frontend::ProjectOopHirNode>();
    root->kind = nativesharp::frontend::ProjectOopHirNodeKind::Block;
    auto* cursor = root.get();
    for (std::size_t index = 0; index < nestingDepth; ++index)
    {
        auto block = std::make_unique<nativesharp::frontend::ProjectOopHirNode>();
        block->kind = nativesharp::frontend::ProjectOopHirNodeKind::Block;
        cursor->children.push_back(std::move(block));
        cursor = cursor->children.front().get();
    }
    auto throwing = std::make_unique<nativesharp::frontend::ProjectOopHirNode>();
    throwing->kind = nativesharp::frontend::ProjectOopHirNodeKind::Throw;
    auto payload = std::make_unique<nativesharp::frontend::ProjectOopHirNode>();
    payload->kind = nativesharp::frontend::ProjectOopHirNodeKind::NullConstant;
    payload->typeKey = "type:Deep.Error";
    throwing->children.push_back(std::move(payload));
    cursor->children.push_back(std::move(throwing));

    ProjectOopHirModel model;
    nativesharp::frontend::ProjectOopHirModelTestAccess::rewriteGeneratorThrows(
        model, root.get(), "type:Deep.Error", "iterator:builtin:int:type:Deep.Error");
    tests.expect(cursor->children.size() == 1U &&
            cursor->children.front()->kind ==
                nativesharp::frontend::ProjectOopHirNodeKind::Block &&
            cursor->children.front()->children.size() == 3U &&
            cursor->children.front()->children.back()->kind ==
                nativesharp::frontend::ProjectOopHirNodeKind::Return,
        "iterative generator throw rewrite reaches and replaces a node at depth 32768");

    std::size_t removedDepth = 0;
    while (root != nullptr && root->children.size() == 1U &&
        root->children.front()->kind == nativesharp::frontend::ProjectOopHirNodeKind::Block &&
        root->children.front()->children.size() == 1U)
    {
        auto next = std::move(root->children.front());
        root->children.clear();
        root = std::move(next);
        ++removedDepth;
    }
    tests.expect(removedDepth == nestingDepth,
        "the deep generator throw fixture retains every enclosing block");
}

void testDeepAsyncPollReturnRewrite(TestContext& tests)
{
    constexpr std::size_t nestingDepth = 32768;
    auto root = std::make_unique<nativesharp::frontend::ProjectOopHirNode>();
    root->kind = nativesharp::frontend::ProjectOopHirNodeKind::Block;
    auto* cursor = root.get();
    for (std::size_t index = 0; index < nestingDepth; ++index)
    {
        auto block = std::make_unique<nativesharp::frontend::ProjectOopHirNode>();
        block->kind = nativesharp::frontend::ProjectOopHirNodeKind::Block;
        cursor->children.push_back(std::move(block));
        cursor = cursor->children.front().get();
    }
    auto returning = std::make_unique<nativesharp::frontend::ProjectOopHirNode>();
    returning->kind = nativesharp::frontend::ProjectOopHirNodeKind::Return;
    returning->cleanupRegionOrdinals = {7U, 11U};
    returning->cleanupContinuationKind =
        nativesharp::frontend::MethodCleanupContinuationKind::Return;
    auto payload = std::make_unique<nativesharp::frontend::ProjectOopHirNode>();
    payload->kind = nativesharp::frontend::ProjectOopHirNodeKind::IntegerConstant;
    payload->typeKey = "builtin:int";
    payload->integerValue = 42U;
    returning->children.push_back(std::move(payload));
    cursor->children.push_back(std::move(returning));

    ProjectOopHirModel model;
    std::string error;
    root = nativesharp::frontend::ProjectOopHirModelTestAccess::rewriteAsyncPollReturns(
        model,
        std::move(root),
        3U,
        "builtin:int",
        "future:builtin:int",
        error,
        true);
    tests.expect(root != nullptr && error.empty(),
        "a 32768-level async poll return rewrites without process-stack recursion (" + error + ")");

    cursor = root.get();
    bool enclosingBlocksPreserved = cursor != nullptr;
    for (std::size_t index = 0; index < nestingDepth && enclosingBlocksPreserved; ++index)
    {
        enclosingBlocksPreserved = !cursor->children.empty() &&
            cursor->children.front() != nullptr &&
            cursor->children.front()->kind ==
                nativesharp::frontend::ProjectOopHirNodeKind::Block;
        if (enclosingBlocksPreserved) cursor = cursor->children.front().get();
    }
    tests.expect(enclosingBlocksPreserved && cursor != nullptr &&
            cursor->children.size() == 1U &&
            cursor->children.front()->kind ==
                nativesharp::frontend::ProjectOopHirNodeKind::Block &&
            cursor->children.front()->children.size() == 3U &&
            cursor->children.front()->children.back()->kind ==
                nativesharp::frontend::ProjectOopHirNodeKind::Return &&
            cursor->children.front()->children.back()->cleanupRegionOrdinals ==
                std::vector<std::size_t>({7U, 11U}) &&
            cursor->children.front()->children.back()->cleanupContinuationKind ==
                nativesharp::frontend::MethodCleanupContinuationKind::Return,
        "iterative async poll return rewrite preserves every block and cleanup continuation");

    tests.expect(root != nullptr && root->children.size() == 2U &&
            root->children.back()->kind == nativesharp::frontend::ProjectOopHirNodeKind::Block &&
            root->children.back()->children.size() == 3U &&
            root->children.back()->children.back()->kind ==
                nativesharp::frontend::ProjectOopHirNodeKind::Return,
        "top-level fallthrough still appends exactly one implicit ready return");
    destroyHirTreeIteratively(root);

    auto throwRoot = std::make_unique<nativesharp::frontend::ProjectOopHirNode>();
    throwRoot->kind = nativesharp::frontend::ProjectOopHirNodeKind::Block;
    cursor = throwRoot.get();
    for (std::size_t index = 0; index < nestingDepth; ++index)
    {
        auto block = std::make_unique<nativesharp::frontend::ProjectOopHirNode>();
        block->kind = nativesharp::frontend::ProjectOopHirNodeKind::Block;
        cursor->children.push_back(std::move(block));
        cursor = cursor->children.front().get();
    }
    auto throwing = std::make_unique<nativesharp::frontend::ProjectOopHirNode>();
    throwing->kind = nativesharp::frontend::ProjectOopHirNodeKind::Throw;
    throwing->cleanupRegionOrdinals = {13U};
    throwing->cleanupContinuationKind =
        nativesharp::frontend::MethodCleanupContinuationKind::Exception;
    auto errorPayload = std::make_unique<nativesharp::frontend::ProjectOopHirNode>();
    errorPayload->kind = nativesharp::frontend::ProjectOopHirNodeKind::NullConstant;
    errorPayload->typeKey = "type:Deep.Error";
    throwing->children.push_back(std::move(errorPayload));
    cursor->children.push_back(std::move(throwing));

    error.clear();
    throwRoot = nativesharp::frontend::ProjectOopHirModelTestAccess::rewriteAsyncPollReturns(
        model,
        std::move(throwRoot),
        3U,
        "builtin:int",
        "future:builtin:int",
        error,
        false);
    tests.expect(throwRoot != nullptr && error.empty(),
        "a 32768-level async poll throw rewrites without process-stack recursion (" + error + ")");

    cursor = throwRoot.get();
    for (std::size_t index = 0; index < nestingDepth; ++index)
        cursor = cursor->children.front().get();
    const auto* errorBlock = cursor->children.front().get();
    tests.expect(errorBlock->kind == nativesharp::frontend::ProjectOopHirNodeKind::Block &&
            errorBlock->children.size() == 3U &&
            errorBlock->children[0]->kind ==
                nativesharp::frontend::ProjectOopHirNodeKind::Assignment &&
            errorBlock->children[0]->children[0]->memberOrdinal == 4U &&
            errorBlock->children[0]->children[1]->typeKey == "type:Deep.Error" &&
            errorBlock->children[1]->children[1]->integerValue == (~0ULL - 1ULL) &&
            errorBlock->children[2]->cleanupRegionOrdinals == std::vector<std::size_t>({13U}) &&
            errorBlock->children[2]->cleanupContinuationKind ==
                nativesharp::frontend::MethodCleanupContinuationKind::Exception,
        "iterative async poll throw rewrite preserves error field, state -2, payload, and cleanup");
    destroyHirTreeIteratively(throwRoot);
}

void testDeepAsyncAwaitScan(TestContext& tests)
{
    constexpr std::size_t nestingDepth = 32768;
    std::vector<std::unique_ptr<nativesharp::ast::AstNode>> syntax;
    syntax.reserve(nestingDepth + 2U);
    syntax.push_back(std::make_unique<nativesharp::ast::AstNode>(
        nativesharp::ast::AstNodeKind::IdentifierExpression,
        nativesharp::SourceRange{}));
    nativesharp::ast::AstNode* callSyntax = syntax.back().get();
    syntax.push_back(std::make_unique<nativesharp::ast::AstNode>(
        nativesharp::ast::AstNodeKind::AwaitExpression,
        nativesharp::SourceRange{}));
    syntax.back()->setFirst(callSyntax);
    nativesharp::ast::AstNode* current = syntax.back().get();
    for (std::size_t index = 0; index < nestingDepth; ++index)
    {
        syntax.push_back(std::make_unique<nativesharp::ast::AstNode>(
            nativesharp::ast::AstNodeKind::UnaryExpression,
            nativesharp::SourceRange{}));
        syntax.back()->setFirst(current);
        current = syntax.back().get();
    }

    nativesharp::frontend::MethodCallBinding binding;
    binding.selectedMemberOrdinal = 17U;
    ProjectOopHirModel model;
    const auto ordinals =
        nativesharp::frontend::ProjectOopHirModelTestAccess::collectAwaitedMemberOrdinals(
            model, current, callSyntax, binding);
    tests.expect(ordinals == std::vector<std::size_t>({17U}),
        "iterative async await scan reaches a bound await through 32768 AST levels");
}

void testDeepBuiltinUnaryExpressionLowering(TestContext& tests)
{
    constexpr std::size_t nestingDepth = 32768;
    std::vector<std::unique_ptr<nativesharp::ast::AstNode>> syntax;
    syntax.reserve(nestingDepth + 1U);
    syntax.push_back(std::make_unique<nativesharp::ast::AstNode>(
        nativesharp::ast::AstNodeKind::IntegerLiteralExpression,
        nativesharp::SourceRange{}));
    syntax.back()->setIntegerValue(1U);
    nativesharp::ast::AstNode* current = syntax.back().get();
    for (std::size_t index = 0; index < nestingDepth; ++index)
    {
        syntax.push_back(std::make_unique<nativesharp::ast::AstNode>(
            nativesharp::ast::AstNodeKind::UnaryExpression,
            nativesharp::SourceRange{}));
        syntax.back()->setTokenKind(nativesharp::lexer::TokenKind::Minus);
        syntax.back()->setFirst(current);
        current = syntax.back().get();
    }

    nativesharp::frontend::MethodExpressionTypeBinding binding;
    binding.typeKey = "builtin:int";
    ProjectOopHirModel model;
    std::string error;
    auto lowered = nativesharp::frontend::ProjectOopHirModelTestAccess::lowerExpression(
        model, current, syntax, binding, error);
    tests.expect(lowered != nullptr && error.empty(),
        "a 32768-level builtin unary expression lowers without process-stack recursion (" +
            error + ")");

    std::size_t observedDepth = 0;
    auto* cursor = lowered.get();
    while (cursor != nullptr &&
        cursor->kind == nativesharp::frontend::ProjectOopHirNodeKind::Unary)
    {
        ++observedDepth;
        cursor = cursor->children.empty() ? nullptr : cursor->children.front().get();
    }
    tests.expect(observedDepth == nestingDepth && cursor != nullptr &&
            cursor->kind == nativesharp::frontend::ProjectOopHirNodeKind::IntegerConstant,
        "iterative builtin unary lowering preserves every operator and its literal leaf");
    destroyHirTreeIteratively(lowered);
}

void testDeepConversionExpressionLowering(TestContext& tests)
{
    constexpr std::size_t nestingDepth = 32768;
    std::vector<std::unique_ptr<nativesharp::ast::AstNode>> syntax;
    syntax.reserve(nestingDepth + 1U);
    syntax.push_back(std::make_unique<nativesharp::ast::AstNode>(
        nativesharp::ast::AstNodeKind::IntegerLiteralExpression,
        nativesharp::SourceRange{}));
    syntax.back()->setIntegerValue(1U);
    nativesharp::ast::AstNode* current = syntax.back().get();
    for (std::size_t index = 0; index < nestingDepth; ++index)
    {
        syntax.push_back(std::make_unique<nativesharp::ast::AstNode>(
            nativesharp::ast::AstNodeKind::CastExpression,
            nativesharp::SourceRange{}));
        syntax.back()->setFirst(current);
        current = syntax.back().get();
    }

    nativesharp::frontend::MethodExpressionTypeBinding binding;
    binding.typeKey = "builtin:int";
    ProjectOopHirModel model;
    std::string error;
    auto lowered = nativesharp::frontend::ProjectOopHirModelTestAccess::lowerExpression(
        model, current, syntax, binding, error);
    tests.expect(lowered != nullptr && error.empty(),
        "a 32768-level conversion chain lowers without process-stack recursion (" +
            error + ")");

    std::size_t observedDepth = 0;
    auto* cursor = lowered.get();
    while (cursor != nullptr &&
        cursor->kind == nativesharp::frontend::ProjectOopHirNodeKind::Conversion)
    {
        ++observedDepth;
        cursor = cursor->children.empty() ? nullptr : cursor->children.front().get();
    }
    tests.expect(observedDepth == nestingDepth && cursor != nullptr &&
            cursor->kind == nativesharp::frontend::ProjectOopHirNodeKind::IntegerConstant,
        "iterative conversion lowering preserves every cast and its literal leaf");
    destroyHirTreeIteratively(lowered);

    const auto checkRuntime = [&](
        nativesharp::ast::AstNodeKind syntaxKind,
        nativesharp::frontend::MethodRuntimeTypeOperationKind operation,
        nativesharp::frontend::ProjectOopHirNodeKind expectedKind,
        std::string resultType)
    {
        std::vector<std::unique_ptr<nativesharp::ast::AstNode>> runtimeSyntax;
        runtimeSyntax.push_back(std::make_unique<nativesharp::ast::AstNode>(
            nativesharp::ast::AstNodeKind::IntegerLiteralExpression,
            nativesharp::SourceRange{}));
        runtimeSyntax.back()->setIntegerValue(9U);
        auto* runtimeLiteral = runtimeSyntax.back().get();
        runtimeSyntax.push_back(std::make_unique<nativesharp::ast::AstNode>(
            syntaxKind,
            nativesharp::SourceRange{}));
        runtimeSyntax.back()->setFirst(runtimeLiteral);
        auto* runtimeExpression = runtimeSyntax.back().get();

        nativesharp::frontend::MethodExpressionTypeBinding runtimeType;
        runtimeType.typeKey = std::move(resultType);
        nativesharp::frontend::MethodRuntimeTypeBinding runtimeBinding;
        runtimeBinding.kind = operation;
        runtimeBinding.syntax = runtimeExpression;
        runtimeBinding.targetTypeOrdinal = 37U;
        runtimeBinding.targetIsInterface = true;
        ProjectOopHirModel runtimeModel;
        nativesharp::frontend::ProjectOopHirModelTestAccess::setRuntimeTypeBinding(
            runtimeModel, runtimeExpression, runtimeBinding);
        std::string runtimeError;
        auto runtimeLowered =
            nativesharp::frontend::ProjectOopHirModelTestAccess::lowerExpression(
                runtimeModel,
                runtimeExpression,
                runtimeSyntax,
                runtimeType,
                runtimeError);
        return runtimeLowered != nullptr && runtimeError.empty() &&
            runtimeLowered->kind == expectedKind &&
            runtimeLowered->targetTypeOrdinal == 37U &&
            runtimeLowered->targetIsInterface &&
            runtimeLowered->children.size() == 1U &&
            runtimeLowered->children.front()->kind ==
                nativesharp::frontend::ProjectOopHirNodeKind::IntegerConstant;
    };
    tests.expect(checkRuntime(
            nativesharp::ast::AstNodeKind::TypeTestExpression,
            nativesharp::frontend::MethodRuntimeTypeOperationKind::TypeTest,
            nativesharp::frontend::ProjectOopHirNodeKind::RuntimeTypeTest,
            "builtin:bool"),
        "iterative type-test lowering preserves runtime target metadata");
    tests.expect(checkRuntime(
            nativesharp::ast::AstNodeKind::SafeCastExpression,
            nativesharp::frontend::MethodRuntimeTypeOperationKind::SafeCast,
            nativesharp::frontend::ProjectOopHirNodeKind::SafeReferenceCast,
            "type:Target"),
        "iterative safe-cast lowering preserves runtime target metadata");
    tests.expect(checkRuntime(
            nativesharp::ast::AstNodeKind::CastExpression,
            nativesharp::frontend::MethodRuntimeTypeOperationKind::CheckedCast,
            nativesharp::frontend::ProjectOopHirNodeKind::CheckedReferenceCast,
            "type:Target"),
        "iterative checked-cast lowering preserves runtime target metadata");

    std::vector<std::unique_ptr<nativesharp::ast::AstNode>> missingBindingSyntax;
    missingBindingSyntax.push_back(std::make_unique<nativesharp::ast::AstNode>(
        nativesharp::ast::AstNodeKind::TypeTestExpression,
        nativesharp::SourceRange{}));
    nativesharp::frontend::MethodExpressionTypeBinding missingBindingType;
    missingBindingType.typeKey = "builtin:bool";
    ProjectOopHirModel missingBindingModel;
    std::string missingBindingError;
    auto missingBinding =
        nativesharp::frontend::ProjectOopHirModelTestAccess::lowerExpression(
            missingBindingModel,
            missingBindingSyntax.front().get(),
            missingBindingSyntax,
            missingBindingType,
            missingBindingError);
    tests.expect(missingBinding == nullptr &&
            missingBindingError ==
                "Stage 39 runtime type operation lost its semantic binding",
        "missing runtime binding fails before visiting the conversion operand");
}

void testDeepClassFieldExpressionLowering(TestContext& tests)
{
    constexpr std::size_t nestingDepth = 32768;
    const std::string nodeType = "type:Deep::Node";
    std::vector<std::unique_ptr<nativesharp::ast::AstNode>> syntax;
    syntax.reserve(nestingDepth + 1U);
    syntax.push_back(std::make_unique<nativesharp::ast::AstNode>(
        nativesharp::ast::AstNodeKind::ThisExpression,
        nativesharp::SourceRange{}));
    nativesharp::ast::AstNode* current = syntax.back().get();
    std::vector<nativesharp::frontend::MethodNameBinding> names;
    names.reserve(nestingDepth);
    for (std::size_t index = 0; index < nestingDepth; ++index)
    {
        syntax.push_back(std::make_unique<nativesharp::ast::AstNode>(
            nativesharp::ast::AstNodeKind::MemberExpression,
            nativesharp::SourceRange{}));
        syntax.back()->setFirst(current);
        names.emplace_back();
        names.back().kind = nativesharp::frontend::MethodNameBindingKind::ExplicitObjectField;
        names.back().syntax = syntax.back().get();
        names.back().typeKey = nodeType;
        names.back().candidateMemberOrdinals.push_back(0U);
        names.back().receiverSyntax = current;
        names.back().receiverTypeKey = nodeType;
        current = syntax.back().get();
    }

    nativesharp::frontend::MethodExpressionTypeBinding type;
    type.typeKey = nodeType;
    ProjectOopHirModel model;
    std::string error;
    auto lowered = nativesharp::frontend::ProjectOopHirModelTestAccess::lowerFieldExpression(
        model, current, syntax, names, type, false, error);
    tests.expect(lowered != nullptr && error.empty(),
        "a 32768-level class-field chain lowers without process-stack recursion (" + error + ")");

    std::size_t observedDepth = 0;
    bool fieldsPreserved = true;
    auto* cursor = lowered.get();
    while (cursor != nullptr &&
        cursor->kind == nativesharp::frontend::ProjectOopHirNodeKind::FieldValue)
    {
        fieldsPreserved = fieldsPreserved && cursor->typeKey == nodeType &&
            cursor->memberOrdinal == 0U && !cursor->volatileAccess &&
            !cursor->baseReceiver && cursor->children.size() == 1U;
        ++observedDepth;
        cursor = cursor->children.empty() ? nullptr : cursor->children.front().get();
    }
    tests.expect(observedDepth == nestingDepth && fieldsPreserved && cursor != nullptr &&
            cursor->kind == nativesharp::frontend::ProjectOopHirNodeKind::ThisValue,
        "iterative class-field lowering preserves every field and its this receiver");
    destroyHirTreeIteratively(lowered);

    ProjectOopHirModel addressModel;
    std::string addressError;
    auto address = nativesharp::frontend::ProjectOopHirModelTestAccess::lowerFieldExpression(
        addressModel, current, syntax, names, type, true, addressError);
    tests.expect(address != nullptr && addressError.empty() &&
            address->kind == nativesharp::frontend::ProjectOopHirNodeKind::FieldAddress &&
            address->children.size() == 1U &&
            address->children.front()->kind ==
                nativesharp::frontend::ProjectOopHirNodeKind::FieldValue,
        "address context changes only the outer class field and preserves value receivers");
    destroyHirTreeIteratively(address);
}

bool preparePropertyGetterFixture(
    const std::filesystem::path& root,
    ProjectCompilation& compilation,
    std::size_t& getterOrdinal,
    std::size_t& callerOrdinal,
    std::string& nodeType,
    std::string& error)
{
    const auto directory = root / "DeepPropertyGetter";
    writeProject(directory, "DeepPropertyGetter");
    writeFile(
        directory / "Main.ns",
        "namespace Deep; public class Node { "
        "public Node Next { get { return this; } } "
        "public Node Probe(Node node) { return node.Next; } }");
    if (!compilation.compileProjectOopHir(directory / "DeepPropertyGetter.nsproj", error) ||
        compilation.memberModel() == nullptr)
    {
        return false;
    }
    bool foundGetter = false;
    bool foundCaller = false;
    const auto members = compilation.memberModel()->members();
    for (std::size_t ordinal = 0; ordinal < members.size(); ++ordinal)
    {
        const auto& member = members[ordinal];
        if (member.isPropertyAccessor && !member.isIndexerAccessor && member.isGetter &&
            member.associatedName == "Next")
        {
            getterOrdinal = ordinal;
            nodeType = member.declaredTypeKey;
            foundGetter = true;
        }
        if (!member.isPropertyAccessor && member.name == "Probe")
        {
            callerOrdinal = ordinal;
            foundCaller = true;
        }
    }
    return foundGetter && foundCaller && !nodeType.empty();
}

struct DeepPropertyGetterSyntax final
{
    std::vector<std::unique_ptr<nativesharp::ast::AstNode>> nodes;
    std::vector<nativesharp::frontend::MethodCallBinding> calls;
    const nativesharp::ast::AstNode* root = nullptr;
};

DeepPropertyGetterSyntax makeDeepPropertyGetterSyntax(
    std::size_t depth,
    std::size_t getterOrdinal,
    const std::string& nodeType)
{
    DeepPropertyGetterSyntax fixture;
    fixture.nodes.reserve(depth + 1U);
    fixture.calls.reserve(depth);
    fixture.nodes.push_back(std::make_unique<nativesharp::ast::AstNode>(
        nativesharp::ast::AstNodeKind::ThisExpression, nativesharp::SourceRange{}));
    auto* current = fixture.nodes.back().get();
    for (std::size_t index = 0; index < depth; ++index)
    {
        fixture.nodes.push_back(std::make_unique<nativesharp::ast::AstNode>(
            nativesharp::ast::AstNodeKind::MemberExpression, nativesharp::SourceRange{}));
        fixture.nodes.back()->setFirst(current);
        fixture.calls.emplace_back();
        auto& call = fixture.calls.back();
        call.resolution = nativesharp::frontend::MethodCallResolutionKind::Resolved;
        call.syntax = fixture.nodes.back().get();
        call.receiverSyntax = current;
        call.receiverTypeKey = nodeType;
        call.selectedMemberOrdinal = getterOrdinal;
        call.resultTypeKey = nodeType;
        current = fixture.nodes.back().get();
    }
    fixture.root = current;
    return fixture;
}

bool propertyGetterChainMatches(
    const nativesharp::frontend::ProjectOopHirNode* node,
    std::size_t expectedDepth,
    std::size_t getterOrdinal,
    const std::string& nodeType)
{
    std::size_t observedDepth = 0;
    while (node != nullptr &&
        node->kind == nativesharp::frontend::ProjectOopHirNodeKind::InstanceCall)
    {
        if (node->memberOrdinal != getterOrdinal || node->typeKey != nodeType ||
            node->children.size() != 1U || node->interfaceDispatch || node->nullConditionalCall)
        {
            return false;
        }
        ++observedDepth;
        node = node->children.front().get();
    }
    return observedDepth == expectedDepth && node != nullptr &&
        node->kind == nativesharp::frontend::ProjectOopHirNodeKind::ThisValue;
}

void testDeepPropertyGetterExpressionLowering(
    TestContext& tests,
    const std::filesystem::path& root)
{
    constexpr std::size_t nestingDepth = 32768;
    ProjectCompilation compilation;
    std::size_t getterOrdinal = 0;
    std::size_t callerOrdinal = 0;
    std::string nodeType;
    std::string error;
    const bool prepared = preparePropertyGetterFixture(
        root, compilation, getterOrdinal, callerOrdinal, nodeType, error);
    tests.expect(prepared, "the property-getter depth fixture builds semantic accessors (" +
        error + ")");
    if (!prepared) return;

    DeepPropertyGetterSyntax syntax =
        makeDeepPropertyGetterSyntax(nestingDepth, getterOrdinal, nodeType);
    nativesharp::frontend::MethodExpressionTypeBinding type;
    type.typeKey = nodeType;
    ProjectOopHirModel model;
    auto lowered = nativesharp::frontend::ProjectOopHirModelTestAccess::
        lowerPropertyGetterExpression(
            model,
            *compilation.memberModel(),
            callerOrdinal,
            syntax.root,
            syntax.nodes,
            syntax.calls,
            type,
            error);
    tests.expect(lowered != nullptr && error.empty(),
        "a 32768-level property-getter chain lowers without process-stack recursion (" +
            error + ")");
    tests.expect(propertyGetterChainMatches(
            lowered.get(), nestingDepth, getterOrdinal, nodeType),
        "iterative property-getter lowering preserves every call and receiver in source order");
    destroyHirTreeIteratively(lowered);
}

void testDeepOverflowPolicyExpressionLowering(TestContext& tests)
{
    constexpr std::size_t nestingDepth = 32768;
    std::vector<std::unique_ptr<nativesharp::ast::AstNode>> syntax;
    syntax.reserve(nestingDepth + 1U);
    syntax.push_back(std::make_unique<nativesharp::ast::AstNode>(
        nativesharp::ast::AstNodeKind::IntegerLiteralExpression,
        nativesharp::SourceRange{}));
    syntax.back()->setIntegerValue(1U);
    nativesharp::ast::AstNode* current = syntax.back().get();
    for (std::size_t index = 0; index < nestingDepth; ++index)
    {
        syntax.push_back(std::make_unique<nativesharp::ast::AstNode>(
            nativesharp::ast::AstNodeKind::OverflowPolicyExpression,
            nativesharp::SourceRange{}));
        syntax.back()->setAuxiliaryText(index % 2U == 0U ? "checked" : "unchecked");
        syntax.back()->setFirst(current);
        current = syntax.back().get();
    }

    nativesharp::frontend::MethodExpressionTypeBinding binding;
    binding.typeKey = "builtin:int";
    ProjectOopHirModel model;
    std::string error;
    auto lowered = nativesharp::frontend::ProjectOopHirModelTestAccess::lowerExpression(
        model, current, syntax, binding, error);
    tests.expect(lowered != nullptr && error.empty(),
        "a 32768-level overflow-policy expression lowers without process-stack recursion (" +
            error + ")");

    std::size_t observedDepth = 0;
    bool policiesPreserved = true;
    auto* cursor = lowered.get();
    while (cursor != nullptr &&
        cursor->kind == nativesharp::frontend::ProjectOopHirNodeKind::OverflowPolicy)
    {
        const std::size_t sourceIndex = nestingDepth - observedDepth - 1U;
        policiesPreserved = policiesPreserved && cursor->stringValue ==
            (sourceIndex % 2U == 0U ? "checked" : "unchecked");
        ++observedDepth;
        cursor = cursor->children.empty() ? nullptr : cursor->children.front().get();
    }
    tests.expect(observedDepth == nestingDepth && policiesPreserved && cursor != nullptr &&
            cursor->kind == nativesharp::frontend::ProjectOopHirNodeKind::IntegerConstant,
        "iterative overflow-policy lowering preserves every policy and its literal leaf");
    destroyHirTreeIteratively(lowered);
}

void testDeepBuiltinBinaryExpressionLowering(TestContext& tests)
{
    constexpr std::size_t nestingDepth = 32768;
    std::vector<std::unique_ptr<nativesharp::ast::AstNode>> syntax;
    syntax.reserve(nestingDepth * 2U + 1U);
    syntax.push_back(std::make_unique<nativesharp::ast::AstNode>(
        nativesharp::ast::AstNodeKind::IntegerLiteralExpression,
        nativesharp::SourceRange{}));
    syntax.back()->setIntegerValue(1U);
    nativesharp::ast::AstNode* current = syntax.back().get();
    for (std::size_t index = 0; index < nestingDepth; ++index)
    {
        syntax.push_back(std::make_unique<nativesharp::ast::AstNode>(
            nativesharp::ast::AstNodeKind::IntegerLiteralExpression,
            nativesharp::SourceRange{}));
        syntax.back()->setIntegerValue(index + 2U);
        nativesharp::ast::AstNode* right = syntax.back().get();
        syntax.push_back(std::make_unique<nativesharp::ast::AstNode>(
            nativesharp::ast::AstNodeKind::BinaryExpression,
            nativesharp::SourceRange{}));
        syntax.back()->setTokenKind(nativesharp::lexer::TokenKind::Plus);
        syntax.back()->setFirst(current);
        syntax.back()->setSecond(right);
        current = syntax.back().get();
    }

    nativesharp::frontend::MethodExpressionTypeBinding binding;
    binding.typeKey = "builtin:int";
    ProjectOopHirModel model;
    std::string error;
    auto lowered = nativesharp::frontend::ProjectOopHirModelTestAccess::lowerExpression(
        model, current, syntax, binding, error);
    tests.expect(lowered != nullptr && error.empty(),
        "a 32768-level builtin binary expression lowers without process-stack recursion (" +
            error + ")");

    std::size_t observedDepth = 0;
    bool rightOperandsPreserved = true;
    auto* cursor = lowered.get();
    while (cursor != nullptr &&
        cursor->kind == nativesharp::frontend::ProjectOopHirNodeKind::Binary)
    {
        rightOperandsPreserved = rightOperandsPreserved && cursor->children.size() == 2U &&
            cursor->children[1]->kind ==
                nativesharp::frontend::ProjectOopHirNodeKind::IntegerConstant &&
            cursor->operation == nativesharp::lexer::TokenKind::Plus;
        ++observedDepth;
        cursor = cursor->children.empty() ? nullptr : cursor->children.front().get();
    }
    tests.expect(observedDepth == nestingDepth && rightOperandsPreserved && cursor != nullptr &&
            cursor->kind == nativesharp::frontend::ProjectOopHirNodeKind::IntegerConstant,
        "iterative builtin binary lowering preserves postorder, both operands, and every operator");
    destroyHirTreeIteratively(lowered);
}

void testDeepConditionalExpressionLowering(TestContext& tests)
{
    constexpr std::size_t nestingDepth = 32768;
    std::vector<std::unique_ptr<nativesharp::ast::AstNode>> syntax;
    syntax.reserve(nestingDepth * 3U + 1U);
    syntax.push_back(std::make_unique<nativesharp::ast::AstNode>(
        nativesharp::ast::AstNodeKind::IntegerLiteralExpression,
        nativesharp::SourceRange{}));
    syntax.back()->setIntegerValue(0U);
    nativesharp::ast::AstNode* current = syntax.back().get();
    for (std::size_t index = 0; index < nestingDepth; ++index)
    {
        syntax.push_back(std::make_unique<nativesharp::ast::AstNode>(
            nativesharp::ast::AstNodeKind::BooleanLiteralExpression,
            nativesharp::SourceRange{}));
        syntax.back()->setTokenKind(nativesharp::lexer::TokenKind::KwTrue);
        nativesharp::ast::AstNode* condition = syntax.back().get();
        syntax.push_back(std::make_unique<nativesharp::ast::AstNode>(
            nativesharp::ast::AstNodeKind::IntegerLiteralExpression,
            nativesharp::SourceRange{}));
        syntax.back()->setIntegerValue(index + 1U);
        nativesharp::ast::AstNode* whenTrue = syntax.back().get();
        syntax.push_back(std::make_unique<nativesharp::ast::AstNode>(
            nativesharp::ast::AstNodeKind::ConditionalExpression,
            nativesharp::SourceRange{}));
        syntax.back()->setFirst(condition);
        syntax.back()->setSecond(whenTrue);
        syntax.back()->setThird(current);
        current = syntax.back().get();
    }

    nativesharp::frontend::MethodExpressionTypeBinding binding;
    binding.typeKey = "builtin:int";
    ProjectOopHirModel model;
    std::string error;
    auto lowered = nativesharp::frontend::ProjectOopHirModelTestAccess::lowerExpression(
        model, current, syntax, binding, error);
    tests.expect(lowered != nullptr && error.empty(),
        "a 32768-level conditional expression lowers without process-stack recursion (" +
            error + ")");

    std::size_t observedDepth = 0;
    bool branchesPreserved = true;
    auto* cursor = lowered.get();
    while (cursor != nullptr &&
        cursor->kind == nativesharp::frontend::ProjectOopHirNodeKind::Conditional)
    {
        branchesPreserved = branchesPreserved && cursor->children.size() == 3U &&
            cursor->children[0]->kind ==
                nativesharp::frontend::ProjectOopHirNodeKind::BooleanConstant &&
            cursor->children[1]->kind ==
                nativesharp::frontend::ProjectOopHirNodeKind::IntegerConstant;
        ++observedDepth;
        cursor = cursor->children.empty() ? nullptr : cursor->children[2].get();
    }
    tests.expect(observedDepth == nestingDepth && branchesPreserved && cursor != nullptr &&
            cursor->kind == nativesharp::frontend::ProjectOopHirNodeKind::IntegerConstant,
        "iterative conditional lowering preserves condition, both branches, and common type");
    destroyHirTreeIteratively(lowered);
}

void testDeepMixedExpressionLowering(TestContext& tests)
{
    constexpr std::size_t nestingDepth = 32768;
    std::vector<std::unique_ptr<nativesharp::ast::AstNode>> syntax;
    syntax.reserve(nestingDepth * 2U + 1U);
    syntax.push_back(std::make_unique<nativesharp::ast::AstNode>(
        nativesharp::ast::AstNodeKind::IntegerLiteralExpression,
        nativesharp::SourceRange{}));
    syntax.back()->setIntegerValue(1U);
    nativesharp::ast::AstNode* current = syntax.back().get();
    for (std::size_t index = 0; index < nestingDepth; ++index)
    {
        if (index % 3U == 0U)
        {
            syntax.push_back(std::make_unique<nativesharp::ast::AstNode>(
                nativesharp::ast::AstNodeKind::UnaryExpression,
                nativesharp::SourceRange{}));
            syntax.back()->setTokenKind(nativesharp::lexer::TokenKind::Minus);
            syntax.back()->setFirst(current);
            current = syntax.back().get();
            continue;
        }
        if (index % 3U == 1U)
        {
            syntax.push_back(std::make_unique<nativesharp::ast::AstNode>(
                nativesharp::ast::AstNodeKind::IntegerLiteralExpression,
                nativesharp::SourceRange{}));
            syntax.back()->setIntegerValue(index + 2U);
            nativesharp::ast::AstNode* right = syntax.back().get();
            syntax.push_back(std::make_unique<nativesharp::ast::AstNode>(
                nativesharp::ast::AstNodeKind::BinaryExpression,
                nativesharp::SourceRange{}));
            syntax.back()->setTokenKind(nativesharp::lexer::TokenKind::Plus);
            syntax.back()->setFirst(current);
            syntax.back()->setSecond(right);
            current = syntax.back().get();
            continue;
        }

        syntax.push_back(std::make_unique<nativesharp::ast::AstNode>(
            nativesharp::ast::AstNodeKind::BooleanLiteralExpression,
            nativesharp::SourceRange{}));
        syntax.back()->setTokenKind(nativesharp::lexer::TokenKind::KwTrue);
        nativesharp::ast::AstNode* condition = syntax.back().get();
        syntax.push_back(std::make_unique<nativesharp::ast::AstNode>(
            nativesharp::ast::AstNodeKind::IntegerLiteralExpression,
            nativesharp::SourceRange{}));
        syntax.back()->setIntegerValue(index + 2U);
        nativesharp::ast::AstNode* whenTrue = syntax.back().get();
        syntax.push_back(std::make_unique<nativesharp::ast::AstNode>(
            nativesharp::ast::AstNodeKind::ConditionalExpression,
            nativesharp::SourceRange{}));
        syntax.back()->setFirst(condition);
        syntax.back()->setSecond(whenTrue);
        syntax.back()->setThird(current);
        current = syntax.back().get();
    }

    nativesharp::frontend::MethodExpressionTypeBinding binding;
    binding.typeKey = "builtin:int";
    ProjectOopHirModel model;
    std::string error;
    auto lowered = nativesharp::frontend::ProjectOopHirModelTestAccess::lowerExpression(
        model, current, syntax, binding, error);
    tests.expect(lowered != nullptr && error.empty(),
        "a 32768-level mixed expression lowers without process-stack recursion (" +
            error + ")");

    std::size_t observedDepth = 0;
    auto* cursor = lowered.get();
    while (cursor != nullptr && observedDepth < nestingDepth)
    {
        const std::size_t sourceIndex = nestingDepth - observedDepth - 1U;
        if (sourceIndex % 3U == 0U)
        {
            if (cursor->kind != nativesharp::frontend::ProjectOopHirNodeKind::Unary ||
                cursor->children.size() != 1U) break;
            cursor = cursor->children[0].get();
        }
        else if (sourceIndex % 3U == 1U)
        {
            if (cursor->kind != nativesharp::frontend::ProjectOopHirNodeKind::Binary ||
                cursor->children.size() != 2U) break;
            cursor = cursor->children[0].get();
        }
        else
        {
            if (cursor->kind != nativesharp::frontend::ProjectOopHirNodeKind::Conditional ||
                cursor->children.size() != 3U) break;
            cursor = cursor->children[2].get();
        }
        ++observedDepth;
    }
    tests.expect(observedDepth == nestingDepth && cursor != nullptr &&
            cursor->kind == nativesharp::frontend::ProjectOopHirNodeKind::IntegerConstant,
        "iterative mixed lowering preserves every family transition and its literal leaf");
    destroyHirTreeIteratively(lowered);
}

void testDeepStaticAssertEvaluation(TestContext& tests)
{
    constexpr std::size_t nestingDepth = 32768;
    std::vector<std::unique_ptr<nativesharp::ast::AstNode>> syntax;
    syntax.reserve(nestingDepth * 2U + 2U);
    syntax.push_back(std::make_unique<nativesharp::ast::AstNode>(
        nativesharp::ast::AstNodeKind::BooleanLiteralExpression,
        nativesharp::SourceRange{}));
    syntax.back()->setTokenKind(nativesharp::lexer::TokenKind::KwTrue);
    nativesharp::ast::AstNode* current = syntax.back().get();
    for (std::size_t index = 0; index < nestingDepth; ++index)
    {
        if (index % 2U == 0U)
        {
            syntax.push_back(std::make_unique<nativesharp::ast::AstNode>(
                nativesharp::ast::AstNodeKind::UnaryExpression,
                nativesharp::SourceRange{}));
            syntax.back()->setTokenKind(nativesharp::lexer::TokenKind::Bang);
            syntax.back()->setFirst(current);
            current = syntax.back().get();
            continue;
        }

        syntax.push_back(std::make_unique<nativesharp::ast::AstNode>(
            nativesharp::ast::AstNodeKind::BooleanLiteralExpression,
            nativesharp::SourceRange{}));
        syntax.back()->setTokenKind(nativesharp::lexer::TokenKind::KwTrue);
        nativesharp::ast::AstNode* condition = syntax.back().get();
        syntax.push_back(std::make_unique<nativesharp::ast::AstNode>(
            nativesharp::ast::AstNodeKind::BooleanLiteralExpression,
            nativesharp::SourceRange{}));
        syntax.back()->setTokenKind(nativesharp::lexer::TokenKind::KwFalse);
        nativesharp::ast::AstNode* whenFalse = syntax.back().get();
        syntax.push_back(std::make_unique<nativesharp::ast::AstNode>(
            nativesharp::ast::AstNodeKind::ConditionalExpression,
            nativesharp::SourceRange{}));
        syntax.back()->setFirst(condition);
        syntax.back()->setSecond(current);
        syntax.back()->setThird(whenFalse);
        current = syntax.back().get();
    }
    auto statement = std::make_unique<nativesharp::ast::AstNode>(
        nativesharp::ast::AstNodeKind::StaticAssertStatement,
        nativesharp::SourceRange{});
    statement->setFirst(current);

    nativesharp::frontend::MethodExpressionTypeBinding binding;
    binding.typeKey = "builtin:bool";
    ProjectOopHirModel model;
    std::string error;
    auto lowered = nativesharp::frontend::ProjectOopHirModelTestAccess::lowerStatement(
        model, statement.get(), syntax, binding, error);
    tests.expect(lowered != nullptr && error.empty(),
        "a 32768-level static_assert constant evaluates without process-stack recursion (" +
            error + ")");
    tests.expect(lowered != nullptr &&
            lowered->kind == nativesharp::frontend::ProjectOopHirNodeKind::Block,
        "deep static_assert preserves the successful empty-block lowering contract");
    destroyHirTreeIteratively(lowered);
}

void testStaticAssertShortCircuitEvaluation(TestContext& tests)
{
    const auto evaluate = [&](nativesharp::lexer::TokenKind operation,
                              nativesharp::lexer::TokenKind leftToken,
                              std::string expectedError)
    {
        std::vector<std::unique_ptr<nativesharp::ast::AstNode>> syntax;
        syntax.push_back(std::make_unique<nativesharp::ast::AstNode>(
            nativesharp::ast::AstNodeKind::BooleanLiteralExpression,
            nativesharp::SourceRange{}));
        syntax.back()->setTokenKind(leftToken);
        nativesharp::ast::AstNode* left = syntax.back().get();
        syntax.push_back(std::make_unique<nativesharp::ast::AstNode>(
            nativesharp::ast::AstNodeKind::NullLiteralExpression,
            nativesharp::SourceRange{}));
        nativesharp::ast::AstNode* invalidIfVisited = syntax.back().get();
        syntax.push_back(std::make_unique<nativesharp::ast::AstNode>(
            nativesharp::ast::AstNodeKind::BinaryExpression,
            nativesharp::SourceRange{}));
        syntax.back()->setTokenKind(operation);
        syntax.back()->setFirst(left);
        syntax.back()->setSecond(invalidIfVisited);
        nativesharp::ast::AstNode* expression = syntax.back().get();
        auto statement = std::make_unique<nativesharp::ast::AstNode>(
            nativesharp::ast::AstNodeKind::StaticAssertStatement,
            nativesharp::SourceRange{});
        statement->setFirst(expression);

        nativesharp::frontend::MethodExpressionTypeBinding binding;
        binding.typeKey = "builtin:bool";
        ProjectOopHirModel model;
        std::string error;
        auto lowered = nativesharp::frontend::ProjectOopHirModelTestAccess::lowerStatement(
            model, statement.get(), syntax, binding, error);
        tests.expect(error == expectedError,
            "static_assert preserves short-circuit selection without evaluating the unused operand");
        destroyHirTreeIteratively(lowered);
    };

    evaluate(
        nativesharp::lexer::TokenKind::AmpAmp,
        nativesharp::lexer::TokenKind::KwFalse,
        "static assertion failed");
    evaluate(
        nativesharp::lexer::TokenKind::PipePipe,
        nativesharp::lexer::TokenKind::KwTrue,
        "");
}

void testDeepHirDump(TestContext& tests)
{
    constexpr std::size_t nestingDepth = 8192;
    auto root = std::make_unique<nativesharp::frontend::ProjectOopHirNode>();
    root->kind = nativesharp::frontend::ProjectOopHirNodeKind::Block;
    root->typeKey = "builtin:void";
    auto* cursor = root.get();
    for (std::size_t index = 0; index < nestingDepth; ++index)
    {
        auto child = std::make_unique<nativesharp::frontend::ProjectOopHirNode>();
        child->kind = nativesharp::frontend::ProjectOopHirNodeKind::Block;
        child->typeKey = "builtin:void";
        cursor->children.push_back(std::move(child));
        cursor = cursor->children.front().get();
    }

    ProjectOopHirModel model;
    nativesharp::frontend::ProjectOopHirModelTestAccess::setDumperBody(
        model, std::move(root));
    const std::string dump = nativesharp::frontend::ProjectOopHirModelDumper::dump(model);
    tests.expect(
        static_cast<std::size_t>(std::count(dump.begin(), dump.end(), '\n')) ==
            nestingDepth + 4U,
        "an 8192-level HIR tree dumps without process-stack recursion");
    tests.expect(dump.starts_with("project.oop.hir functions=1\n") &&
            dump.find("function #0 deep_hir_dump abi=static") != std::string::npos &&
            dump.find("field-initializers=0\n") != std::string::npos,
        "iterative HIR dumping preserves header, function contract, preorder, and footer");
    auto body = nativesharp::frontend::ProjectOopHirModelTestAccess::takeDumperBody(model);
    destroyHirTreeIteratively(body);
}

void testDeepHirCoordinatorWalker(TestContext& tests, std::string_view walker)
{
    constexpr std::size_t nestingDepth = 32768;
    auto root = std::make_unique<nativesharp::frontend::ProjectOopHirNode>();
    root->kind = nativesharp::frontend::ProjectOopHirNodeKind::Block;
    auto* cursor = root.get();
    for (std::size_t index = 0; index < nestingDepth; ++index)
    {
        auto child = std::make_unique<nativesharp::frontend::ProjectOopHirNode>();
        child->kind = nativesharp::frontend::ProjectOopHirNodeKind::Block;
        cursor->children.push_back(std::move(child));
        cursor = cursor->children.front().get();
    }

    cursor->kind = nativesharp::frontend::ProjectOopHirNodeKind::OwnedTemporaryBorrow;
    cursor->localOrdinal = 100U;
    cursor->cleanupRegionOrdinal = 200U;
    cursor->hirRegionOrdinal = 5U;
    cursor->exceptionTargetRegionOrdinal = 6U;
    auto declaration = std::make_unique<nativesharp::frontend::ProjectOopHirNode>();
    declaration->kind = nativesharp::frontend::ProjectOopHirNodeKind::LocalDeclaration;
    declaration->localOrdinal = 77U;
    auto* declarationPointer = declaration.get();
    auto construction = std::make_unique<nativesharp::frontend::ProjectOopHirNode>();
    construction->kind = nativesharp::frontend::ProjectOopHirNodeKind::ObjectConstruction;
    auto* constructionPointer = construction.get();
    declaration->children.push_back(std::move(construction));
    cursor->children.push_back(std::move(declaration));

    if (walker == "collect")
    {
        auto secondDeclaration = std::make_unique<nativesharp::frontend::ProjectOopHirNode>();
        secondDeclaration->kind =
            nativesharp::frontend::ProjectOopHirNodeKind::LocalDeclaration;
        auto* secondDeclarationPointer = secondDeclaration.get();
        root->children.push_back(std::move(secondDeclaration));
        std::vector<nativesharp::frontend::ProjectOopHirNode*> declarations;
        nativesharp::frontend::ProjectOopHirModelTestAccess::collectLocalDeclarations(
            root.get(), declarations);
        tests.expect(declarations.size() == 2U && declarations[0] == declarationPointer &&
                declarations[1] == secondDeclarationPointer,
            "deep HIR declaration collection preserves preorder without process recursion");
    }
    else if (walker == "uses")
    {
        tests.expect(
            nativesharp::frontend::ProjectOopHirModelTestAccess::stackClassLocalUsesAreSafe(
                root.get(), declarationPointer, 77U),
            "deep HIR stack-promotion use proof completes without process recursion");

        auto assignment = std::make_unique<nativesharp::frontend::ProjectOopHirNode>();
        assignment->kind = nativesharp::frontend::ProjectOopHirNodeKind::Assignment;
        auto fieldAddress = std::make_unique<nativesharp::frontend::ProjectOopHirNode>();
        fieldAddress->kind = nativesharp::frontend::ProjectOopHirNodeKind::FieldAddress;
        auto localValue = std::make_unique<nativesharp::frontend::ProjectOopHirNode>();
        localValue->kind = nativesharp::frontend::ProjectOopHirNodeKind::LocalValue;
        localValue->localOrdinal = 77U;
        fieldAddress->children.push_back(std::move(localValue));
        assignment->children.push_back(std::move(fieldAddress));
        tests.expect(
            nativesharp::frontend::ProjectOopHirModelTestAccess::stackClassLocalUsesAreSafe(
                assignment.get(), declarationPointer, 77U),
            "iterative stack-promotion proof preserves direct field-store safety");
        assignment->kind = nativesharp::frontend::ProjectOopHirNodeKind::Block;
        tests.expect(
            !nativesharp::frontend::ProjectOopHirModelTestAccess::stackClassLocalUsesAreSafe(
                assignment.get(), declarationPointer, 77U),
            "iterative stack-promotion proof still rejects an escaping field address");
        destroyHirTreeIteratively(assignment);
    }
    else if (walker == "allocation")
    {
        auto laterConstruction = std::make_unique<nativesharp::frontend::ProjectOopHirNode>();
        laterConstruction->kind =
            nativesharp::frontend::ProjectOopHirNodeKind::ObjectConstruction;
        root->children.push_back(std::move(laterConstruction));
        tests.expect(
            nativesharp::frontend::ProjectOopHirModelTestAccess::firstUnpromotedObjectConstruction(
                root.get()) == constructionPointer,
            "deep HIR allocation search preserves first-preorder result");
    }
    else if (walker == "scan")
    {
        std::size_t localCount = 0;
        std::size_t cleanupCount = 0;
        std::size_t regionCount = 0;
        nativesharp::frontend::ProjectOopHirModelTestAccess::scanParallelOrdinals(
            root.get(), 90U, 190U, localCount, cleanupCount, regionCount);
        tests.expect(localCount == 11U && cleanupCount == 11U && regionCount == 6U,
            "deep HIR ordinal scan preserves synthetic maxima");
    }
    else if (walker == "apply")
    {
        nativesharp::frontend::ProjectOopHirModelTestAccess::applyParallelOrdinalOffsets(
            root.get(), 90U, 190U, 3U, 4U, 7U);
        tests.expect(cursor->localOrdinal == 103U && cursor->cleanupRegionOrdinal == 204U &&
                cursor->hirRegionOrdinal == 12U && cursor->exceptionTargetRegionOrdinal == 13U,
            "deep HIR ordinal rebasing preserves all offsets");
    }
    destroyHirTreeIteratively(root);
}

void testDeepHirModelDestruction(TestContext& tests)
{
    constexpr std::size_t nestingDepth = 32768;
    const auto makeDeepTree = []
    {
        auto root = std::make_unique<nativesharp::frontend::ProjectOopHirNode>();
        root->kind = nativesharp::frontend::ProjectOopHirNodeKind::Block;
        auto* cursor = root.get();
        for (std::size_t index = 0; index < nestingDepth; ++index)
        {
            auto child = std::make_unique<nativesharp::frontend::ProjectOopHirNode>();
            child->kind = nativesharp::frontend::ProjectOopHirNodeKind::Block;
            cursor->children.push_back(std::move(child));
            cursor = cursor->children.front().get();
        }
        return root;
    };

    ProjectOopHirModel model;
    nativesharp::frontend::ProjectOopHirModelTestAccess::setDumperBody(
        model, makeDeepTree());
    nativesharp::frontend::ProjectOopHirModelTestAccess::setDumperFieldInitializer(
        model, makeDeepTree());
    tests.expect(true,
        "32768-level HIR function and field roots reach model destruction without extraction");
}

void testDeepHirNodeDestruction(TestContext& tests)
{
    constexpr std::size_t nestingDepth = 32768;
    auto root = std::make_unique<nativesharp::frontend::ProjectOopHirNode>();
    root->kind = nativesharp::frontend::ProjectOopHirNodeKind::Block;
    auto* cursor = root.get();
    for (std::size_t index = 0; index < nestingDepth; ++index)
    {
        auto child = std::make_unique<nativesharp::frontend::ProjectOopHirNode>();
        child->kind = nativesharp::frontend::ProjectOopHirNodeKind::Block;
        cursor->children.push_back(std::move(child));
        cursor = cursor->children.front().get();
    }
    root.reset();
    tests.expect(true,
        "a 32768-level standalone HIR temporary is destroyed without process-stack recursion");
}

void testDeepHirBlockLowering(TestContext& tests)
{
    constexpr std::size_t nestingDepth = 32768;
    std::vector<std::unique_ptr<nativesharp::ast::AstNode>> syntax;
    syntax.reserve(nestingDepth + 1U);
    syntax.push_back(std::make_unique<nativesharp::ast::AstNode>(
        nativesharp::ast::AstNodeKind::BlockStatement,
        nativesharp::SourceRange{}));
    nativesharp::ast::AstNode* current = syntax.back().get();
    for (std::size_t index = 0; index < nestingDepth; ++index)
    {
        syntax.push_back(std::make_unique<nativesharp::ast::AstNode>(
            nativesharp::ast::AstNodeKind::BlockStatement,
            nativesharp::SourceRange{}));
        syntax.back()->addChild(current);
        current = syntax.back().get();
    }

    ProjectOopHirModel model;
    std::string error;
    std::unique_ptr<nativesharp::frontend::ProjectOopHirNode> lowered =
        nativesharp::frontend::ProjectOopHirModelTestAccess::lowerBlock(
            model, current, error);
    tests.expect(lowered != nullptr && error.empty(),
        "a 32768-level block chain lowers without process-stack recursion (" + error + ")");
    const auto* cursor = lowered.get();
    std::size_t observedDepth = 0;
    while (cursor != nullptr && cursor->children.size() == 1U &&
        cursor->children.front() != nullptr &&
        cursor->children.front()->kind ==
            nativesharp::frontend::ProjectOopHirNodeKind::Block)
    {
        cursor = cursor->children.front().get();
        ++observedDepth;
    }
    tests.expect(observedDepth == nestingDepth && cursor != nullptr && cursor->children.empty(),
        "deep block lowering preserves every block and the empty leaf");
}

void testDeepHirDeconstructionWalk(TestContext& tests)
{
    constexpr std::size_t nestingDepth = 32768;
    std::vector<std::unique_ptr<nativesharp::ast::AstNode>> syntax;
    syntax.reserve(nestingDepth + 1U);
    syntax.push_back(std::make_unique<nativesharp::ast::AstNode>(
        nativesharp::ast::AstNodeKind::IdentifierExpression,
        nativesharp::SourceRange{}));
    syntax.back()->setText("leaf");
    nativesharp::ast::AstNode* current = syntax.back().get();
    for (std::size_t index = 0; index < nestingDepth; ++index)
    {
        syntax.push_back(std::make_unique<nativesharp::ast::AstNode>(
            nativesharp::ast::AstNodeKind::TupleExpression,
            nativesharp::SourceRange{}));
        syntax.back()->addChild(current);
        current = syntax.back().get();
    }

    std::size_t expanded = 0;
    std::size_t leaves = 0;
    std::uint64_t observedOffset = 0;
    const auto expand = [&] (
        const nativesharp::ast::AstNode* tuple,
        const std::string& typeKey,
        std::uint64_t byteOffset,
        std::vector<nativesharp::frontend::HirTuplePatternFrame>& children)
    {
        ++expanded;
        children.push_back(nativesharp::frontend::HirTuplePatternFrame{
            tuple->children().front(), typeKey, byteOffset + 1U});
        return true;
    };
    const auto visit = [&] (
        const nativesharp::ast::AstNode*,
        const std::string&,
        std::uint64_t byteOffset)
    {
        ++leaves;
        observedOffset = byteOffset;
        return true;
    };
    const bool ok = nativesharp::frontend::visitHirTuplePatternLeaves(
        current, "synthetic:tuple", 0, expand, visit);
    tests.expect(ok && expanded == nestingDepth && leaves == 1U &&
            observedOffset == nestingDepth,
        "a 32768-level deconstruction pattern preserves preorder, type, and accumulated offset");

    std::vector<std::unique_ptr<nativesharp::ast::AstNode>> branchSyntax;
    const auto makeBranchNode = [&] (nativesharp::ast::AstNodeKind kind, std::string text = {})
    {
        branchSyntax.push_back(std::make_unique<nativesharp::ast::AstNode>(
            kind, nativesharp::SourceRange{}));
        branchSyntax.back()->setText(std::move(text));
        return branchSyntax.back().get();
    };
    auto* branchRoot = makeBranchNode(nativesharp::ast::AstNodeKind::TupleExpression);
    auto* left = makeBranchNode(nativesharp::ast::AstNodeKind::TupleExpression);
    auto* right = makeBranchNode(nativesharp::ast::AstNodeKind::TupleExpression);
    left->addChild(makeBranchNode(
        nativesharp::ast::AstNodeKind::IdentifierExpression, "a"));
    left->addChild(makeBranchNode(
        nativesharp::ast::AstNodeKind::IdentifierExpression, "b"));
    right->addChild(makeBranchNode(
        nativesharp::ast::AstNodeKind::IdentifierExpression, "c"));
    right->addChild(makeBranchNode(
        nativesharp::ast::AstNodeKind::IdentifierExpression, "d"));
    branchRoot->addChild(left);
    branchRoot->addChild(right);
    const auto expandBranch = [] (
        const nativesharp::ast::AstNode* tuple,
        const std::string& typeKey,
        std::uint64_t byteOffset,
        std::vector<nativesharp::frontend::HirTuplePatternFrame>& children)
    {
        for (std::size_t index = 0; index < tuple->children().size(); ++index)
            children.push_back(nativesharp::frontend::HirTuplePatternFrame{
                tuple->children()[index], typeKey, byteOffset + index});
        return true;
    };
    std::vector<std::string> order;
    const bool branchOk = nativesharp::frontend::visitHirTuplePatternLeaves(
        branchRoot, "synthetic:branch", 0, expandBranch,
        [&] (const nativesharp::ast::AstNode* leaf, const std::string&, std::uint64_t)
        {
            order.push_back(leaf->text());
            return true;
        });
    tests.expect(branchOk && order == std::vector<std::string>({"a", "b", "c", "d"}),
        "iterative deconstruction traversal preserves left-to-right leaf order");

    order.clear();
    const bool stopped = nativesharp::frontend::visitHirTuplePatternLeaves(
        branchRoot, "synthetic:branch", 0, expandBranch,
        [&] (const nativesharp::ast::AstNode* leaf, const std::string&, std::uint64_t)
        {
            order.push_back(leaf->text());
            return leaf->text() != "c";
        });
    tests.expect(!stopped && order == std::vector<std::string>({"a", "b", "c"}),
        "iterative deconstruction traversal stops at the first failing leaf");
}

void testDeepAggregateLeaves(TestContext& tests)
{
    constexpr std::size_t nestingDepth = 4096;
    constexpr std::uint64_t initialOffset = 13U;

    nativesharp::frontend::ProjectNameLookup nameLookup;
    nativesharp::frontend::ProjectInheritanceModel inheritanceModel;
    nativesharp::frontend::ProjectObjectLayoutModel objectLayoutModel;
    ProjectOopHirModel model;
    std::vector<std::string> typeKeys;
    const bool configured =
        nativesharp::frontend::ProjectOopHirModelTestAccess::configureAggregateLeafChain(
            model,
            nameLookup,
            inheritanceModel,
            objectLayoutModel,
            nestingDepth,
            typeKeys);
    tests.expect(configured, "the deep aggregate fixture builds valid name and layout models");
    if (!configured)
    {
        return;
    }

    nativesharp::ast::AstNode syntax(
        nativesharp::ast::AstNodeKind::IdentifierExpression,
        nativesharp::SourceRange{});
    std::vector<std::unique_ptr<nativesharp::frontend::ProjectOopHirNode>> leaves;
    std::string error;
    const bool lowered =
        nativesharp::frontend::ProjectOopHirModelTestAccess::appendAggregateLeaves(
            model,
            typeKeys.front(),
            initialOffset,
            &syntax,
            leaves,
            error);
    tests.expect(lowered && error.empty(),
        "a 4096-level aggregate leaf chain lowers without process-stack recursion (" +
            error + ")");
    tests.expect(leaves.size() == 1U &&
            leaves.front()->kind ==
                nativesharp::frontend::ProjectOopHirNodeKind::AggregateElement &&
            leaves.front()->typeKey == "builtin:int" &&
            leaves.front()->integerValue == initialOffset + 8U * nestingDepth &&
            leaves.front()->syntax == &syntax &&
            leaves.front()->children.empty(),
        "deep aggregate flattening preserves the scalar leaf, provenance, and absolute offset");

    leaves.clear();
    error.clear();
    const bool arrayLowered =
        nativesharp::frontend::ProjectOopHirModelTestAccess::appendAggregateLeaves(
            model, "builtin:int[3]", initialOffset, &syntax, leaves, error);
    tests.expect(arrayLowered && error.empty() && leaves.size() == 3U &&
            leaves[0]->integerValue == initialOffset &&
            leaves[1]->integerValue == initialOffset + 4U &&
            leaves[2]->integerValue == initialOffset + 8U,
        "iterative fixed-array flattening preserves ascending source order and stride");

    nativesharp::frontend::ObjectFieldLayout firstField;
    firstField.offset = 8U;
    firstField.typeKey = "builtin:int";
    nativesharp::frontend::ObjectFieldLayout secondField;
    secondField.offset = 24U;
    secondField.typeKey = "builtin:long";
    std::vector<nativesharp::frontend::ObjectFieldLayout> rootFields;
    rootFields.push_back(std::move(firstField));
    rootFields.push_back(std::move(secondField));
    nativesharp::frontend::ProjectOopHirModelTestAccess::setAggregateRootFields(
        objectLayoutModel, std::move(rootFields));
    leaves.clear();
    error.clear();
    const bool structLowered =
        nativesharp::frontend::ProjectOopHirModelTestAccess::appendAggregateLeaves(
            model, typeKeys.front(), initialOffset, &syntax, leaves, error);
    tests.expect(structLowered && error.empty() && leaves.size() == 2U &&
            leaves[0]->typeKey == "builtin:int" &&
            leaves[0]->integerValue == initialOffset + 8U &&
            leaves[1]->typeKey == "builtin:long" &&
            leaves[1]->integerValue == initialOffset + 24U,
        "iterative struct flattening preserves declared field order and absolute offsets");
}

void testDeepAggregateElements(TestContext& tests)
{
    constexpr std::size_t nestingDepth = 4096;
    constexpr std::uint64_t initialOffset = 17U;

    nativesharp::frontend::ProjectNameLookup nameLookup;
    nativesharp::frontend::ProjectInheritanceModel inheritanceModel;
    nativesharp::frontend::ProjectObjectLayoutModel objectLayoutModel;
    nativesharp::frontend::ProjectMethodBodyModel methodBodyModel;
    ProjectOopHirModel model;
    std::vector<std::string> typeKeys;
    const bool configured =
        nativesharp::frontend::ProjectOopHirModelTestAccess::configureAggregateLeafChain(
            model,
            nameLookup,
            inheritanceModel,
            objectLayoutModel,
            nestingDepth,
            typeKeys);
    tests.expect(configured, "the deep aggregate element fixture builds valid semantic models");
    if (!configured)
    {
        return;
    }

    std::vector<std::unique_ptr<nativesharp::ast::AstNode>> syntax;
    syntax.reserve(nestingDepth + 1U);
    syntax.push_back(std::make_unique<nativesharp::ast::AstNode>(
        nativesharp::ast::AstNodeKind::IntegerLiteralExpression,
        nativesharp::SourceRange{}));
    syntax.back()->setIntegerValue(7U);
    auto* literal = syntax.back().get();
    auto* current = literal;
    std::vector<nativesharp::ast::AstNode*> aggregates(nestingDepth);
    for (std::size_t remaining = nestingDepth; remaining > 0; --remaining)
    {
        const std::size_t ordinal = remaining - 1U;
        syntax.push_back(std::make_unique<nativesharp::ast::AstNode>(
            nativesharp::ast::AstNodeKind::AggregateInitializerExpression,
            nativesharp::SourceRange{}));
        syntax.back()->addChild(current);
        current = syntax.back().get();
        aggregates[ordinal] = current;
    }

    nativesharp::frontend::MethodExpressionTypeBinding literalType;
    literalType.syntax = literal;
    literalType.typeKey = "builtin:int";
    nativesharp::frontend::ProjectOopHirModelTestAccess::configureAggregateElementBindings(
        model, methodBodyModel, aggregates, literal, typeKeys, literalType);

    std::vector<std::unique_ptr<nativesharp::frontend::ProjectOopHirNode>> elements;
    std::string error;
    const bool lowered =
        nativesharp::frontend::ProjectOopHirModelTestAccess::appendAggregateElements(
            model,
            aggregates.front(),
            typeKeys.front(),
            initialOffset,
            elements,
            error);
    tests.expect(lowered && error.empty(),
        "a 4096-level aggregate initializer lowers without process-stack recursion (" +
            error + ")");
    tests.expect(elements.size() == 1U &&
            elements.front()->kind ==
                nativesharp::frontend::ProjectOopHirNodeKind::AggregateElement &&
            elements.front()->typeKey == "builtin:int" &&
            elements.front()->integerValue == initialOffset + 8U * nestingDepth &&
            elements.front()->syntax == literal &&
            elements.front()->children.size() == 1U &&
            elements.front()->children.front()->kind ==
                nativesharp::frontend::ProjectOopHirNodeKind::IntegerConstant &&
            elements.front()->children.front()->integerValue == 7U,
        "deep aggregate lowering preserves the leaf value, provenance, and absolute offset");
}
}

int main()
{
    TestContext tests;
    const std::filesystem::path root =
        std::filesystem::temp_directory_path() / "nativesharp_project_oop_lowering_tests";
    std::error_code cleanupError;
    std::filesystem::remove_all(root, cleanupError);
    std::filesystem::create_directories(root);

    char* focus = nullptr;
    std::size_t focusLength = 0;
    const int focusError = _dupenv_s(
        &focus, &focusLength, "NSC_PROJECT_OOP_LOWERING_FOCUS");
    const std::string focusValue = focusError == 0 && focus != nullptr
        ? std::string(focus)
        : std::string{};
    std::free(focus);
    if (focusValue == "precise-incremental-invalidation")
    {
        testPreciseIncrementalInvalidation(tests, root);
    }
    else if (focusValue == "hir-default-argument-depth")
    {
        testDeepOptionalDefaultHir(tests, root);
    }
    else if (focusValue == "hir-async-yield-state-depth")
    {
        testDeepYieldStateAssignment(tests);
    }
    else if (focusValue == "hir-generator-throw-depth")
    {
        testDeepGeneratorThrowRewrite(tests);
    }
    else if (focusValue == "hir-async-poll-return-depth")
    {
        testDeepAsyncPollReturnRewrite(tests);
    }
    else if (focusValue == "hir-async-await-scan-depth")
    {
        testDeepAsyncAwaitScan(tests);
    }
    else if (focusValue == "hir-builtin-unary-depth")
    {
        testDeepBuiltinUnaryExpressionLowering(tests);
    }
    else if (focusValue == "hir-conversion-depth")
    {
        testDeepConversionExpressionLowering(tests);
    }
    else if (focusValue == "hir-class-field-depth")
    {
        testDeepClassFieldExpressionLowering(tests);
    }
    else if (focusValue == "hir-property-getter-depth")
    {
        testDeepPropertyGetterExpressionLowering(tests, root);
    }
    else if (focusValue == "hir-overflow-policy-depth")
    {
        testDeepOverflowPolicyExpressionLowering(tests);
    }
    else if (focusValue == "hir-builtin-binary-depth")
    {
        testDeepBuiltinBinaryExpressionLowering(tests);
    }
    else if (focusValue == "hir-conditional-depth")
    {
        testDeepConditionalExpressionLowering(tests);
    }
    else if (focusValue == "hir-mixed-depth")
    {
        testDeepMixedExpressionLowering(tests);
    }
    else if (focusValue == "hir-static-assert-depth")
    {
        testDeepStaticAssertEvaluation(tests);
    }
    else if (focusValue == "hir-static-assert-short-circuit")
    {
        testStaticAssertShortCircuitEvaluation(tests);
    }
    else if (focusValue == "hir-dump-depth")
    {
        testDeepHirDump(tests);
    }
    else if (focusValue.starts_with("hir-coordinator-walker-"))
    {
        testDeepHirCoordinatorWalker(
            tests, focusValue.substr(std::string_view("hir-coordinator-walker-").size()));
    }
    else if (focusValue == "hir-model-destruction-depth")
    {
        testDeepHirModelDestruction(tests);
    }
    else if (focusValue == "hir-node-destruction-depth")
    {
        testDeepHirNodeDestruction(tests);
    }
    else if (focusValue == "hir-block-depth")
    {
        testDeepHirBlockLowering(tests);
    }
    else if (focusValue == "hir-deconstruction-walk-depth")
    {
        testDeepHirDeconstructionWalk(tests);
    }
    else if (focusValue == "hir-aggregate-leaves-depth")
    {
        testDeepAggregateLeaves(tests);
    }
    else if (focusValue == "hir-aggregate-elements-depth")
    {
        testDeepAggregateElements(tests);
    }
    else
    {
        testEndToEndOopLowering(tests, root);
        testDeallocationContract(tests, root);
        testAggregateCallableBoundary(tests, root);
        testNarrowSignedEnumNameLowering(tests, root);
        testPreciseIncrementalInvalidation(tests, root);
        testPartitionStaticFieldReachability(tests, root);
        testDeepOptionalDefaultHir(tests, root);
        testDeepYieldStateAssignment(tests);
        testDeepGeneratorThrowRewrite(tests);
        testDeepAsyncPollReturnRewrite(tests);
        testDeepAsyncAwaitScan(tests);
        testDeepBuiltinUnaryExpressionLowering(tests);
        testDeepClassFieldExpressionLowering(tests);
        testDeepPropertyGetterExpressionLowering(tests, root);
        testDeepOverflowPolicyExpressionLowering(tests);
        testDeepBuiltinBinaryExpressionLowering(tests);
        testDeepConditionalExpressionLowering(tests);
        testDeepMixedExpressionLowering(tests);
        testDeepStaticAssertEvaluation(tests);
        testStaticAssertShortCircuitEvaluation(tests);
    }

    std::filesystem::remove_all(root, cleanupError);
    return tests.result();
}
