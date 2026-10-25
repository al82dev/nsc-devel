#include "ThinLtoBackend.h"

#include "nativesharp/LLVM/LlvmModule.h"
#include "nativesharp/LLVM/LlvmTargetMachine.h"

#if !defined(NSC_THINLTO_UNAVAILABLE)
#include "llvm-c/Core.h"
#include "llvm-c/Error.h"
#include "llvm-c/TargetMachine.h"
#include "llvm-c/Transforms/PassBuilder.h"
#include "llvm/ADT/StringRef.h"
#include "llvm/Analysis/ModuleSummaryAnalysis.h"
#include "llvm/Analysis/ProfileSummaryInfo.h"
#include "llvm/Bitcode/BitcodeWriter.h"
#include "llvm/Config/llvm-config.h"
#if __has_include("llvm/Support/VCSRevision.h")
#include "llvm/Support/VCSRevision.h"
#endif
#include "llvm/IR/DiagnosticInfo.h"
#include "llvm/IR/DiagnosticPrinter.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/ModuleSummaryIndex.h"
#include "llvm/LTO/Config.h"
#include "llvm/LTO/LTO.h"
#include "llvm/Object/Archive.h"
#include "llvm/Object/Binary.h"
#include "llvm/Object/ObjectFile.h"
#include "llvm/Support/CachePruning.h"
#include "llvm/Support/Caching.h"
#include "llvm/Support/CodeGen.h"
#include "llvm/Support/Error.h"
#include "llvm/Support/FileSystem.h"
#include "llvm/Support/MemoryBuffer.h"
#include "llvm/Support/Threading.h"
#include "llvm/Support/raw_ostream.h"
#endif

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <memory>
#include <unordered_map>
#include <unordered_set>
#include <utility>

