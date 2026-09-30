#include "nativesharp/Frontend/ProjectCompilation.h"
#include "nativesharp/Frontend/ComptimeEngine.h"

#include "nativesharp/Compilation/ProjectGraphLoader.h"
#include "nativesharp/Driver/CompilationWorkerPool.h"
#include "nativesharp/Lexer/TokenKind.h"
#include "nativesharp/Source/SourceFile.h"
#include "nativesharp/Source/SourcePosition.h"

#include <atomic>
#include <chrono>
#include <exception>
#include <set>
#include <memory>
#include <stdexcept>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>
#include <vector>

namespace nativesharp::frontend
{
void ProjectCompilation::setCompilationJobs(std::size_t jobs) noexcept
{
    compilationJobs_ = std::max<std::size_t>(1, jobs);
}

void ProjectCompilation::setProjectLoadOptions(compilation::ProjectLoadOptions options)
{
    projectLoadOptions_ = std::move(options);
}

void ProjectCompilation::setCancellationCheck(std::function<bool()> check)
{
    cancellationCheck_ = std::move(check);
}

void ProjectCompilation::setSourceBufferProvider(SourceBufferProvider provider)
{
    sourceBufferProvider_ = std::move(provider);
}

void ProjectCompilation::setPhaseTimingSink(
    std::function<void(std::string_view, std::chrono::nanoseconds)> sink)
{
    phaseTimingSink_ = std::move(sink);
}

void ProjectCompilation::setProgressSink(
    std::function<void(const basic::CompilationEvent&)> sink)
{
    progressSink_ = std::move(sink);
}

bool ProjectCompilation::cancellationRequested(std::string& loadError, std::string_view nextPhase) const
{
    if (!cancellationCheck_ || !cancellationCheck_()) return false;
    loadError = "compilation cancelled before phase '" + std::string(nextPhase) + "'";
    return true;
}
namespace
{
using lexer::Token;
using lexer::TokenKind;

class ProjectPhaseTimer final
{
public:
    ProjectPhaseTimer(
        const std::function<void(std::string_view, std::chrono::nanoseconds)>& timingSink,
        const std::function<void(const basic::CompilationEvent&)>& progressSink,
        std::string_view phase)
        : timingSink_(timingSink), progressSink_(progressSink), phase_(phase),
          started_(timingSink || progressSink ? std::chrono::steady_clock::now()
                                              : std::chrono::steady_clock::time_point{})
    {
        if (progressSink_)
            progressSink_({basic::CompilationEventKind::PhaseStarted,
                phase_, {}, 0, 0, {}});
    }

