#include "OopLlvmImplementation.h"
#include "DropCase.h"
#include "LlvmInstrumentationSwitches.h"
#include "LlvmExpressionFrame.h"
#include "MatchValuePop.h"
#include "../Lowering/OopLlvmSupport.h"

#include <fstream>

namespace nativesharp::llvm_backend
{
using namespace llvm_api;

OopLlvmImplementation::OopLlvmImplementation(
        const frontend::ProjectNameLookup& nameLookup,
        const frontend::ProjectMemberModel* memberModel,
        const frontend::ProjectOopMirModel& mirModel,
        const source::SourceManager* sourceManager,
        DebugInformationKind debugInformation,
        LlvmOptimizationLevel optimizationLevel,
        std::span<const DebugPrefixMap> debugPrefixMaps ,
        bool partitioned ,
        bool ownsModuleGlobals ,
        std::span<const std::size_t> functionOrdinals ,
        std::span<const std::size_t> declarationOrdinals ,
        std::span<const std::uint8_t> reachableDefinitions ,
        std::span<const std::uint8_t> staticFieldDeclarations ,
        std::string_view moduleName )
        : nameLookup_(nameLookup),
          memberModel_(memberModel),
          mirModel_(mirModel),
          sourceManager_(sourceManager),
          debugInformation_(debugInformation),
          optimizationLevel_(optimizationLevel),
          debugPrefixMaps_(debugPrefixMaps.begin(), debugPrefixMaps.end()),
          partitioned_(partitioned),
          ownsModuleGlobals_(ownsModuleGlobals),
          ownedReachableDefinitions_(reachableDefinitions.empty()
                  ? projectOopReachableDefinitionMask(mirModel)
                  : std::vector<std::uint8_t>{}),
          reachableDefinitions_(reachableDefinitions.empty()
                  ? std::span<const std::uint8_t>(ownedReachableDefinitions_)
                  : reachableDefinitions),
          functionDefinitions_(mirModel.functions().size(), false),
          functionDeclarations_(mirModel.functions().size(), false),
          restrictFunctionDeclarations_(!declarationOrdinals.empty()),
          ownedStaticFieldDeclarations_(staticFieldDeclarations.empty()
                  ? projectOopStaticFieldDeclarationMask(
                        mirModel, functionOrdinals, partitioned, ownsModuleGlobals)
                  : std::vector<std::uint8_t>{}),
          staticFieldDeclarations_(staticFieldDeclarations.empty()
                  ? std::span<const std::uint8_t>(ownedStaticFieldDeclarations_)
                  : staticFieldDeclarations),
          moduleName_(moduleName)
{
        for (const std::size_t ordinal : functionOrdinals)
        {
            if (ordinal < functionDefinitions_.size()) functionDefinitions_[ordinal] = true;
            else invalidFunctionOrdinal_ = ordinal;
        }
        for (const std::size_t ordinal : declarationOrdinals)
        {
            if (ordinal < functionDeclarations_.size()) functionDeclarations_[ordinal] = true;
            else invalidFunctionOrdinal_ = ordinal;
        }
        for (std::size_t ordinal = 0; ordinal < functionDefinitions_.size(); ++ordinal)
            functionDeclarations_[ordinal] = functionDeclarations_[ordinal] || functionDefinitions_[ordinal];
    }

bool OopLlvmImplementation::lower(std::string& error)
{
        error.clear();
        if (invalidFunctionOrdinal_.has_value())
        {
            error = "LLVM code partition refers to invalid MIR function ordinal " +
                std::to_string(*invalidFunctionOrdinal_);
            return false;
        }
        context_ = LLVMContextCreate();
        module_ = LLVMModuleCreateWithNameInContext(moduleName_.c_str(), context_);
        builder_ = LLVMCreateBuilderInContext(context_);
        if (context_ == nullptr || module_ == nullptr || builder_ == nullptr)
        {
            error = "Stage 34 failed to create LLVM context/module/builder";
            dispose();
            return false;
        }
        LLVMSetTarget(module_, kTargetTriple);

        if (!initializeDebugInformation(error))
        {
            dispose();
            return false;
        }

        if (!tracePhase("declare-runtime", [&] { return declareRuntime(error); }) ||
            !tracePhase("declare-static-fields", [&] {
                return declareStaticFields(error, !partitioned_ || ownsModuleGlobals_); }) ||
            !tracePhase("declare-functions", [&] { return declareFunctions(error); }) ||
            !tracePhase("declare-type-initializers", [&] {
                return declareTypeInitializers(error, !partitioned_ || ownsModuleGlobals_); }) ||
            !tracePhase("declare-destruction", [&] { return declareDestructionEntries(error); }) ||
            !tracePhase("declare-value-drop", [&] { return declareValueDropEntries(error); }) ||
            !tracePhase("dispatch-metadata", [&] {
                return emitDispatchMetadata(error, !partitioned_ || ownsModuleGlobals_); }) ||
            !tracePhase("rtti-helpers", [&] { return defineRttiHelpers(error); }) ||
            !tracePhase("typed-catch-filters", [&] { return defineTypedCatchFilters(error); }) ||
            ((!partitioned_ || ownsModuleGlobals_) &&
             (!tracePhase("define-type-initializers", [&] { return defineTypeInitializers(error); }) ||
              !tracePhase("define-static-accessors", [&] { return defineStaticFieldAccessors(error); }))) ||
            !tracePhase("callback-thunks", [&] {
                return defineCallbackGuardThunks(error, !partitioned_ || ownsModuleGlobals_); }) ||
            !tracePhase("define-functions", [&] { return defineFunctions(error); }) ||
            !tracePhase("static-local-destructors", [&] {
                return defineStaticLocalDestructors(error); }) ||
            ((!partitioned_ || ownsModuleGlobals_) &&
             (!tracePhase("c-abi-wrappers", [&] { return defineCAbiWrappers(error); }) ||
              !tracePhase("value-drop", [&] { return defineValueDropEntries(error); }) ||
              !tracePhase("destruction", [&] { return defineDestructionEntries(error); }))))
        {
            dispose();
            return false;
        }

        if (debugBuilder_ != nullptr)
        {
            LLVMDIBuilderFinalize(debugBuilder_);
            LLVMDisposeDIBuilder(debugBuilder_);
            debugBuilder_ = nullptr;
        }

        LLVMDisposeBuilder(builder_);
        builder_ = nullptr;
        char* verificationMessage = nullptr;
        const int broken = LLVMVerifyModule(module_, LLVMReturnStatusAction, &verificationMessage);
        if (broken != 0)
        {
            error = "Stage 34 LLVM verifier rejected OOP module";
            if (verificationMessage != nullptr)
            {
                error += ": ";
                error += verificationMessage;
            }
            if (verificationMessage != nullptr) LLVMDisposeMessage(verificationMessage);
            dispose();
            return false;
        }
        if (verificationMessage != nullptr) LLVMDisposeMessage(verificationMessage);
        return true;
    }

LLVMContextRef OopLlvmImplementation::releaseContext() noexcept
{
        LLVMContextRef result = context_;
        context_ = nullptr;
        return result;
    }

LLVMModuleRef OopLlvmImplementation::releaseModule() noexcept
{
        LLVMModuleRef result = module_;
        module_ = nullptr;
        return result;
    }

template <typename Callback>
    bool OopLlvmImplementation::tracePhase(std::string_view phase, Callback&& callback)
{
        if constexpr (!kLoweringPhaseTraceEnabled) return callback();
        // INSTRUMENTATION CODE: NSC305-L06G. File-backed because the real compiler is launched
        // by a GUI. Remove only after the remaining coordinator cost has a measured owner.
        const auto started = std::chrono::steady_clock::now();
        const bool result = callback();
        const std::uint64_t elapsed = static_cast<std::uint64_t>(
            std::chrono::duration_cast<std::chrono::microseconds>(
                std::chrono::steady_clock::now() - started).count());
        std::call_once(loweringTraceReset_, [] {
            std::ofstream clear(R"(A:\temp\nsc-llvm-lowering-phases.log)",
                std::ios::binary | std::ios::trunc);
            if (clear) clear << "NSC305-L06G LLVM LOWERING PHASE INSTRUMENTATION version=1\n";
        });
        const std::lock_guard<std::mutex> lock(loweringTraceMutex_);
        std::ofstream output(R"(A:\temp\nsc-llvm-lowering-phases.log)",
            std::ios::binary | std::ios::app);
        if (output)
            output << "module=" << moduleName_ << " phase=" << phase
                   << " us=" << elapsed << " ok=" << (result ? 1 : 0) << '\n';
        return result;
    }

void OopLlvmImplementation::dispose() noexcept
{
        if (debugBuilder_ != nullptr)
        {
            LLVMDisposeDIBuilder(debugBuilder_);
            debugBuilder_ = nullptr;
        }
        if (builder_ != nullptr)
        {
            LLVMDisposeBuilder(builder_);
            builder_ = nullptr;
        }
        if (module_ != nullptr)
        {
            LLVMDisposeModule(module_);
            module_ = nullptr;
        }
        if (context_ != nullptr)
        {
            LLVMContextDispose(context_);
            context_ = nullptr;
        }
    }

}