namespace nativesharp::llvm_backend
{
#if defined(NSC_THINLTO_UNAVAILABLE)
ThinLtoBackend::ThinLtoBackend(std::size_t moduleCount)
    : bitcode_(moduleCount),
      identifiers_(moduleCount),
      finalModuleTexts_(moduleCount)
{
}

bool ThinLtoBackend::available() noexcept
{
    return false;
}

bool ThinLtoBackend::addModule(
    std::size_t, LlvmModule&, const LlvmTargetMachine&, LlvmOptimizationLevel, std::string& error)
{
    error = "ThinLTO needs the STATIC LLVM host";
    return false;
}

bool ThinLtoBackend::run(
    std::span<const std::filesystem::path>,
    const LlvmTargetMachine&,
    LlvmOptimizationLevel,
    std::size_t,
    const std::filesystem::path&,
    std::string& error)
{
    error = "ThinLTO needs the STATIC LLVM host";
    return false;
}

std::string ThinLtoBackend::compatibilityIdentity(const LlvmTargetMachine&, LlvmOptimizationLevel)
{
    return {};
}

bool ThinLtoBackend::collectUndefinedSymbols(
    const std::filesystem::path&, std::vector<std::string>&, std::string& error)
{
    error = "ThinLTO needs the STATIC LLVM host";
    return false;
}
#else
namespace
{
unsigned levelNumber(LlvmOptimizationLevel level) noexcept
{
    switch (level)
    {
    case LlvmOptimizationLevel::O0: return 0;
    case LlvmOptimizationLevel::O1: return 1;
    case LlvmOptimizationLevel::O2: return 2;
    case LlvmOptimizationLevel::O3: return 3;
    }
    return 0;
}

// The code generation level LlvmTargetMachine::createWindowsX64 uses for `level`.
// El nivel de generacion de codigo que usa LlvmTargetMachine::createWindowsX64 para `level`.
llvm::CodeGenOptLevel codeGenerationLevel(LlvmOptimizationLevel level) noexcept
{
    switch (level)
    {
    case LlvmOptimizationLevel::O0: return llvm::CodeGenOptLevel::None;
    case LlvmOptimizationLevel::O1: return llvm::CodeGenOptLevel::Less;
    case LlvmOptimizationLevel::O2: return llvm::CodeGenOptLevel::Default;
    case LlvmOptimizationLevel::O3: return llvm::CodeGenOptLevel::Aggressive;
    }
    return llvm::CodeGenOptLevel::None;
}

std::vector<std::string> splitFeatures(const std::string& features)
{
    std::vector<std::string> result;
    llvm::SmallVector<llvm::StringRef, 16> parts;
    llvm::StringRef(features).split(parts, ',', -1, false);
    for (const llvm::StringRef part : parts) result.emplace_back(part.str());
    return result;
}

// Appends the undefined symbols of one COFF object. Agrega los simbolos indefinidos de un objeto COFF.
bool appendUndefinedSymbols(
    const llvm::object::ObjectFile& object,
    std::vector<std::string>& symbols,
    std::string& error)
{
    for (const llvm::object::SymbolRef& symbol : object.symbols())
    {
        llvm::Expected<std::uint32_t> flags = symbol.getFlags();
        if (!flags)
        {
            error = llvm::toString(flags.takeError());
            return false;
        }
        if ((*flags & llvm::object::SymbolRef::SF_Undefined) == 0) continue;
        llvm::Expected<llvm::StringRef> name = symbol.getName();
        if (!name)
        {
            error = llvm::toString(name.takeError());
            return false;
        }
        symbols.push_back(name->str());
    }
    return true;
}

std::size_t importedDefinitions(const llvm::Module& module) noexcept
{
    std::size_t count = 0;
    for (const llvm::Function& function : module)
        if (!function.isDeclaration() && function.hasAvailableExternallyLinkage()) ++count;
    return count;
}
}

ThinLtoBackend::ThinLtoBackend(std::size_t moduleCount)
    : bitcode_(moduleCount),
      identifiers_(moduleCount),
      finalModuleTexts_(moduleCount)
{
}

bool ThinLtoBackend::available() noexcept
{
    return true;
}

bool ThinLtoBackend::collectUndefinedSymbols(
    const std::filesystem::path& file,
    std::vector<std::string>& symbols,
    std::string& error)
{
    llvm::Expected<llvm::object::OwningBinary<llvm::object::Binary>> binary =
        llvm::object::createBinary(file.string());
    if (!binary)
    {
        error = "cannot read '" + file.string() + "': " + llvm::toString(binary.takeError());
        return false;
    }
    llvm::object::Binary* contents = binary->getBinary();
    if (const auto* object = llvm::dyn_cast<llvm::object::ObjectFile>(contents))
        return appendUndefinedSymbols(*object, symbols, error);
    const auto* archive = llvm::dyn_cast<llvm::object::Archive>(contents);
    // Any other format (an import-only file, for instance) references nothing of the program.
    // Cualquier otro formato (un archivo solo de imports, por ejemplo) no referencia al programa.
    if (archive == nullptr) return true;
    llvm::Error childError = llvm::Error::success();
    for (const llvm::object::Archive::Child& child : archive->children(childError))
    {
        llvm::Expected<std::unique_ptr<llvm::object::Binary>> member = child.getAsBinary();
        if (!member)
        {
            error = "cannot read a member of '" + file.string() + "': " + llvm::toString(member.takeError());
            llvm::consumeError(std::move(childError));
            return false;
        }
        const auto* object = llvm::dyn_cast<llvm::object::ObjectFile>(member->get());
        if (object != nullptr && !appendUndefinedSymbols(*object, symbols, error))
        {
            llvm::consumeError(std::move(childError));
            return false;
        }
    }
    if (childError)
    {
        error = "cannot list the members of '" + file.string() + "': " + llvm::toString(std::move(childError));
        return false;
    }
    return true;
}

std::string ThinLtoBackend::compatibilityIdentity(const LlvmTargetMachine& targetMachine, LlvmOptimizationLevel level)
{
    // The exact source revision of LLVM, when this LLVM build records one (item 53).
    // La revision exacta de las fuentes de LLVM, cuando este build de LLVM la registra (punto 53).
#if defined(LLVM_REVISION)
    const std::string revision = std::string("llvm-revision ") + LLVM_REVISION + "\n";
#else
    const std::string revision = "llvm-revision none\n";
#endif
    return std::string("llvm ") + LLVM_VERSION_STRING + "\n" + revision +
        "triple " + targetMachine.triple() + "\n" +
        "cpu " + targetMachine.cpuName() + "\n" +
        "features " + targetMachine.featureString() + "\n" +
        "layout " + targetMachine.dataLayout() + "\n" +
        "optimization O" + std::to_string(levelNumber(level)) + "\n";
}

bool ThinLtoBackend::addModule(
    std::size_t ordinal,
    LlvmModule& module,
    const LlvmTargetMachine& targetMachine,
    LlvmOptimizationLevel level,
    std::string& error)
{
    if (ordinal >= bitcode_.size() || module.module_ == nullptr || targetMachine.targetMachine_ == nullptr)
    {
        error = "ThinLTO received an invalid code unit";
        return false;
    }
    LLVMModuleRef moduleRef = static_cast<LLVMModuleRef>(module.module_);
    LLVMPassBuilderOptionsRef options = LLVMCreatePassBuilderOptions();
    const std::string pipeline = "thinlto-pre-link<O" + std::to_string(levelNumber(level)) + ">";
    LLVMErrorRef failure = LLVMRunPasses(
        moduleRef, pipeline.c_str(), static_cast<LLVMTargetMachineRef>(targetMachine.targetMachine_), options);
    LLVMDisposePassBuilderOptions(options);
    if (failure != nullptr)
    {
        char* message = LLVMGetErrorMessage(failure);
        error = "ThinLTO pre-link pipeline failed: " + std::string(message != nullptr ? message : "");
        LLVMDisposeErrorMessage(message);
        return false;
    }
    llvm::Module& unwrapped = *llvm::unwrap(moduleRef);
    llvm::ProfileSummaryInfo profile(unwrapped);
    const llvm::ModuleSummaryIndex index = llvm::buildModuleSummaryIndex(unwrapped, nullptr, &profile);
    std::string bytes;
    llvm::raw_string_ostream stream(bytes);
    // The module hash (GenerateHash) is what lets LLVM cache this unit's backend; without it the
    // in-process backend always runs.
    // El hash de modulo (GenerateHash) es lo que permite a LLVM cachear el backend de esta unidad;
    // sin el, el backend en proceso siempre corre.
    llvm::WriteBitcodeToFile(unwrapped, stream, false, &index, true);
    stream.flush();
    bitcode_[ordinal] = std::move(bytes);
    // ThinLTO keys modules by identifier, so it is the stable ordinal and never a path.
    // ThinLTO identifica los modulos por su identificador: el ordinal estable, nunca una ruta.
    identifiers_[ordinal] = "nativesharp-thinlto-unit-" + std::to_string(ordinal);
    return true;
}

bool ThinLtoBackend::run(
    std::span<const std::filesystem::path> objectFiles,
    const LlvmTargetMachine& targetMachine,
    LlvmOptimizationLevel level,
    std::size_t threads,
    const std::filesystem::path& cacheDirectory,
    std::string& error)
{
    if (objectFiles.size() != bitcode_.size())
    {
        error = "ThinLTO object list does not match its code units";
        return false;
    }
    importedFunctions_ = 0;
    cachedBackends_ = 0;
    backendRuns_ = 0;
    llvm::lto::Config config;
    config.CPU = targetMachine.cpuName();
    config.MAttrs = splitFeatures(targetMachine.featureString());
    config.DefaultTriple = targetMachine.triple();
    config.OptLevel = levelNumber(level);
    config.CGOptLevel = codeGenerationLevel(level);
    // LLVMCreateTargetMachine runs with LLVMRelocDefault and LLVMCodeModelDefault; let LLVM pick the
    // same effective models instead of the LTO defaults.
    // LLVMCreateTargetMachine usa LLVMRelocDefault y LLVMCodeModelDefault; LLVM elige los mismos
    // modelos efectivos en lugar de los valores por omision de LTO.
    config.RelocModel = std::nullopt;
    config.CodeModel = std::nullopt;
    // A final link drops what nothing references only when each function and data object has its
    // own section (item 50). Un enlace final solo tira lo que nadie referencia cuando cada funcion y
    // dato tiene su propia seccion (punto 50).
    config.Options.FunctionSections = internalize_;
    config.Options.DataSections = internalize_;
    config.DiagHandler = [this](const llvm::DiagnosticInfo& info)
    {
        if (info.getSeverity() != llvm::DS_Error) return;
        std::string text;
        llvm::raw_string_ostream stream(text);
        llvm::DiagnosticPrinterRawOStream printer(stream);
        info.print(printer);
        stream.flush();
        const std::lock_guard lock(diagnosticsMutex_);
        diagnostics_ += (diagnostics_.empty() ? "" : "; ") + text;
    };
    // Runs once per backend that really executes; a backend served by the cache skips it.
    // Se ejecuta una vez por backend que de verdad corre; uno servido por el cache no pasa aqui.
    config.PostImportModuleHook = [this](unsigned, const llvm::Module& module)
    {
        ++backendRuns_;
        importedFunctions_ += importedDefinitions(module);
        return true;
    };
    std::unordered_map<std::string, std::size_t> ordinalByIdentifier;
    for (std::size_t ordinal = 0; ordinal < identifiers_.size(); ++ordinal)
        ordinalByIdentifier.emplace(identifiers_[ordinal], ordinal);
    const auto ordinalFor = [&](unsigned task, const llvm::Twine& moduleName) -> std::size_t
    {
        const auto found = ordinalByIdentifier.find(moduleName.str());
        if (found != ordinalByIdentifier.end()) return found->second;
        return task == 0 ? objectFiles.size() : static_cast<std::size_t>(task - 1U);
    };
    if (captureFinalModuleTexts_)
    {
        config.PostOptModuleHook = [this](unsigned task, const llvm::Module& module)
        {
            const std::size_t ordinal = task == 0 ? finalModuleTexts_.size() : task - 1U;
            if (ordinal >= finalModuleTexts_.size()) return true;
            std::string text;
            llvm::raw_string_ostream stream(text);
            module.print(stream, nullptr);
            stream.flush();
            finalModuleTexts_[ordinal] = std::move(text);
            return true;
        };
    }
    llvm::lto::ThinBackend backend = llvm::lto::createInProcessThinBackend(
        llvm::heavyweight_hardware_concurrency(static_cast<unsigned>(threads == 0 ? 1 : threads)));
    llvm::lto::LTO lto(std::move(config), std::move(backend));
    std::unordered_set<std::string> prevailing;
    for (std::size_t ordinal = 0; ordinal < bitcode_.size(); ++ordinal)
    {
        llvm::Expected<std::unique_ptr<llvm::lto::InputFile>> input =
            llvm::lto::InputFile::create(llvm::MemoryBufferRef(bitcode_[ordinal], identifiers_[ordinal]));
        if (!input)
        {
            error = "ThinLTO cannot read code unit " + std::to_string(ordinal) + ": " +
                llvm::toString(input.takeError());
            return false;
        }
        std::vector<llvm::lto::SymbolResolution> resolutions;
        for (const llvm::lto::InputFile::Symbol& symbol : (*input)->symbols())
        {
            llvm::lto::SymbolResolution resolution;
            if (!symbol.isUndefined())
            {
                resolution.Prevailing = prevailing.insert(symbol.getName().str()).second;
                resolution.FinalDefinitionInLinkageUnit = resolution.Prevailing;
                resolution.VisibleToRegularObj =
                    !internalize_ || preservedSymbols_.contains(symbol.getName().str());
            }
            resolutions.push_back(resolution);
        }
        if (llvm::Error added = lto.add(std::move(*input), resolutions))
        {
            error = "ThinLTO cannot add code unit " + std::to_string(ordinal) + ": " +
                llvm::toString(std::move(added));
            return false;
        }
    }
    // The backends write objects without an object path: LLVM records a stream's path as the
    // CodeView object name (S_OBJNAME), which made two identical -g builds differ by the stamp of
    // the temporary directory and a cached build differ from an uncached one.
    // Los backends escriben objetos sin ruta de objeto: LLVM guarda la ruta del stream como nombre
    // de objeto CodeView (S_OBJNAME), lo que hacia diferir dos builds -g identicos por el sello del
    // directorio temporal y un build con cache de uno sin cache.
    const llvm::AddStreamFn addStream = [&](unsigned task, const llvm::Twine& moduleName)
        -> llvm::Expected<std::unique_ptr<llvm::CachedFileStream>>
    {
        const std::size_t ordinal = ordinalFor(task, moduleName);
        if (ordinal >= objectFiles.size())
            return llvm::createStringError("ThinLTO produced an unexpected task " + std::to_string(task));
        std::error_code openError;
        auto output = std::make_unique<llvm::raw_fd_ostream>(
            objectFiles[ordinal].string(), openError, llvm::sys::fs::OF_None);
        if (openError) return llvm::errorCodeToError(openError);
        return std::make_unique<llvm::CachedFileStream>(std::move(output));
    };
    // The backend cache keeps one raw object per LLVM cache key, named llvmcache-<key> so that
    // llvm::pruneCache bounds it. A hit copies the entry to the unit's object; a miss lets the
    // backend write the object and stores a copy after the run.
    // El cache de backends guarda un objeto crudo por llave de LLVM, llamado llvmcache-<llave> para
    // que llvm::pruneCache lo acote. Un acierto copia la entrada al objeto de la unidad; un fallo deja
    // que el backend escriba el objeto y guarda una copia al terminar.
    std::mutex missesMutex;
    std::vector<std::pair<std::size_t, std::string>> misses;
    llvm::FileCache backendCache;
    if (!cacheDirectory.empty())
    {
        std::error_code createError;
        std::filesystem::create_directories(cacheDirectory, createError);
        if (createError)
        {
            error = "ThinLTO cannot create its backend cache: " + createError.message();
            return false;
        }
        backendCache = llvm::FileCache(
            [&](unsigned task, llvm::StringRef key, const llvm::Twine& moduleName) -> llvm::Expected<llvm::AddStreamFn>
            {
                const std::size_t ordinal = ordinalFor(task, moduleName);
                if (ordinal >= objectFiles.size())
                    return llvm::createStringError("ThinLTO produced an unexpected task " + std::to_string(task));
                const std::filesystem::path entry = cacheDirectory / ("llvmcache-" + key.str());
                std::error_code copyError;
                if (std::filesystem::is_regular_file(entry, copyError) &&
                    std::filesystem::copy_file(
                        entry, objectFiles[ordinal], std::filesystem::copy_options::overwrite_existing, copyError))
                    return llvm::AddStreamFn();
                const std::lock_guard lock(missesMutex);
                misses.emplace_back(ordinal, key.str());
                return addStream;
            },
            cacheDirectory.string());
    }
    if (llvm::Error failed = lto.run(addStream, backendCache))
    {
        error = "ThinLTO failed: " + llvm::toString(std::move(failed));
        if (!diagnostics_.empty()) error += " (" + diagnostics_ + ")";
        return false;
    }
    cachedBackends_ = bitcode_.size() - std::min<std::size_t>(backendRuns_, bitcode_.size());
    if (!cacheDirectory.empty() && diagnostics_.empty())
    {
        // Publishing is best effort: a failed copy only costs a future miss.
        // Publicar es mejor esfuerzo: una copia fallida solo cuesta un fallo futuro.
        for (const auto& [ordinal, key] : misses)
        {
            const std::filesystem::path entry = cacheDirectory / ("llvmcache-" + key);
            std::filesystem::path temporary = entry;
            temporary += ".partial-" + std::to_string(ordinal);
            std::error_code storeError;
            if (std::filesystem::copy_file(
                    objectFiles[ordinal], temporary, std::filesystem::copy_options::overwrite_existing, storeError))
                std::filesystem::rename(temporary, entry, storeError);
            if (storeError) std::filesystem::remove(temporary, storeError);
        }
    }
    if (!cacheDirectory.empty())
    {
        // The backend cache lives beside the artifact cache but outside its pruning index, so it is
        // bounded here: entries unused for a week go, and the directory stays under 1 GiB.
        // El cache de backends vive junto al cache de artefactos pero fuera de su indice de poda, asi
        // que se acota aqui: salen las entradas sin uso en una semana y el directorio queda bajo 1 GiB.
        llvm::CachePruningPolicy policy;
        policy.Expiration = std::chrono::hours(24 * 7);
        policy.MaxSizeBytes = 1024ULL * 1024ULL * 1024ULL;
        // Pruning is housekeeping: a failure keeps the cache and never fails the build.
        // La poda es mantenimiento: si falla se conserva el cache y el build no falla.
        llvm::Expected<bool> pruned = llvm::pruneCache(cacheDirectory.string(), policy);
        if (!pruned) llvm::consumeError(pruned.takeError());
    }
    if (!diagnostics_.empty())
    {
        error = "ThinLTO reported errors: " + diagnostics_;
        return false;
    }
    return true;
}

#endif

void ThinLtoBackend::appendBitcodeUnit(std::string bitcode, std::string identifier)
{
    bitcode_.push_back(std::move(bitcode));
    identifiers_.push_back(std::move(identifier));
    finalModuleTexts_.emplace_back();
}

std::span<const std::string> ThinLtoBackend::unitBitcode() const noexcept
{
    return bitcode_;
}

void ThinLtoBackend::setPreservedSymbols(std::vector<std::string> symbols)
{
    internalize_ = true;
    preservedSymbols_.clear();
    for (std::string& symbol : symbols) preservedSymbols_.insert(std::move(symbol));
}

void ThinLtoBackend::setCaptureFinalModuleTexts(bool capture) noexcept
{
    captureFinalModuleTexts_ = capture;
}

std::span<const std::string> ThinLtoBackend::finalModuleTexts() const noexcept
{
    return finalModuleTexts_;
}

std::size_t ThinLtoBackend::importedFunctionCount() const noexcept
{
    return importedFunctions_;
}

std::size_t ThinLtoBackend::cachedBackendCount() const noexcept
{
    return cachedBackends_;
}
}
