#include "../Internal/OopLlvmImplementation.h"
#include "../Internal/DropCase.h"
#include "../Internal/LlvmExpressionFrame.h"
#include "../Internal/MatchValuePop.h"
#include "OopLlvmSupport.h"

namespace nativesharp::llvm_backend
{
using namespace llvm_api;

bool OopLlvmImplementation::defineFunction(const frontend::ProjectOopMirFunction& function, std::string& error)
{
        const auto found = functions_.find(function.mangledName);
        if (found == functions_.end())
        {
            error = "Stage 34 internal function declaration missing";
            return false;
        }
        prepareFunctionDefinitionState(function, found->second);
        LLVMBasicBlockRef entry = appendBlock("entry");
        enterBlock(entry);
        constructionThisOverride_ = nullptr;
        if (!emitRuntimeDebugTrace(1U, runtimeDebugCallableRecord(function, "ENTER"), error))
            return false;
        const bool sretReturn = function.abi.returnPassing ==
            frontend::ProjectOopMirAbiPassing::Indirect;
        if (!initializeFunctionReceiver(function, error) ||
            !initializeCapturedThis(function, error) ||
            !initializeFunctionParameters(function, sretReturn, error)) return false;
        emitClosureCaptureDebugVariables(function);

        // NS304-T05-F10: expose the future's state discriminant as an artificial debug variable
        // (offset 0) so a debugger can inspect 0/-1/1..N. Only for the poll (self = parameter 0).
        emitPollStateDebugVariable(function);
        if (!prepareFunctionCleanupState(function, error)) return false;
        // EN: Freeze 3.0 C-02: a poll/MoveNext dispatches on its state before the body.
        // ES: Congelamiento 3.0 C-02: un poll/MoveNext despacha sobre su estado antes del cuerpo.
        if (!beginCoroutineDispatch(function, error)) return false;
        if (!lowerStatement(function.body.get(), error)) return false;
        if (!finishCoroutineDispatch(error)) return false;
        return finishFunctionDefinition(function, error);
    }

std::string OopLlvmImplementation::localValueName(
        std::size_t localOrdinal,
        std::string_view prefix,
        std::string_view suffix)
{
        const auto [entry, inserted] = localNameNumbers_.try_emplace(localOrdinal, localNameNumbers_.size());
        (void)inserted;
        return std::string(prefix) + std::to_string(entry->second) + std::string(suffix);
}

void OopLlvmImplementation::prepareFunctionDefinitionState(
        const frontend::ProjectOopMirFunction& function,
        const FunctionInfo& info)
{
        currentFunction_ = info.function;
        currentMirFunction_ = &function;
        currentDebugSubprogram_ = info.subprogram;
        currentDebugScope_ = currentDebugSubprogram_;
        debugDeclaredLocals_.clear();
        debugCaptureVariables_.clear();
        closureCaptureAddresses_.clear();
        currentDebugFileKey_ = 0;
        if (function.namespaceFunctionOrdinal.has_value() && function.body != nullptr &&
            function.body->syntax != nullptr && function.body->syntax->range().isValid())
            currentDebugFileKey_ = function.body->syntax->range().begin().fileId().value();
        else if (memberModel_ != nullptr && function.memberOrdinal < memberModel_->members().size())
            currentDebugFileKey_ = memberModel_->members()[function.memberOrdinal].fileId.value();
        LLVMSetCurrentDebugLocation2(
            builder_, currentDebugSubprogram_ == nullptr ? nullptr :
                LLVMDIBuilderCreateDebugLocation(context_, 0, 0, currentDebugSubprogram_, nullptr));
        parameterStorage_.clear();
        localStorage_.clear();
        localNameNumbers_.clear();
        parameterValueDropFlags_.clear();
        localValueDropFlags_.clear();
        stackObjectStorage_.clear();
        parameterCaptureCells_.clear();
        localCaptureCells_.clear();
        coroutineCapturedLocals_.clear();
        thisCaptureCell_ = nullptr;
        ownedCaptureCells_.clear();
        captureCellExceptionalCleanupBlock_ = nullptr;
        activeCatchExceptions_.clear();
        loopStack_.clear();
        exceptionDispatchBlocks_.clear();
        cleanupRegions_.clear();
        cleanupChainCache_.clear();
        activeCleanupRegions_.clear();
        exceptionHandlerStack_.clear();
        funcletStack_.clear();
        returnStorage_ = nullptr;
        sretStorage_ = nullptr;
        blockCounter_ = 0;
    }

bool OopLlvmImplementation::initializeFunctionReceiver(
        const frontend::ProjectOopMirFunction& function,
        std::string& error)
{
        if (function.isStatic)
        {
            thisValue_ = nullptr;
            closureEnvironment_ = nullptr;
            if (!function.isLambda && !function.namespaceFunctionOrdinal.has_value() &&
                !ensureTypeInitialized(function.declaringTypeOrdinal, error)) return false;
        }
        else if (function.isLambda)
        {
            thisValue_ = nullptr;
            closureEnvironment_ = LLVMGetParam(currentFunction_, 0);
            LLVMSetValueName2(closureEnvironment_, "closure.env", 11);
        }
        else
        {
            closureEnvironment_ = nullptr;
            thisValue_ = LLVMGetParam(currentFunction_, 0);
            LLVMSetValueName2(thisValue_, "this", 4);
            if (!emitDebugThis(function, error)) return false;
        }
        if (function.constructor && !function.isStatic &&
            !ensureTypeInitialized(function.declaringTypeOrdinal, error)) return false;
        return true;
    }

bool OopLlvmImplementation::initializeCapturedThis(
        const frontend::ProjectOopMirFunction& function,
        std::string& error)
{
        if (!function.capturesThisCell) return true;
        if (thisValue_ == nullptr) return fail(error, "E11 captured this has no receiver value");
        LLVMValueRef valueAddress = nullptr;
        const std::string typeKey = "type:" +
            nameLookup_.namedTypes()[function.declaringTypeOrdinal].identity.key();
        thisCaptureCell_ = allocateCaptureCell(typeKey, valueAddress, "capture.this", error);
        if (thisCaptureCell_ == nullptr || valueAddress == nullptr) return false;
        LLVMValueRef capturedThis = thisValue_;
        if (isStructValue(typeKey))
        {
            LLVMTypeRef type = llvmType(typeKey, error);
            if (type == nullptr) return false;
            capturedThis = LLVMBuildLoad2(builder_, type, thisValue_, "capture.this.load");
        }
        LLVMBuildStore(builder_, capturedThis, valueAddress);
        ownedCaptureCells_.push_back(thisCaptureCell_);
        return true;
    }

bool OopLlvmImplementation::initializeFunctionParameters(
        const frontend::ProjectOopMirFunction& function,
        bool sretReturn,
        std::string& error)
{
        sretStorage_ = nullptr;
        if (sretReturn)
        {
            sretStorage_ = LLVMGetParam(
                currentFunction_, static_cast<unsigned>(*function.abi.hiddenReturnParameterOrdinal));
            LLVMSetValueName2(sretStorage_, "sret.out", 8);
        }
        for (std::size_t index = 0; index < function.parameterTypeKeys.size(); ++index)
        {
            if (!initializeFunctionParameter(function, index, error)) return false;
        }
        return true;
    }

bool OopLlvmImplementation::initializeFunctionParameter(
        const frontend::ProjectOopMirFunction& function,
        std::size_t index,
        std::string& error)
{
        LLVMValueRef value = LLVMGetParam(
            currentFunction_, static_cast<unsigned>(function.abi.parameterPhysicalOrdinals[index]));
        const std::string parameterName = "p" + std::to_string(index);
        LLVMSetValueName2(value, parameterName.c_str(), parameterName.size());
        const bool captured = std::ranges::find(
            function.capturedParameterOrdinals, index) != function.capturedParameterOrdinals.end();
        if (captured)
        {
            LLVMValueRef valueAddress = nullptr;
            LLVMValueRef cell = allocateCaptureCell(
                function.parameterTypeKeys[index], valueAddress,
                "capture.p" + std::to_string(index), error);
            if (cell == nullptr || valueAddress == nullptr) return false;
            if (function.abi.parameterPassing[index] == frontend::ProjectOopMirAbiPassing::Indirect)
            {
                LLVMTypeRef type = llvmType(function.parameterTypeKeys[index], error);
                if (type == nullptr) return false;
                value = LLVMBuildLoad2(builder_, type, value, "capture.param.aggregate.load");
            }
            LLVMBuildStore(builder_, value, valueAddress);
            parameterStorage_[index] = valueAddress;
            parameterCaptureCells_[index] = cell;
            ownedCaptureCells_.push_back(cell);
            return emitDebugParameter(function, index, valueAddress, error);
        }
        if (function.abi.parameterPassing[index] == frontend::ProjectOopMirAbiPassing::Indirect)
        {
            // Win64 indirect aggregate: the incoming value IS a pointer to a copy the caller
            // made for THIS call, and that memory belongs to the call -- the callee may read
            // and write it freely (clang re-copies when forwarding, which is what makes this
            // safe). So the pointer itself is the parameter home: member GEPs, whole-value
            // loads and stores all work through it unchanged, and no second copy is made.
            parameterStorage_[index] = value;
            return emitDebugParameter(function, index, value, error);
        }
        LLVMTypeRef type = llvmType(function.parameterTypeKeys[index], error);
        if (type == nullptr) return false;
        // ES: Toda ranura de pila de la función vive en el bloque de entrada (ver
        // collectLocalStorage).
        // EN: Every stack slot of the function lives in the entry block (see collectLocalStorage).
        LLVMValueRef storage = entryBlockAlloca(type, 0, (parameterName + ".addr").c_str());
        LLVMBuildStore(builder_, value, storage);
        parameterStorage_[index] = storage;
        return emitDebugParameter(function, index, storage, error);
    }

void OopLlvmImplementation::emitPollStateDebugVariable(
        const frontend::ProjectOopMirFunction& function)
{
        // NS304-T05-F10: expose the future's state discriminant as an artificial debug variable
        // (offset 0) so a debugger can inspect 0/-1/1..N. Only for the poll (self = parameter 0).
        if (!function.mangledName.ends_with("$poll") ||
            debugInformation_ != DebugInformationKind::Full ||
            currentDebugSubprogram_ == nullptr || parameterStorage_.empty() ||
            function.body == nullptr || function.body->syntax == nullptr ||
            !function.body->syntax->range().isValid()) return;
        LLVMValueRef self = LLVMBuildLoad2(
            builder_, LLVMPointerTypeInContext(context_, 0),
            parameterStorage_[0], "debug.state.self");
        const SourceRange range = function.body->syntax->range();
        LLVMMetadataRef file = debugFile(range.begin());
        LLVMMetadataRef stateType = debugNamedValueType("builtin:int", "state");
        if (file == nullptr || stateType == nullptr) return;
        const source::SourcePosition position = sourceManager_->getPosition(range.begin());
        LLVMMetadataRef variable = LLVMDIBuilderCreateAutoVariable(
            debugBuilder_, currentDebugSubprogram_, "state", 5, file, position.line,
            stateType, 1, LLVMDIFlagArtificial, 0);
        LLVMMetadataRef expression = LLVMDIBuilderCreateExpression(debugBuilder_, nullptr, 0);
        LLVMMetadataRef location = LLVMDIBuilderCreateDebugLocation(
            context_, position.line, position.column, currentDebugSubprogram_, nullptr);
        if (variable != nullptr && expression != nullptr && location != nullptr)
        {
            LLVMDIBuilderInsertDeclareRecordAtEnd(
                debugBuilder_, self, variable, expression, location,
                LLVMGetInsertBlock(builder_));
        }
    }

bool OopLlvmImplementation::prepareFunctionCleanupState(
        const frontend::ProjectOopMirFunction& function,
        std::string& error)
{
        collectLocalAllocas(function.body.get(), error);
        if (!error.empty()) return false;
        for (const frontend::ProjectOopMirCleanupRegion& cleanup : function.cleanupRegions)
        {
            if (cleanup.action == nullptr)
                return fail(error, "OOP MIR cleanup contract contains a null action");
            cleanupRegions_.emplace(cleanup.ordinal, cleanup.action);
            if (memberModel_ == nullptr || !cleanup.action->automaticDestruction ||
                !memberModel_->typeTransitivelyHasCustomDestructor(cleanup.typeKey)) continue;
            LLVMTypeRef flagType = LLVMInt1TypeInContext(context_);
            if (cleanup.localOrdinal.has_value())
            {
                LLVMValueRef flag = entryBlockAlloca(
                    flagType, 0, localValueName(*cleanup.localOrdinal, "drop.flag.l", "").c_str());
                LLVMBuildStore(builder_, LLVMConstInt(flagType, 0, 0), flag);
                localValueDropFlags_[*cleanup.localOrdinal] = flag;
            }
            else if (cleanup.parameterOrdinal.has_value())
            {
                LLVMValueRef flag = entryBlockAlloca(
                    flagType, 0,
                    ("drop.flag.p" + std::to_string(*cleanup.parameterOrdinal)).c_str());
                LLVMBuildStore(builder_, LLVMConstInt(flagType, 1, 0), flag);
                parameterValueDropFlags_[*cleanup.parameterOrdinal] = flag;
            }
        }
        const bool hasExceptionalControl = function.exceptionFlow.requiresPersonality;
        if (hasExceptionalControl || !ownedCaptureCells_.empty() || !coroutineCapturedLocals_.empty())
            LLVMSetPersonalityFn(currentFunction_, sehPersonalityFunction_);
        if (function.constructor || function.returnTypeKey == "builtin:void" ||
            !hasExceptionalControl) return true;
        LLVMTypeRef returnType = function.referenceReturn
            ? LLVMPointerTypeInContext(context_, 0)
            : llvmType(function.returnTypeKey, error);
        if (returnType == nullptr) return false;
        returnStorage_ = entryBlockAlloca(
            returnType, structAlignmentFor(function.returnTypeKey), "eh.return.addr");
        return true;
    }

bool OopLlvmImplementation::finishFunctionDefinition(
        const frontend::ProjectOopMirFunction& function,
        std::string& error)
{
        if (terminated()) return true;
        if (!function.constructor && function.returnTypeKey != "builtin:void")
        {
            LLVMBuildUnreachable(builder_);
            return true;
        }
        releaseOwnedCaptureCells();
        if (!emitRuntimeDebugTrace(
                1U, runtimeDebugCallableRecord(function, "EXIT"), error)) return false;
        LLVMBuildRetVoid(builder_);
        return true;
    }

LLVMValueRef OopLlvmImplementation::collectStaticLocalStorage(
        const frontend::ProjectOopMirNode* node,
        std::string_view storageTypeKey,
        std::string& error)
{
        if (node == nullptr || !node->localOrdinal.has_value())
        {
            fail(error, "T01-F06 static local lost its stable local ordinal");
            return nullptr;
        }
        const std::size_t ordinal = *node->localOrdinal;
        const auto found = staticLocalStorage_.find(ordinal);
        if (found != staticLocalStorage_.end()) return found->second.storage;

        LLVMTypeRef type = llvmType(storageTypeKey, error);
        if (type == nullptr) return nullptr;
        const std::string suffix = std::to_string(ordinal);
        LLVMValueRef storage = LLVMAddGlobal(
            module_, type, ("__nsc_static_local_storage_" + suffix).c_str());
        LLVMSetLinkage(storage, LLVMPrivateLinkage);
        LLVMSetInitializer(storage, LLVMConstNull(type));
        const unsigned alignment = node->referenceLocal ? 8U : structAlignmentFor(node->typeKey);
        if (alignment > 1U) LLVMSetAlignment(storage, alignment);

        LLVMTypeRef i8 = LLVMInt8TypeInContext(context_);
        LLVMValueRef state = LLVMAddGlobal(
            module_, i8, ("__nsc_static_local_state_" + suffix).c_str());
        LLVMSetLinkage(state, LLVMPrivateLinkage);
        LLVMSetInitializer(state, LLVMConstInt(i8, 0, 0));

        LLVMTypeRef i32 = LLVMInt32TypeInContext(context_);
        LLVMValueRef owner = LLVMAddGlobal(
            module_, i32, ("__nsc_static_local_owner_" + suffix).c_str());
        LLVMSetLinkage(owner, LLVMPrivateLinkage);
        LLVMSetInitializer(owner, LLVMConstInt(i32, 0, 0));

        staticLocalStorage_.emplace(
            ordinal, StaticLocalStorage{node, storage, state, owner});
        return storage;
    }

bool OopLlvmImplementation::collectLocalStorage(
        const frontend::ProjectOopMirNode* node,
        bool& stopAfterChildren,
        std::string& error)
{
        stopAfterChildren = false;
        if (localStorage_.contains(*node->localOrdinal)) return true;
        const std::string& typeKey =
            (node->kind == frontend::ProjectOopMirNodeKind::UsingRegion ||
             node->kind == frontend::ProjectOopMirNodeKind::CatchRegion)
            ? node->elementTypeKey
            : node->typeKey;
        const std::string storageTypeKey =
            node->kind == frontend::ProjectOopMirNodeKind::LocalDeclaration &&
                node->referenceLocal
            ? typeKey + "*"
            : typeKey;
        // T07-F04. A local inferred as `never` is a control-flow fact, not an object.
        // Its initializer terminates the block, so reserving even one byte would invent
        // a layout and debugger lifetime for an uninhabited type.
        if (storageTypeKey == "builtin:never")
        {
            stopAfterChildren = true;
            return true;
        }
        if (node->staticLocal)
        {
            LLVMValueRef storage = collectStaticLocalStorage(node, storageTypeKey, error);
            if (storage == nullptr) return false;
            localStorage_[*node->localOrdinal] = storage;
            stopAfterChildren = true;
            return true;
        }
        LLVMTypeRef type = llvmType(storageTypeKey, error);
        if (type == nullptr) return false;
        const bool captured = currentMirFunction_ != nullptr && std::ranges::find(
            currentMirFunction_->capturedLocalOrdinals,
            *node->localOrdinal) != currentMirFunction_->capturedLocalOrdinals.end();
        LLVMValueRef slot = nullptr;
        // EN: Freeze 3.0 C-17: an async-generator poll is a frame-resident coroutine now, so a
        //     captured local keeps its cell in the frame like any other coroutine's (C-05); the
        //     former rejection of this case was lifted.
        // ES: Congelamiento 3.0 C-17: el poll de un generador async ahora es una corrutina con frame
        //     residente, asi que un local capturado guarda su celda en el frame como en cualquier
        //     otra corrutina (C-05); se retiro el rechazo que existia para este caso.
        if (captured && isFrameResidentCoroutine(*currentMirFunction_))
        {
            // EN: Freeze 3.0 C-05: the cell outlives this call; aliasCoroutineFrameLocals finds it
            //     in the frame or creates it on the first entry.
            // ES: Congelamiento 3.0 C-05: la celda sobrevive a esta llamada;
            //     aliasCoroutineFrameLocals la encuentra en el frame o la crea en la primera entrada.
            coroutineCapturedLocals_.emplace(*node->localOrdinal, typeKey);
            localStorage_[*node->localOrdinal] = nullptr;
            return true;
        }
        if (captured)
        {
            LLVMValueRef cell = allocateCaptureCell(
                typeKey, slot, localValueName(*node->localOrdinal, "capture.local", ""), error);
            if (cell == nullptr || slot == nullptr) return false;
            localCaptureCells_[*node->localOrdinal] = cell;
            ownedCaptureCells_.push_back(cell);
        }
        else
        {
            // ES: La ranura se crea en el bloque de entrada aunque el builder ya esté en otro
            // bloque. Una celda de captura anterior (o la de un parámetro) emite una reserva en el
            // montón con su comprobación de null, que cierra el bloque de entrada; los locales que
            // venían después quedaban como `alloca` dinámicos. LLVM los describía en el PDB con
            // ubicaciones indirectas, a otros los perdía (en DeepFunctional faltaban `shifted` y
            // `same`), y un `alloca` fuera del bloque de entrada vuelve a reservar pila cada vez
            // que se ejecuta.
            // EN: The slot is created in the entry block even when the builder already sits in
            // another block. An earlier capture cell (or a parameter's) emits a heap allocation with
            // its null check, which closes the entry block; the locals that came after became
            // dynamic `alloca`s. LLVM described them in the PDB with indirect locations, lost others
            // (DeepFunctional was missing `shifted` and `same`), and an `alloca` outside the entry
            // block reserves stack again every time it runs.
            slot = entryBlockAlloca(
                type, 0, localValueName(*node->localOrdinal, "local", ".addr").c_str());
        }
        // An indirect-struct carrier is [N x i8], whose alloca defaults to align 1 --
        // but member accesses through the slot do naturally-aligned typed loads and
        // stores, so the slot must carry the struct layout's real alignment.
        const unsigned structAlignment = node->referenceLocal ? 8U : structAlignmentFor(typeKey);
        if (!captured && structAlignment > 1U) LLVMSetAlignment(slot, structAlignment);
        localStorage_[*node->localOrdinal] = slot;
        return true;
    }

void OopLlvmImplementation::collectStackPromotedObjectStorage(
        const frontend::ProjectOopMirNode* node,
        std::string& error)
{
        if (node->kind != frontend::ProjectOopMirNodeKind::LocalDeclaration ||
            !node->stackPromotedObject || stackObjectStorage_.contains(*node->localOrdinal)) return;
        if (node->children.size() != 1 || node->children.front() == nullptr ||
            node->children.front()->kind != frontend::ProjectOopMirNodeKind::AllocateAndConstruct ||
            !node->children.front()->stackPromotedObject ||
            node->children.front()->localOrdinal != node->localOrdinal ||
            node->children.front()->immediate == 0 || node->children.front()->alignment == 0)
        {
            error = "item 323 stack-promoted local has no matching object layout";
            return;
        }
        const frontend::ProjectOopMirNode* construction = node->children.front().get();
        LLVMTypeRef bytes = LLVMArrayType2(LLVMInt8TypeInContext(context_), construction->immediate);
        LLVMValueRef object = entryBlockAlloca(
            bytes, static_cast<unsigned>(construction->alignment),
            localValueName(*node->localOrdinal, "local", ".object").c_str());
        stackObjectStorage_[*node->localOrdinal] = object;
    }

void OopLlvmImplementation::collectLocalAllocas(const frontend::ProjectOopMirNode* node, std::string& error)
{
        if (node == nullptr || !error.empty()) return;
        if ((node->kind == frontend::ProjectOopMirNodeKind::LocalDeclaration ||
             node->kind == frontend::ProjectOopMirNodeKind::OwnedTemporaryBorrow ||
             node->kind == frontend::ProjectOopMirNodeKind::UsingRegion ||
             node->kind == frontend::ProjectOopMirNodeKind::CatchRegion) &&
            node->localOrdinal.has_value())
        {
            bool stopAfterChildren = false;
            if (!collectLocalStorage(node, stopAfterChildren, error)) return;
            if (stopAfterChildren)
            {
                for (const auto& child : node->children) collectLocalAllocas(child.get(), error);
                return;
            }
            collectStackPromotedObjectStorage(node, error);
            if (!error.empty()) return;
        }
        for (const auto& child : node->children) collectLocalAllocas(child.get(), error);
    }

}