    ~ProjectPhaseTimer()
    {
        if (!timingSink_ && !progressSink_) return;
        const std::chrono::nanoseconds elapsed =
            std::chrono::duration_cast<std::chrono::nanoseconds>(
                std::chrono::steady_clock::now() - started_);
        if (timingSink_) timingSink_(phase_, elapsed);
        if (progressSink_)
            progressSink_({basic::CompilationEventKind::PhaseFinished,
                phase_, {}, 0, 0, elapsed});
    }

private:
    const std::function<void(std::string_view, std::chrono::nanoseconds)>& timingSink_;
    const std::function<void(const basic::CompilationEvent&)>& progressSink_;
    std::string_view phase_;
    std::chrono::steady_clock::time_point started_;
};

void reportSourceProgress(
    const std::function<void(const basic::CompilationEvent&)>& sink,
    std::string_view phase,
    const std::filesystem::path& path,
    std::size_t completed,
    std::size_t total)
{
    if (!sink) return;
    const std::string detail = path.generic_string();
    sink({basic::CompilationEventKind::WorkFinished,
        phase, detail, completed, total, {}});
}

bool waitForSyntaxWorkers(
    driver::CompilationWorkerPool& pool,
    std::string_view phase,
    std::string& error)
{
    try
    {
        pool.wait();
        return true;
    }
    catch (const std::exception& exception)
    {
        error = "parallel " + std::string(phase) + " failed: " + exception.what();
    }
    catch (...)
    {
        error = "parallel " + std::string(phase) + " failed with a non-standard exception";
    }
    return false;
}

std::string joinName(std::span<const std::string> prefix, std::string_view name)
{
    std::string result;
    for (const std::string& segment : prefix)
    {
        if (!result.empty())
        {
            result.push_back('.');
        }
        result += segment;
    }
    if (!result.empty() && !name.empty())
    {
        result.push_back('.');
    }
    result += name;
    return result;
}

bool readQualifiedName(
    std::span<const Token> tokens,
    std::size_t& index,
    std::vector<std::string>& segments)
{
    segments.clear();
    if (index >= tokens.size() || tokens[index].kind() != TokenKind::Identifier)
    {
        return false;
    }

    segments.emplace_back(tokens[index].lexeme());
    ++index;
    while (index + 1 < tokens.size() &&
           tokens[index].kind() == TokenKind::Dot &&
           tokens[index + 1].kind() == TokenKind::Identifier)
    {
        segments.emplace_back(tokens[index + 1].lexeme());
        index += 2;
    }
    return true;
}

void skipBalancedBlock(std::span<const Token> tokens, std::size_t& index)
{
    if (index >= tokens.size() || tokens[index].kind() != TokenKind::LeftBrace)
    {
        return;
    }

    int depth = 0;
    while (index < tokens.size())
    {
        if (tokens[index].kind() == TokenKind::LeftBrace)
        {
            ++depth;
        }
        else if (tokens[index].kind() == TokenKind::RightBrace)
        {
            --depth;
            ++index;
            if (depth == 0)
            {
                return;
            }
            continue;
        }
        else if (tokens[index].kind() == TokenKind::EndOfFile)
        {
            return;
        }
        ++index;
    }
}

// NSC-LIMIT-009. Las formas bajo las que un tipo puede escribirse, sembradas todas. Para un tipo
// de namespace son las dos de siempre --`Item` y `ns.Item`--; para uno ANIDADO son ademas las
// intermedias, `Outer.Item` y `ns.Outer.Item`, porque el nombre fuente de un anidado lleva punto y
// el parser tiene que reconocerlo escrito desde dentro del contenedor y desde fuera.
//
// Aqui NO hay identidad canonica: esto es la lista de nombres con la que el parser desambigua
// `Foo x;` de una expresion, y nada mas. La identidad --con `+`-- la construye TypeIdentity, en
// otra fase y a partir del arbol, no de esta tabla.
void insertTypeSeeds(
    std::set<std::string>& names,
    const std::vector<std::string>& namespacePrefix,
    const std::vector<std::string>& typeChain)
{
    if (typeChain.empty())
    {
        return;
    }
    for (std::size_t start = 0; start < typeChain.size(); ++start)
    {
        std::string spelling;
        for (std::size_t index = start; index < typeChain.size(); ++index)
        {
            if (!spelling.empty()) spelling.push_back('.');
            spelling += typeChain[index];
        }
        names.insert(spelling);
        names.insert(joinName(namespacePrefix, spelling));
    }
}

void collectTypeSeedsInScope(
    std::span<const Token> tokens,
    std::size_t& index,
    const std::vector<std::string>& namespacePrefix,
    const std::vector<std::string>& containingTypes,
    TokenKind terminator,
    std::set<std::string>& names)
{
    while (index < tokens.size() && tokens[index].kind() != TokenKind::EndOfFile)
    {
        if (terminator != TokenKind::Invalid && tokens[index].kind() == terminator)
        {
            return;
        }

        if (tokens[index].kind() == TokenKind::KwNamespace)
        {
            ++index;
            std::vector<std::string> declaredSegments;
            if (!readQualifiedName(tokens, index, declaredSegments))
            {
                continue;
            }

            std::vector<std::string> nested = namespacePrefix;
            nested.insert(nested.end(), declaredSegments.begin(), declaredSegments.end());
            if (index < tokens.size() && tokens[index].kind() == TokenKind::Semicolon)
            {
                ++index;
                collectTypeSeedsInScope(tokens, index, nested, containingTypes, terminator, names);
                return;
            }
            if (index < tokens.size() && tokens[index].kind() == TokenKind::LeftBrace)
            {
                ++index;
                collectTypeSeedsInScope(
                    tokens, index, nested, containingTypes, TokenKind::RightBrace, names);
                if (index < tokens.size() && tokens[index].kind() == TokenKind::RightBrace)
                {
                    ++index;
                }
                continue;
            }
            continue;
        }

        if (tokens[index].kind() == TokenKind::KwStruct ||
            (tokens[index].kind() == TokenKind::Identifier &&
             tokens[index].lexeme() == "union") ||
            tokens[index].kind() == TokenKind::KwClass ||
            tokens[index].kind() == TokenKind::KwInterface ||
            tokens[index].kind() == TokenKind::KwEnum)
        {
            ++index;
            std::vector<std::string> chain = containingTypes;
            if (index < tokens.size() && tokens[index].kind() == TokenKind::Identifier)
            {
                chain.emplace_back(tokens[index].lexeme());
                insertTypeSeeds(names, namespacePrefix, chain);
                ++index;
            }
            // NSC-LIMIT-009. Avanzar HASTA el cuerpo, y despues BAJAR a el.
            //
            // Las dos mitades importan. La primera porque entre el nombre y el `{` cabe una lista
            // de parametros genericos, una lista de bases, el tipo subyacente de un enum y una
            // clausula `where`: la version anterior solo miraba si el token siguiente al nombre
            // era `{`, de modo que en `class C<T> : Base { ... }` no reconocia el cuerpo y seguia
            // leyendo sus tokens en linea recta, como si fueran declaraciones del namespace.
            //
            // Y la segunda porque un cuerpo de tipo YA NO se salta: dentro puede haber otro tipo.
            // Bajar es lo unico que hace que `Outer.Item` exista para el parser. Los cuerpos de
            // metodo de dentro se saltan solos: los cierra la rama `LeftBrace` de mas abajo.
            while (index < tokens.size() &&
                   tokens[index].kind() != TokenKind::LeftBrace &&
                   tokens[index].kind() != TokenKind::Semicolon &&
                   tokens[index].kind() != TokenKind::RightBrace &&
                   tokens[index].kind() != TokenKind::EndOfFile)
            {
                ++index;
            }
            if (index < tokens.size() && tokens[index].kind() == TokenKind::LeftBrace)
            {
                ++index;
                collectTypeSeedsInScope(
                    tokens, index, namespacePrefix, chain, TokenKind::RightBrace, names);
                if (index < tokens.size() && tokens[index].kind() == TokenKind::RightBrace)
                {
                    ++index;
                }
            }
            continue;
        }

        if (tokens[index].kind() == TokenKind::KwDelegate)
        {
            // Stage 52 fix: this scanner previously only seeded class/struct/interface/enum
            // names, never delegate names. That is invisible within a single file, because the
            // real parser (Parser::parseDelegateDeclaration) inserts the delegate's name into
            // its OWN incrementally-built knownTypeNames_ the moment it parses the declaration,
            // so a later use in the SAME translation unit already sees it. But a project that
            // only *references* another project's delegate (e.g. this package's own
            // `HashFunc<T>`/`Comparison<T>`/`Predicate<T>`/`Action<T>` from Delegates.ns, used by
            // a consuming project via <ProjectReference>) never physically parses that
            // declaration itself -- it relies entirely on this cross-file seed list, which was
            // missing the name. The concrete, reliably reproducible symptom: `Predicate<int> p =
            // someMethod;` in a project referencing this one failed to parse ("expected
            // expression") for a builtin type argument specifically (a class type argument
            // happened to disambiguate through a different path and did not expose the gap).
            //
            // A delegate declaration has the shape `delegate ReturnType Name<TParams>(params);`
            // (or `delegate ReturnType Name(params);` for a non-generic delegate) -- ReturnType
            // can itself be an arbitrary type, including a generic one, so the delegate's own
            // name is identified here as the first identifier directly followed by '<' or '('
            // (its generic-parameter list or its parameter list), skipping over any balanced
            // '<...>' the return type itself contributes first.
            ++index;
            while (index < tokens.size() &&
                   tokens[index].kind() != TokenKind::Semicolon &&
                   tokens[index].kind() != TokenKind::LeftBrace)
            {
                if (tokens[index].kind() == TokenKind::Identifier &&
                    index + 1 < tokens.size() &&
                    (tokens[index + 1].kind() == TokenKind::Less ||
                     tokens[index + 1].kind() == TokenKind::LeftParen))
                {
                    std::vector<std::string> chain = containingTypes;
                    chain.emplace_back(tokens[index].lexeme());
                    insertTypeSeeds(names, namespacePrefix, chain);
                    ++index;
                    break;
                }
                if (tokens[index].kind() == TokenKind::Less)
                {
                    int depth = 1;
                    ++index;
                    while (index < tokens.size() && depth > 0 &&
                           tokens[index].kind() != TokenKind::Semicolon &&
                           tokens[index].kind() != TokenKind::LeftBrace)
                    {
                        if (tokens[index].kind() == TokenKind::Less) ++depth;
                        else if (tokens[index].kind() == TokenKind::Greater) --depth;
                        ++index;
                    }
                    continue;
                }
                ++index;
            }
            while (index < tokens.size() &&
                   tokens[index].kind() != TokenKind::Semicolon &&
                   tokens[index].kind() != TokenKind::LeftBrace)
            {
                ++index;
            }
            if (index < tokens.size() && tokens[index].kind() == TokenKind::Semicolon)
            {
                ++index;
            }
            continue;
        }

        if (tokens[index].kind() == TokenKind::LeftBrace)
        {
            skipBalancedBlock(tokens, index);
            continue;
        }
        ++index;
    }
}

void collectUsingAliases(
    std::span<const Token> tokens,
    std::vector<std::pair<std::string, std::string>>& aliases)
{
    std::size_t index = 0;
    while (index + 3 < tokens.size())
    {
        if (tokens[index].kind() == TokenKind::KwUsing &&
            tokens[index + 1].kind() == TokenKind::Identifier &&
            tokens[index + 2].kind() == TokenKind::Equal)
        {
            const std::string alias(tokens[index + 1].lexeme());
            index += 3;
            std::vector<std::string> targetSegments;
            if (readQualifiedName(tokens, index, targetSegments))
            {
                aliases.emplace_back(alias, joinName(targetSegments, ""));
            }
            continue;
        }
        ++index;
    }
}

std::vector<std::string> collectParserKnownTypeNames(
    std::span<const std::unique_ptr<ProjectSyntaxUnit>> units)
{
    std::set<std::string> names;
    std::vector<std::pair<std::string, std::string>> aliases;
    for (const std::unique_ptr<ProjectSyntaxUnit>& unit : units)
    {
        if (unit == nullptr || !unit->isLexicallyValid())
        {
            continue;
        }
        std::size_t index = 0;
        collectTypeSeedsInScope(unit->tokens(), index, {}, {}, TokenKind::Invalid, names);
        collectUsingAliases(unit->tokens(), aliases);
    }

    const std::vector<std::string> baseNames(names.begin(), names.end());
    for (const auto& [alias, target] : aliases)
    {
        for (const std::string& known : baseNames)
        {
            if (known == target)
            {
                names.insert(alias);
            }
            const std::string prefix = target + ".";
            if (known.starts_with(prefix))
            {
                names.insert(alias + known.substr(target.size()));
            }
        }
    }
    return std::vector<std::string>(names.begin(), names.end());
}
}

bool ProjectCompilation::setPrecompiledModules(
    std::vector<PrecompiledSyntaxModule> modules,
    std::string& error)
{
    error.clear();
    if (projectGraph_.has_value() || !units_.empty())
    {
        error = "precompiled modules must be configured before project compilation starts";
        return false;
    }
    precompiledModules_ = std::move(modules);
    return true;
}

bool ProjectCompilation::setPreloadedProjectGraph(
    compilation::ProjectGraph graph,
    std::string& error)
{
    error.clear();
    if (projectGraph_.has_value() || !units_.empty() || genericModel_ != nullptr ||
        nameLookup_ != nullptr || memberModel_ != nullptr || namespaceFunctionModel_ != nullptr)
    {
        error = "project graph must be configured before project compilation starts";
        return false;
    }
    if (graph.rootProject() == nullptr)
    {
        error = "preloaded project graph has no root project";
        return false;
    }
    projectGraph_ = std::move(graph);
    return true;
}

bool ProjectCompilation::compileProjectSyntax(
    const std::filesystem::path& projectFile,
    std::string& loadError)
{
    return compileSyntaxCore(projectFile, loadError);
}

bool ProjectCompilation::compileProjectNames(
    const std::filesystem::path& projectFile,
    std::string& loadError)
{
    if (!compileProjectGenerics(projectFile, loadError))
    {
        return false;
    }
    if (cancellationRequested(loadError, "names")) return false;

    const ProjectPhaseTimer timing(phaseTimingSink_, progressSink_, "names");
    nameLookup_ = std::make_unique<ProjectNameLookup>();
    if (!nameLookup_->build(*projectGraph_, units_, diagnostics_, compilationJobs_)) return false;
    return enterStage(CompilationStage::Binding, loadError);
}

bool ProjectCompilation::compileProjectGenerics(
    const std::filesystem::path& projectFile,
    std::string& loadError)
{
    if (!compileSyntaxCore(projectFile, loadError))
    {
        return false;
    }
    if (cancellationRequested(loadError, "generics")) return false;

    const ProjectPhaseTimer timing(phaseTimingSink_, progressSink_, "generics");
    genericModel_ = std::make_unique<ProjectGenericModel>();
    std::vector<PrecompiledGenericSpecialization> precompiledSpecializations;
    for (const PrecompiledSyntaxModule& module : precompiledModules_)
    {
        precompiledSpecializations.insert(
            precompiledSpecializations.end(),
            module.genericSpecializations.begin(),
            module.genericSpecializations.end());
    }
    return genericModel_->build(
        *projectGraph_, units_, precompiledSpecializations, diagnostics_);
}

bool ProjectCompilation::compileProjectMembers(
    const std::filesystem::path& projectFile,
    std::string& loadError)
{
    if (!compileProjectNames(projectFile, loadError))
    {
        return false;
    }
    if (cancellationRequested(loadError, "members")) return false;

    const ProjectPhaseTimer timing(phaseTimingSink_, progressSink_, "members");
    memberModel_ = std::make_unique<ProjectMemberModel>();
    if (!memberModel_->build(*nameLookup_, units_, diagnostics_, genericModel_.get())) return false;

    std::vector<PrecompiledMemberIdentity> precompiledMembers;
    std::vector<std::pair<std::string, std::string>> specializationTypeAliases;
    std::unordered_map<std::string_view, const GenericSpecializationRecord*>
        localSpecializationsByKey;
    localSpecializationsByKey.reserve(genericModel_->specializations().size());
    for (const GenericSpecializationRecord& specialization : genericModel_->specializations())
        localSpecializationsByKey.emplace(
            specialization.specializationKey, &specialization);
    for (const PrecompiledSyntaxModule& module : precompiledModules_)
    {
        precompiledMembers.insert(precompiledMembers.end(), module.members.begin(), module.members.end());
        for (const PrecompiledGenericSpecialization& imported : module.genericSpecializations)
        {
            if (imported.kind == GenericTemplateKind::Method) continue;
            const auto local = localSpecializationsByKey.find(imported.specializationKey);
            if (local == localSpecializationsByKey.end()) continue;

            const auto typeKey = [](std::string_view templateKey, std::string_view specializedName) {
                const std::size_t moduleSeparator = templateKey.find("::");
                if (moduleSeparator == std::string_view::npos) return std::string{};
                const std::size_t terminal = templateKey.find_last_of(".+");
                const std::size_t terminalBegin = terminal == std::string_view::npos ||
                        terminal < moduleSeparator + 2U
                    ? moduleSeparator + 2U
                    : terminal + 1U;
                if (templateKey.find('`', terminalBegin) == std::string_view::npos) return std::string{};
                return "type:" + std::string(templateKey.substr(0, terminalBegin)) +
                    std::string(specializedName);
            };
            const std::string localTypeKey =
                typeKey(local->second->templateKey, local->second->specializedName);
            const std::string publishedTypeKey = typeKey(imported.templateKey, imported.specializedName);
            if (!localTypeKey.empty() && !publishedTypeKey.empty())
                specializationTypeAliases.emplace_back(localTypeKey, publishedTypeKey);
        }
    }
    if (!memberModel_->bindPrecompiledPhysicalNames(
            precompiledMembers, specializationTypeAliases, loadError))
    {
        return false;
    }

    namespaceFunctionModel_ = std::make_unique<ProjectNamespaceFunctionModel>();
    return namespaceFunctionModel_->build(
        *nameLookup_, *memberModel_, units_, diagnostics_, genericModel_.get());
}


bool ProjectCompilation::compileProjectInheritance(
    const std::filesystem::path& projectFile,
    std::string& loadError)
{
    if (!compileProjectMembers(projectFile, loadError))
    {
        return false;
    }
    if (cancellationRequested(loadError, "inheritance")) return false;

    const ProjectPhaseTimer timing(phaseTimingSink_, progressSink_, "inheritance");
    inheritanceModel_ = std::make_unique<ProjectInheritanceModel>();
    return inheritanceModel_->build(*nameLookup_, diagnostics_);
}

// NSC-LIMIT-007. LOS CONSTRAINTS SE VALIDAN AQUI, y este es el unico punto posible de toda la
// cadena. La materializacion generica corre en la fase 2 --antes que nombres, miembros, herencia
// e interfaces-- asi que no puede validar; y si se dejara para despues, los cuerpos ya se habrian
// analizado sobre una especializacion invalida y el error saldria de DENTRO del cuerpo sustituido,
// apuntando a las tripas de la biblioteca en vez de al sitio de la llamada.
//
// Es una FASE, no un trozo de la de cuerpos, y la diferencia es medible: `--check-project-generic-
// constraints` tiene que poder fallar por un `where` y solo por un `where`. Cuando la validacion
// vivia dentro de `compileProjectMethodBodies`, esa entrada analizaba ademas cada cuerpo del
// proyecto y devolvia rojo por errores ajenos al ID.
bool ProjectCompilation::buildGenericConstraintModel()
{
    const ProjectPhaseTimer timing(phaseTimingSink_, progressSink_, "generic-constraints");
    genericConstraintModel_ = std::make_unique<ProjectGenericConstraintModel>();
    return genericConstraintModel_->build(
        *genericModel_, *nameLookup_, *memberModel_, *inheritanceModel_, *interfaceModel_,
        diagnostics_);
}

bool ProjectCompilation::compileProjectGenericConstraints(
    const std::filesystem::path& projectFile,
    std::string& loadError)
{
    if (!compileProjectInterfaces(projectFile, loadError))
    {
        return false;
    }
    if (cancellationRequested(loadError, "generic-constraints")) return false;
    return buildGenericConstraintModel();
}

bool ProjectCompilation::compileProjectMethodBodies(
    const std::filesystem::path& projectFile,
    std::string& loadError)
{
    // LA CADENA SE ENCADENA POR `compileProjectInterfaces`, NO POR LA FASE DE CONSTRAINTS, y la
    // distincion no es de estilo: la fase de constraints devuelve `false` tambien cuando el
    // programa es valido en todo salvo en un `where`, y eso NO es motivo para parar --se sigue
    // analizando el resto del fichero, o un solo `Caja<int>` mal escrito ocultaria todos los
    // demas diagnosticos--. Encadenar por su valor de retorno confundia las dos cosas: cuando
    // una fase ANTERIOR fallaba sin dejar `loadError` --un error de diagnostico, no de carga--
    // se seguia adelante con los modelos sin construir, y el modelo de cuerpos deferenciaba un
    // puntero nulo. Medido: segfault en ProjectMemberModel::isBuilt.
    if (!compileProjectInterfaces(projectFile, loadError))
    {
        return false;
    }
    if (cancellationRequested(loadError, "method-bodies")) return false;
    buildGenericConstraintModel();

    const ProjectPhaseTimer timing(phaseTimingSink_, progressSink_, "method-bodies");
    methodBodyModel_ = std::make_unique<ProjectMethodBodyModel>();
    std::optional<std::size_t> publicSurfaceProjectOrdinal;
    if (projectGraph_.has_value() && projectGraph_->rootProject() != nullptr &&
        projectGraph_->rootProject()->outputKind() != compilation::ProjectOutputKind::Executable)
    {
        publicSurfaceProjectOrdinal = projectGraph_->projects().size() - 1U;
    }
    if (!methodBodyModel_->build(
        *nameLookup_, *memberModel_, *namespaceFunctionModel_, *inheritanceModel_, *interfaceModel_,
        genericModel_.get(), diagnostics_,
        genericConstraintModel_.get(), publicSurfaceProjectOrdinal)) return false;
    if (!enterStage(CompilationStage::TypeChecking, loadError)) return false;
    return enterStage(CompilationStage::Comptime, loadError);
}

bool ProjectCompilation::compileProjectInterfaces(
    const std::filesystem::path& projectFile,
    std::string& loadError)
{
    if (!compileProjectInheritance(projectFile, loadError))
    {
        return false;
    }
    if (cancellationRequested(loadError, "interfaces")) return false;

    const ProjectPhaseTimer timing(phaseTimingSink_, progressSink_, "interfaces");
    interfaceModel_ = std::make_unique<ProjectInterfaceModel>();
    return interfaceModel_->build(*nameLookup_, *memberModel_, *inheritanceModel_, diagnostics_);
}

bool ProjectCompilation::compileProjectVirtualDispatch(
    const std::filesystem::path& projectFile,
    std::string& loadError)
{
    if (!compileProjectMethodBodies(projectFile, loadError))
    {
        return false;
    }
    if (cancellationRequested(loadError, "virtual-dispatch")) return false;

    const ProjectPhaseTimer timing(phaseTimingSink_, progressSink_, "virtual-dispatch");
    virtualDispatchModel_ = std::make_unique<ProjectVirtualDispatchModel>();
    return virtualDispatchModel_->build(
        *nameLookup_, *memberModel_, *inheritanceModel_, diagnostics_);
}

bool ProjectCompilation::compileProjectDestruction(
    const std::filesystem::path& projectFile,
    std::string& loadError)
{
    if (!compileProjectVirtualDispatch(projectFile, loadError))
    {
        return false;
    }
    if (cancellationRequested(loadError, "destruction")) return false;

    const ProjectPhaseTimer timing(phaseTimingSink_, progressSink_, "destruction");
    destructionModel_ = std::make_unique<ProjectDestructionModel>();
    return destructionModel_->build(
        *nameLookup_, *memberModel_, *inheritanceModel_, diagnostics_);
}

bool ProjectCompilation::compileProjectConstructions(
    const std::filesystem::path& projectFile,
    std::string& loadError)
{
    if (!compileProjectDestruction(projectFile, loadError))
    {
        return false;
    }
    if (cancellationRequested(loadError, "construction")) return false;

    const ProjectPhaseTimer timing(phaseTimingSink_, progressSink_, "construction");
    constructionModel_ = std::make_unique<ProjectConstructionModel>();
    return constructionModel_->build(
        *nameLookup_, *memberModel_, *inheritanceModel_, *interfaceModel_, *methodBodyModel_, units_, diagnostics_);
}

bool ProjectCompilation::compileProjectObjectLayouts(
    const std::filesystem::path& projectFile,
    std::string& loadError)
{
    if (!compileProjectConstructions(projectFile, loadError))
    {
        return false;
    }
    if (cancellationRequested(loadError, "layout")) return false;

    const ProjectPhaseTimer timing(phaseTimingSink_, progressSink_, "layout");
    objectLayoutModel_ = std::make_unique<ProjectObjectLayoutModel>();
    return objectLayoutModel_->build(
        *nameLookup_, *memberModel_, *inheritanceModel_, *constructionModel_, diagnostics_);
}

bool ProjectCompilation::compileProjectOopHir(
    const std::filesystem::path& projectFile,
    std::string& loadError)
{
    if (!compileProjectObjectLayouts(projectFile, loadError))
    {
        return false;
    }
    if (cancellationRequested(loadError, "oop-hir")) return false;

    const ProjectPhaseTimer timing(phaseTimingSink_, progressSink_, "oop-hir");
    oopHirModel_ = std::make_unique<ProjectOopHirModel>();
    if (oopHirModel_->build(
            *nameLookup_, *memberModel_, *inheritanceModel_, *interfaceModel_, *methodBodyModel_,
            *constructionModel_, *objectLayoutModel_, loadError, genericConstraintModel_.get(),
            compilationJobs_, namespaceFunctionModel_.get()))
    {
        return true;
    }

    // EL SITIO DONDE FALLO, delante del mensaje. Es la otra mitad del envoltorio de
    // `lowerExpression` (doc 112 §A.4): aquel apunta el rango del nodo MAS INTERNO que no se pudo
    // bajar, y hasta hoy NADIE LO CONSUMIA -- se guardaba y se tiraba.
    //
    // POR QUE AQUI Y NO EN EL MODELO. `ProjectOopHirModel` no tiene `SourceManager` y no le hace
    // falta para recordar un rango: recordar es barato, traducir necesita los ficheros. Esta
    // clase ya tiene los dos. Enhebrar el `SourceManager` hasta el modelo habria sido pasarlo por
    // seis constructores para hacer aqui mismo lo que ya se puede hacer.
    //
    // El formato es `fichero:linea:columna: mensaje`, el de siempre en compiladores de C, para que
    // un editor pueda saltar al sitio.
    const SourceRange range = oopHirModel_->lastErrorRange();
    if (range.isValid() && !loadError.empty())
    {
        const source::SourcePosition position = sourceManager_.getPosition(range.begin());
        const std::string_view path = sourceManager_.getPath(range.begin());
        if (!path.empty())
        {
            loadError = std::string(path) + ':' + std::to_string(position.line) + ':' +
                std::to_string(position.column) + ": " + loadError;
        }
    }
    return false;
}

bool ProjectCompilation::compileProjectOopMir(
    const std::filesystem::path& projectFile,
    std::string& loadError)
{
    if (!compileProjectOopHir(projectFile, loadError))
    {
        return false;
    }
    if (cancellationRequested(loadError, "oop-mir")) return false;

    const ProjectPhaseTimer timing(phaseTimingSink_, progressSink_, "oop-mir");
    oopMirModel_ = std::make_unique<ProjectOopMirModel>();
    if (projectGraph_.has_value() && projectGraph_->rootProject() != nullptr)
        oopMirModel_->setLanguageEdition(projectGraph_->rootProject()->languageEdition());
    if (!oopMirModel_->build(
        *nameLookup_, *memberModel_, *objectLayoutModel_, *virtualDispatchModel_, *interfaceModel_,
        *destructionModel_, *oopHirModel_, *namespaceFunctionModel_, loadError, compilationJobs_))
        return false;
    if (!enterStage(CompilationStage::Mir, loadError)) return false;
    return enterStage(CompilationStage::Monomorphization, loadError);
}

bool ProjectCompilation::compileSyntaxCore(
    const std::filesystem::path& projectFile,
    std::string& loadError)
{
    loadError.clear();
    if (!units_.empty() || genericModel_ != nullptr || nameLookup_ != nullptr || memberModel_ != nullptr || inheritanceModel_ != nullptr || interfaceModel_ != nullptr ||
        methodBodyModel_ != nullptr || virtualDispatchModel_ != nullptr || destructionModel_ != nullptr ||
        constructionModel_ != nullptr ||
        objectLayoutModel_ != nullptr ||
        oopHirModel_ != nullptr || oopMirModel_ != nullptr)
    {
        loadError = "ProjectCompilation instances are single-use";
        return false;
    }

    if (!projectGraph_.has_value())
    {
        compilation::ProjectGraphLoader loader(compilationJobs_, {}, projectLoadOptions_);
        std::optional<compilation::ProjectGraph> graph;
        {
            const ProjectPhaseTimer timing(phaseTimingSink_, progressSink_, "project-graph");
            graph = loader.load(projectFile, loadError);
        }
        if (!graph.has_value())
        {
            return false;
        }
        projectGraph_ = std::move(*graph);
    }
    if (cancellationRequested(loadError, "syntax")) return false;
    std::vector<compilation::PrecompiledProjectDescriptor> precompiledDescriptors;
    precompiledDescriptors.reserve(precompiledModules_.size());
    for (const PrecompiledSyntaxModule& module : precompiledModules_)
    {
        precompiledDescriptors.push_back({
            module.ownerProjectFile,
            module.metadataPath,
            module.moduleName,
            module.languageEdition});
    }
    if (!projectGraph_->addPrecompiledProjects(precompiledDescriptors, loadError))
    {
        return false;
    }
    struct SourceRead final
    {
        std::size_t projectOrdinal = 0;
        std::size_t sourceOrdinal = 0;
        std::string moduleName;
        language::LanguageEdition languageEdition = language::LanguageEdition::V2;
        std::filesystem::path path;
        std::optional<std::string> buffer;
        std::optional<ComptimeResult> comptime;
        std::string error;
    };
    comptimeRecords_.clear();
    driver::CompilationWorkerPool syntaxWorkers(compilationJobs_);
    std::vector<SourceRead> sourceReads;
    for (std::size_t projectOrdinal = 0;
         projectOrdinal < projectGraph_->projects().size();
         ++projectOrdinal)
    {
        const compilation::Project& project = projectGraph_->projects()[projectOrdinal];
        for (const compilation::ProjectSource& source : project.sourceSet().sources())
        {
            sourceReads.push_back({
                projectOrdinal,
                source.ordinal,
                project.moduleName(),
                project.languageEdition(),
                source.absolutePath,
                std::nullopt,
                std::nullopt,
                {}});
        }
    }

    {
        const ProjectPhaseTimer timing(phaseTimingSink_, progressSink_, "syntax-read");
        std::atomic_size_t completed = 0;
        for (std::size_t index = 0; index < sourceReads.size(); ++index)
        {
            syntaxWorkers.submit([&, index](std::size_t) {
                if (sourceBufferProvider_)
                {
                    sourceReads[index].buffer = sourceBufferProvider_(sourceReads[index].path);
                }
                if (!sourceReads[index].buffer.has_value())
                {
                    sourceReads[index].buffer = source::SourceManager::readFileBuffer(
                        sourceReads[index].path, sourceReads[index].error);
                }
                reportSourceProgress(progressSink_, "syntax-read", sourceReads[index].path,
                    completed.fetch_add(1, std::memory_order_relaxed) + 1U,
                    sourceReads.size());
            });
        }
        if (!waitForSyntaxWorkers(syntaxWorkers, "source reading", loadError)) return false;
        for (const SourceRead& read : sourceReads)
        {
            if (!read.buffer.has_value())
            {
                loadError = read.error;
                return false;
            }
        }
    }

    {
        const ProjectPhaseTimer timing(phaseTimingSink_, progressSink_, "syntax-comptime");
        const auto comptimeCache = std::make_shared<ComptimeCache>();
        std::atomic_size_t completed = 0;
        for (std::size_t index = 0; index < sourceReads.size(); ++index)
        {
            syntaxWorkers.submit([&, index](std::size_t) {
                sourceReads[index].comptime =
                    ComptimeEngine({}, comptimeCache).execute(*sourceReads[index].buffer);
                reportSourceProgress(progressSink_, "syntax-comptime", sourceReads[index].path,
                    completed.fetch_add(1, std::memory_order_relaxed) + 1U,
                    sourceReads.size());
            });
        }
        if (!waitForSyntaxWorkers(syntaxWorkers, "comptime", loadError)) return false;
        for (SourceRead& read : sourceReads)
        {
            if (!read.comptime.has_value() || !read.comptime->success)
            {
                loadError = read.path.generic_string() + ": " +
                    (read.comptime.has_value() ? read.comptime->error : "comptime produced no result");
                return false;
            }
            const compilation::Project& owner = projectGraph_->projects()[read.projectOrdinal];
            std::error_code relativeError;
            std::filesystem::path relativePath = std::filesystem::relative(
                read.path, owner.projectFile().parent_path(), relativeError);
            if (relativeError) relativePath = read.path.filename();
            comptimeRecords_.push_back({
                read.projectOrdinal,
                read.sourceOrdinal,
                read.moduleName,
                relativePath.generic_string(),
                read.comptime->evaluations.size(),
                read.comptime->executedSteps,
                read.comptime->cacheHits,
                read.comptime->cacheMisses,
                read.comptime->reproducibilityDigest,
                read.comptime->dependencies});
            const FileId fileId = sourceManager_.addBuffer(
                read.path, std::move(read.comptime->source));
            units_.push_back(std::make_unique<ProjectSyntaxUnit>(
                read.projectOrdinal,
                read.sourceOrdinal,
                std::move(read.moduleName),
                fileId,
                false,
                read.languageEdition));
        }
    }

    std::set<std::string> materializedMetadata;
    {
        const ProjectPhaseTimer timing(phaseTimingSink_, progressSink_, "precompiled-syntax");
        for (const PrecompiledSyntaxModule& module : precompiledModules_)
        {
        std::error_code metadataKeyError;
        std::string metadataKey = std::filesystem::weakly_canonical(
            module.metadataPath, metadataKeyError).generic_string();
        if (metadataKeyError)
            metadataKey = std::filesystem::absolute(module.metadataPath).lexically_normal().generic_string();
        if (!materializedMetadata.insert(metadataKey).second) continue;

        std::optional<std::size_t> projectOrdinal;
        for (std::size_t ordinal = 0; ordinal < projectGraph_->projects().size(); ++ordinal)
        {
            std::error_code leftError;
            std::error_code rightError;
            const std::filesystem::path left = std::filesystem::weakly_canonical(
                projectGraph_->projects()[ordinal].projectFile(), leftError);
            const std::filesystem::path right =
                std::filesystem::weakly_canonical(module.metadataPath, rightError);
            if (!leftError && !rightError && left == right)
            {
                projectOrdinal = ordinal;
                break;
            }
        }
        if (!projectOrdinal.has_value())
        {
            loadError = "internal error: precompiled module '" + module.moduleName +
                "' has no project-graph node";
            return false;
        }

        std::filesystem::path displayPath = module.metadataPath;
        displayPath += ".public-api.ns";
        const FileId fileId = sourceManager_.addBuffer(displayPath, module.sourceText);
        const source::SourceFile* sourceFile = sourceManager_.getFile(fileId);
        if (sourceFile == nullptr)
        {
            loadError = "internal error: generated precompiled API has no SourceFile slot";
            return false;
        }
        auto unit = std::make_unique<ProjectSyntaxUnit>(
            *projectOrdinal,
            0,
            module.moduleName,
            fileId,
            true,
            projectGraph_->projects()[*projectOrdinal].languageEdition());
        units_.push_back(std::move(unit));

        if (!module.templateSourceText.empty())
        {
            std::filesystem::path templateDisplayPath = module.metadataPath;
            templateDisplayPath += ".public-templates.ns";
            const FileId templateFileId =
                sourceManager_.addBuffer(templateDisplayPath, module.templateSourceText);
            const source::SourceFile* templateSourceFile = sourceManager_.getFile(templateFileId);
            if (templateSourceFile == nullptr)
            {
                loadError = "internal error: generated precompiled templates have no SourceFile slot";
                return false;
            }
            // Deliberately NOT precompiled: these public bodies are input to the consumer's
            // ordinary generic pass, which specializes only what that consumer requests.
            auto templateUnit = std::make_unique<ProjectSyntaxUnit>(
                *projectOrdinal,
                1,
                module.moduleName,
                templateFileId,
                false,
                projectGraph_->projects()[*projectOrdinal].languageEdition());
            units_.push_back(std::move(templateUnit));
        }
        }
    }

    bool lexicalOk = true;
    {
        const ProjectPhaseTimer timing(phaseTimingSink_, progressSink_, "syntax-lex");
        std::atomic_size_t completed = 0;
        std::vector<diagnostics::DiagnosticEngine> localDiagnostics;
        localDiagnostics.reserve(units_.size());
        for (std::size_t index = 0; index < units_.size(); ++index)
        {
            localDiagnostics.emplace_back(diagnostics_.errorLimit());
            syntaxWorkers.submit([&, index](std::size_t) {
                const source::SourceFile* sourceFile =
                    sourceManager_.getFile(units_[index]->fileId());
                if (sourceFile == nullptr)
                {
                    throw std::logic_error("syntax unit has no SourceFile slot");
                }
                units_[index]->lex(*sourceFile, localDiagnostics[index]);
                reportSourceProgress(progressSink_, "syntax-lex",
                    sourceFile->path(),
                    completed.fetch_add(1, std::memory_order_relaxed) + 1U, units_.size());
            });
        }
        if (!waitForSyntaxWorkers(syntaxWorkers, "lexing", loadError)) return false;
        for (std::size_t index = 0; index < units_.size(); ++index)
        {
            lexicalOk = units_[index]->isLexicallyValid() && lexicalOk;
        }
        diagnostics_.mergeDeterministically(localDiagnostics);
    }

    {
        const ProjectPhaseTimer timing(phaseTimingSink_, progressSink_, "syntax-parse");
        const std::vector<std::string> knownTypeNames = collectParserKnownTypeNames(units_);
        std::atomic_size_t parseCompleted = 0;
        bool syntaxOk = lexicalOk;
        std::vector<diagnostics::DiagnosticEngine> localDiagnostics;
        localDiagnostics.reserve(units_.size());
        for (std::size_t index = 0; index < units_.size(); ++index)
        {
            localDiagnostics.emplace_back(diagnostics_.errorLimit());
            if (!units_[index]->isLexicallyValid()) continue;
            syntaxWorkers.submit([&, index](std::size_t) {
                units_[index]->parse(localDiagnostics[index], knownTypeNames);
                reportSourceProgress(progressSink_, "syntax-parse",
                    std::filesystem::path(sourceManager_.getPath(
                        SourceLocation(units_[index]->fileId(), 0U))),
                    parseCompleted.fetch_add(1, std::memory_order_relaxed) + 1U, units_.size());
            });
        }
        if (!waitForSyntaxWorkers(syntaxWorkers, "parsing", loadError)) return false;
        for (std::size_t index = 0; index < units_.size(); ++index)
        {
            syntaxOk = units_[index]->isSyntaxValid() && syntaxOk;
        }
        diagnostics_.mergeDeterministically(localDiagnostics);
        if (!syntaxOk || diagnostics_.hasErrors()) return false;
        return enterStage(CompilationStage::Parse, loadError);
    }
}

bool ProjectCompilation::enterStage(CompilationStage stage, std::string& error)
{
    return stageTracker_.enter(stage, error);
}

bool ProjectCompilation::completeCodeGeneration(std::string& error)
{
    return enterStage(CompilationStage::Codegen, error);
}

std::span<const CompilationStage> ProjectCompilation::completedStages() const noexcept
{
    return stageTracker_.completed();
}

const compilation::ProjectGraph* ProjectCompilation::projectGraph() const noexcept
{
    return projectGraph_.has_value() ? &*projectGraph_ : nullptr;
}

const source::SourceManager& ProjectCompilation::sourceManager() const noexcept
{
    return sourceManager_;
}

diagnostics::DiagnosticEngine& ProjectCompilation::diagnostics() noexcept
{
    return diagnostics_;
}

const diagnostics::DiagnosticEngine& ProjectCompilation::diagnostics() const noexcept
{
    return diagnostics_;
}

std::span<const std::unique_ptr<ProjectSyntaxUnit>> ProjectCompilation::units() const noexcept
{
    return units_;
}

std::span<const ProjectComptimeRecord> ProjectCompilation::comptimeRecords() const noexcept
{
    return comptimeRecords_;
}

const ProjectNameLookup* ProjectCompilation::nameLookup() const noexcept
{
    return nameLookup_.get();
}

const ProjectGenericModel* ProjectCompilation::genericModel() const noexcept
{
    return genericModel_.get();
}

const ProjectGenericConstraintModel* ProjectCompilation::genericConstraintModel() const noexcept
{
    return genericConstraintModel_.get();
}

const ProjectMemberModel* ProjectCompilation::memberModel() const noexcept
{
    return memberModel_.get();
}

const ProjectNamespaceFunctionModel* ProjectCompilation::namespaceFunctionModel() const noexcept
{
    return namespaceFunctionModel_.get();
}

const ProjectInheritanceModel* ProjectCompilation::inheritanceModel() const noexcept
{
    return inheritanceModel_.get();
}

const ProjectInterfaceModel* ProjectCompilation::interfaceModel() const noexcept
{
    return interfaceModel_.get();
}

const ProjectMethodBodyModel* ProjectCompilation::methodBodyModel() const noexcept
{
    return methodBodyModel_.get();
}

const ProjectVirtualDispatchModel* ProjectCompilation::virtualDispatchModel() const noexcept
{
    return virtualDispatchModel_.get();
}

const ProjectDestructionModel* ProjectCompilation::destructionModel() const noexcept
{
    return destructionModel_.get();
}

const ProjectConstructionModel* ProjectCompilation::constructionModel() const noexcept
{
    return constructionModel_.get();
}

const ProjectObjectLayoutModel* ProjectCompilation::objectLayoutModel() const noexcept
{
    return objectLayoutModel_.get();
}

const ProjectOopHirModel* ProjectCompilation::oopHirModel() const noexcept
{
    return oopHirModel_.get();
}

const ProjectOopMirModel* ProjectCompilation::oopMirModel() const noexcept
{
    return oopMirModel_.get();
}
}
