#include "../Internal/OopLlvmImplementation.h"
#include "OopLlvmSupport.h"

#include <algorithm>
#include <cstdint>
#include <unordered_map>
#include <vector>

namespace nativesharp::llvm_backend
{
using namespace llvm_api;

// EN: Freeze 3.0 C-02. A poll (or a MoveNext) used to re-run its body from the top on every call
//     and skip each suspension point whose state did not match: code before the point ran again,
//     every await shared state 1 and frame slot 0, and a local's address died with the poll's
//     stack. Now the frame owns the locals, the entry switches on the state, and the switch jumps
//     to the start of the statement that suspended.
// ES: Congelamiento 3.0 C-02. Un poll (o un MoveNext) volvia a correr su cuerpo desde arriba en
//     cada llamada y saltaba cada punto de suspension cuyo estado no coincidia: el codigo previo
//     se repetia, todos los await compartian el estado 1 y el slot 0 del frame, y la direccion de
//     un local moria con la pila del poll. Ahora el frame posee los locales, la entrada hace switch
//     sobre el estado y salta al inicio de la sentencia que se suspendio.
namespace
{
std::uint64_t alignUp8(std::uint64_t value) noexcept
{
    return (value + 7U) / 8U * 8U;
}

// EN: MIR closes a coroutine frame with one drop-flag byte per frame local (see
//     appendCoroutineDropFlags); this is where that area begins.
// ES: MIR cierra el frame de una corrutina con un byte de bandera por local del frame (ver
//     appendCoroutineDropFlags); aqui empieza esa area.
std::uint64_t dropFlagBase(const frontend::ProjectOopMirFunction& function) noexcept
{
    const std::uint64_t flags = alignUp8(function.asyncFrameLocals.size());
    return function.asyncFrameSize >= flags ? function.asyncFrameSize - flags : 0;
}

bool isSuspensionPoint(const frontend::ProjectOopMirNode& node) noexcept
{
    return node.kind == frontend::ProjectOopMirNodeKind::SuspendPoint ||
        node.kind == frontend::ProjectOopMirNodeKind::YieldValue ||
        (node.kind == frontend::ProjectOopMirNodeKind::AwaitFuture && node.integerValue == 1);
}
}

bool OopLlvmImplementation::isFrameResidentCoroutine(
        const frontend::ProjectOopMirFunction& function) const
{
        // EN: The async-generator poll ($moveNext$poll) keeps its own save/restore protocol.
        // ES: El poll del generador async ($moveNext$poll) conserva su protocolo de guardado.
        const std::string& name = function.mangledName;
        const bool poll = name.ends_with("$poll") && !name.ends_with("$moveNext$poll");
        const bool moveNext = name.ends_with("$moveNext") && !function.parameterTypeKeys.empty() &&
            !function.parameterTypeKeys[0].starts_with("async-iterator:");
        return (poll || moveNext) && function.body != nullptr &&
            !function.parameterTypeKeys.empty() && parameterStorage_.contains(0);
}

void OopLlvmImplementation::collectCoroutineSuspensions(const frontend::ProjectOopMirNode* root)
{
        // EN: Ids follow source order (pre-order, children left to right); each id is recorded on
        //     every ancestor so the innermost statement lowered around it can claim it.
        // ES: Los ids siguen el orden del fuente (pre-orden, hijos de izquierda a derecha); cada id
        //     se anota en cada ancestro para que la sentencia mas interna lo reclame.
        std::unordered_map<const frontend::ProjectOopMirNode*, const frontend::ProjectOopMirNode*> parents;
        std::vector<const frontend::ProjectOopMirNode*> work{root};
        std::int64_t nextId = 1;
        while (!work.empty())
        {
            const frontend::ProjectOopMirNode* node = work.back();
            work.pop_back();
            if (node == nullptr) continue;
            if (isSuspensionPoint(*node))
            {
                const std::int64_t id = nextId++;
                coroutine_.suspensionIds.emplace(node, id);
                for (const frontend::ProjectOopMirNode* ancestor = node; ancestor != nullptr;)
                {
                    coroutine_.subtreeIds[ancestor].push_back(id);
                    const auto parent = parents.find(ancestor);
                    ancestor = parent == parents.end() ? nullptr : parent->second;
                }
            }
            for (auto child = node->children.rbegin(); child != node->children.rend(); ++child)
            {
                if (*child == nullptr) continue;
                parents.emplace(child->get(), node);
                work.push_back(child->get());
            }
        }
}

LLVMValueRef OopLlvmImplementation::ensureCoroutineFrame(
        const frontend::ProjectOopMirFunction& function, std::string& error)
{
        // EN: The constructor allocates the frame; a missing one gets a heap frame here, never a
        //     stack one, because the locals must outlive this call.
        // ES: El constructor reserva el frame; si falta, se reserva uno en el heap aqui, nunca en
        //     la pila, porque los locales deben sobrevivir a esta llamada.
        LLVMValueRef frameAddress = byteOffset(coroutine_.self, 8, "coro.frame.addr");
        LLVMValueRef frame = LLVMBuildLoad2(
            builder_, LLVMPointerTypeInContext(context_, 0), frameAddress, "coro.frame");
        if (function.asyncFrameSize == 0) return frame;
        if (allocationFunction_ == nullptr || allocationFunctionType_ == nullptr)
            return failValue(error, "coroutine frame allocation is unavailable");
        LLVMBasicBlockRef present = LLVMGetInsertBlock(builder_);
        LLVMBasicBlockRef allocate = appendBlock("coro.frame.alloc");
        LLVMBasicBlockRef ready = appendBlock("coro.frame.ready");
        LLVMBuildCondBr(
            builder_,
            LLVMBuildICmp(
                builder_, LLVMIntEQ, frame, LLVMConstNull(LLVMPointerTypeInContext(context_, 0)),
                "coro.frame.missing"),
            allocate, ready);
        enterBlock(allocate);
        LLVMValueRef size = LLVMConstInt(LLVMInt64TypeInContext(context_), function.asyncFrameSize, 0);
        LLVMValueRef allocArgs[] = {size};
        LLVMValueRef created = buildNoUnwindCall(
            allocationFunctionType_, allocationFunction_, allocArgs, 1, "coro.frame.new");
        if (created == nullptr) return failValue(error, "coroutine frame allocation failed");
        LLVMBuildStore(builder_, created, frameAddress);
        zeroCoroutineDropFlags(created, function);
        LLVMBasicBlockRef allocated = LLVMGetInsertBlock(builder_);
        LLVMBuildBr(builder_, ready);
        enterBlock(ready);
        LLVMValueRef phi = LLVMBuildPhi(builder_, LLVMPointerTypeInContext(context_, 0), "coro.frame.phi");
        LLVMValueRef incoming[] = {frame, created};
        LLVMBasicBlockRef incomingBlocks[] = {present, allocated};
        LLVMAddIncoming(phi, incoming, incomingBlocks, 2);
        return phi;
}

void OopLlvmImplementation::zeroCoroutineDropFlags(
        LLVMValueRef frame, const frontend::ProjectOopMirFunction& function)
{
        // EN: Freeze 3.0 C-05: the whole frame starts zeroed, so the drop guards start cleared and
        //     the capture-cell slots start empty.
        // ES: Congelamiento 3.0 C-05: todo el frame inicia en cero, asi las guardas de destruccion
        //     inician limpias y los slots de celdas de captura inician vacios.
        if (function.asyncFrameSize == 0 || memsetFunction_ == nullptr) return;
        LLVMValueRef arguments[] = {
            frame,
            LLVMConstInt(LLVMInt8TypeInContext(context_), 0, 0),
            LLVMConstInt(LLVMInt64TypeInContext(context_), function.asyncFrameSize, 0),
            LLVMConstInt(LLVMInt1TypeInContext(context_), 0, 0)};
        buildNoUnwindCall(memsetFunctionType_, memsetFunction_, arguments, 4, "");
}

bool OopLlvmImplementation::aliasCoroutineFrameLocals(
        const frontend::ProjectOopMirFunction& function, std::string& error)
{
        // EN: A local's storage IS its frame slot (and its drop guard a frame byte); the entry-block
        //     allocas collectLocalAllocas made stay unused. Captured locals keep their cells.
        // ES: El almacenamiento de un local ES su slot del frame (y su guarda un byte del frame);
        //     los allocas de entrada quedan sin uso. Los locales capturados conservan su celda.
        const std::uint64_t flagBase = dropFlagBase(function);
        for (std::size_t index = 0; index < function.asyncFrameLocals.size(); ++index)
        {
            const auto& [localOrdinal, offset, typeKey] = function.asyncFrameLocals[index];
            (void)typeKey;
            const auto captured = coroutineCapturedLocals_.find(localOrdinal);
            if (captured != coroutineCapturedLocals_.end())
            {
                LLVMValueRef cell = ensureCoroutineCaptureCell(
                    byteOffset(coroutine_.frame, offset, "coro.cell.slot"), captured->second,
                    localOrdinal, error);
                if (cell == nullptr) return false;
                continue;
            }
            if (localCaptureCells_.contains(localOrdinal)) continue;
            const auto storage = localStorage_.find(localOrdinal);
            if (storage == localStorage_.end() || storage->second == nullptr) continue;
            storage->second = byteOffset(coroutine_.frame, offset, "coro.local.slot");
            const auto flag = localValueDropFlags_.find(localOrdinal);
            if (flag != localValueDropFlags_.end())
                flag->second = byteOffset(coroutine_.frame, flagBase + index, "coro.drop.flag");
        }
        for (const auto& [localOrdinal, typeKey] : coroutineCapturedLocals_)
            if (!localCaptureCells_.contains(localOrdinal))
                return fail(error, "captured coroutine local has no frame slot for its cell");
        return true;
}

LLVMValueRef OopLlvmImplementation::ensureCoroutineCaptureCell(
        LLVMValueRef cellSlot, std::string_view typeKey, std::size_t localOrdinal, std::string& error)
{
        // EN: Freeze 3.0 C-05 (found while porting the file-mode cases): a captured local of a
        //     coroutine got a new cell on every poll, so a lambda made before an `await` and the
        //     code after it saw different variables. The cell is made once and kept in the frame;
        //     the return that completes the coroutine releases it, a suspension does not.
        // ES: Congelamiento 3.0 C-05 (hallazgo al portar los casos de modo archivo): un local
        //     capturado de una corrutina recibia una celda nueva en cada poll, asi que una lambda
        //     creada antes de un `await` y el codigo posterior veian variables distintas. La celda
        //     se crea una vez y se guarda en el frame; el return que completa la corrutina la
        //     libera, una suspension no.
        LLVMTypeRef pointerType = LLVMPointerTypeInContext(context_, 0);
        LLVMValueRef stored = LLVMBuildLoad2(builder_, pointerType, cellSlot, "coro.cell");
        LLVMBasicBlockRef present = LLVMGetInsertBlock(builder_);
        LLVMBasicBlockRef create = appendBlock("coro.cell.create");
        LLVMBasicBlockRef ready = appendBlock("coro.cell.ready");
        LLVMBuildCondBr(
            builder_,
            LLVMBuildICmp(builder_, LLVMIntEQ, stored, LLVMConstNull(pointerType), "coro.cell.missing"),
            create, ready);
        enterBlock(create);
        LLVMValueRef unusedValue = nullptr;
        const std::string stem = localValueName(localOrdinal, "capture.local", "");
        LLVMValueRef created = allocateCaptureCell(typeKey, unusedValue, stem, error);
        if (created == nullptr) return nullptr;
        LLVMBuildStore(builder_, created, cellSlot);
        LLVMBasicBlockRef createdBlock = LLVMGetInsertBlock(builder_);
        LLVMBuildBr(builder_, ready);
        enterBlock(ready);
        LLVMValueRef cell = LLVMBuildPhi(builder_, pointerType, "coro.cell.phi");
        LLVMValueRef incoming[] = {stored, created};
        LLVMBasicBlockRef incomingBlocks[] = {present, createdBlock};
        LLVMAddIncoming(cell, incoming, incomingBlocks, 2);
        LLVMValueRef value = captureCellValueAddress(cell, typeKey, stem, error);
        if (value == nullptr) return nullptr;
        localCaptureCells_[localOrdinal] = cell;
        localStorage_[localOrdinal] = value;
        ownedCaptureCells_.push_back(cell);
        return cell;
}

bool OopLlvmImplementation::emitCoroutineDoneReturn(
        const frontend::ProjectOopMirFunction& function, std::string& error)
{
        // EN: A finished poll returns at once; a finished MoveNext answers false.
        // ES: Un poll terminado regresa de inmediato; un MoveNext terminado responde false.
        if (function.returnTypeKey == "builtin:void" || sretStorage_ != nullptr)
        {
            LLVMBuildRetVoid(builder_);
            return true;
        }
        LLVMTypeRef returnType = llvmType(function.returnTypeKey, error);
        if (returnType == nullptr) return false;
        LLVMBuildRet(builder_, LLVMConstNull(returnType));
        return true;
}

bool OopLlvmImplementation::beginCoroutineDispatch(
        const frontend::ProjectOopMirFunction& function, std::string& error)
{
        coroutine_ = OopLlvmCoroutineState{};
        if (!isFrameResidentCoroutine(function)) return true;
        LLVMTypeRef intType = llvmType("builtin:int", error);
        if (intType == nullptr) return false;
        coroutine_.active = true;
        coroutine_.self = LLVMBuildLoad2(
            builder_, LLVMPointerTypeInContext(context_, 0), parameterStorage_[0], "coro.self");
        coroutine_.frame = ensureCoroutineFrame(function, error);
        if (coroutine_.frame == nullptr) return false;
        if (!aliasCoroutineFrameLocals(function, error)) return false;
        collectCoroutineSuspensions(function.body.get());
        coroutine_.resumeState = entryBlockAlloca(intType, 0, "coro.resume.state");
        LLVMValueRef state = LLVMBuildLoad2(builder_, intType, coroutine_.self, "coro.state");
        LLVMBuildStore(builder_, state, coroutine_.resumeState);
        LLVMBasicBlockRef start = appendBlock("coro.start");
        LLVMBasicBlockRef done = appendBlock("coro.done");
        coroutine_.dispatch = LLVMBuildSwitch(
            builder_, state, done, static_cast<unsigned>(coroutine_.suspensionIds.size() + 1U));
        LLVMAddCase(coroutine_.dispatch, LLVMConstInt(intType, 0, 0), start);
        enterBlock(done);
        if (!emitCoroutineDoneReturn(function, error)) return false;
        enterBlock(start);
        return true;
}

bool OopLlvmImplementation::finishCoroutineDispatch(std::string& error)
{
        if (!coroutine_.active) return true;
        LLVMTypeRef intType = llvmType("builtin:int", error);
        if (intType == nullptr) return false;
        std::vector<std::int64_t> ids;
        ids.reserve(coroutine_.resumeBlocks.size());
        for (const auto& [id, block] : coroutine_.resumeBlocks) ids.push_back(id);
        std::ranges::sort(ids);
        for (const std::int64_t id : ids)
            LLVMAddCase(
                coroutine_.dispatch,
                LLVMConstInt(intType, static_cast<unsigned long long>(id), 1),
                coroutine_.resumeBlocks[id]);
        std::vector<std::int64_t> cancelIds;
        for (const auto& [id, block] : coroutine_.cancelBlocks) cancelIds.push_back(id);
        std::ranges::sort(cancelIds);
        for (const std::int64_t id : cancelIds)
            LLVMAddCase(
                coroutine_.dispatch,
                LLVMConstInt(
                    intType,
                    static_cast<unsigned long long>(OopLlvmCoroutineState::kCancelStateBase - id), 1),
                coroutine_.cancelBlocks[id]);
        coroutine_ = OopLlvmCoroutineState{};
        return true;
}

void OopLlvmImplementation::enterCoroutineResumeTarget(const frontend::ProjectOopMirNode* statement)
{
        // EN: Outer statements claim first and inner ones override, so each id ends at the start
        //     of the innermost statement around it. Re-running that statement re-evaluates what
        //     precedes the point inside it; completed awaits there read their stored result.
        // ES: Las sentencias externas reclaman primero y las internas sobrescriben, asi cada id
        //     termina en el inicio de la sentencia mas interna. Re-ejecutarla re-evalua lo previo
        //     al punto dentro de ella; los await ya completados leen su resultado guardado.
        if (!coroutine_.active || statement == nullptr || terminated()) return;
        const auto found = coroutine_.subtreeIds.find(statement);
        if (found == coroutine_.subtreeIds.end()) return;
        LLVMBasicBlockRef block = appendBlock("coro.resume.stmt");
        LLVMBuildBr(builder_, block);
        enterBlock(block);
        for (const std::int64_t id : found->second) coroutine_.resumeBlocks[id] = block;
}

LLVMValueRef OopLlvmImplementation::coroutineResumeMatches(
        const frontend::ProjectOopMirNode* node, std::int64_t& id, std::string& error)
{
        const auto found = coroutine_.suspensionIds.find(node);
        if (found == coroutine_.suspensionIds.end())
            return failValue(error, "coroutine suspension point has no state id");
        id = found->second;
        if (!coroutine_.resumeBlocks.contains(id))
            return failValue(error, "coroutine suspension point is outside a resumable statement");
        LLVMTypeRef intType = llvmType("builtin:int", error);
        if (intType == nullptr) return nullptr;
        LLVMValueRef seen = LLVMBuildLoad2(builder_, intType, coroutine_.resumeState, "coro.seen");
        return LLVMBuildICmp(
            builder_, LLVMIntEQ, seen, LLVMConstInt(intType, static_cast<unsigned long long>(id), 1),
            "coro.is.resume");
}

bool OopLlvmImplementation::lowerCoroutineSuspendPoint(
        const frontend::ProjectOopMirNode* node, std::string& error)
{
        // EN: `yield;` leaves with state = id on the way in, and continues on the resume.
        // ES: `yield;` sale con estado = id al llegar, y continua al reanudar.
        std::int64_t id = 0;
        LLVMValueRef resuming = coroutineResumeMatches(node, id, error);
        if (resuming == nullptr) return false;
        LLVMTypeRef intType = llvmType("builtin:int", error);
        if (intType == nullptr) return false;
        LLVMBasicBlockRef leave = appendBlock("coro.suspend.leave");
        LLVMBasicBlockRef resumed = appendBlock("coro.suspend.resumed");
        LLVMBuildCondBr(builder_, resuming, resumed, leave);
        enterBlock(leave);
        LLVMBuildStore(
            builder_, LLVMConstInt(intType, static_cast<unsigned long long>(id), 1), coroutine_.self);
        LLVMBuildRetVoid(builder_);
        enterBlock(resumed);
        LLVMBuildStore(builder_, LLVMConstInt(intType, 0, 0), coroutine_.resumeState);
        return true;
}

bool OopLlvmImplementation::lowerCoroutineYieldValue(
        const frontend::ProjectOopMirNode* node, std::string& error)
{
        // EN: `yield return v` stores the item, sets state = id and answers true; the next
        //     MoveNext resumes right after it.
        // ES: `yield return v` guarda el elemento, pone estado = id y responde true; el siguiente
        //     MoveNext reanuda justo despues.
        std::int64_t id = 0;
        LLVMValueRef resuming = coroutineResumeMatches(node, id, error);
        if (resuming == nullptr) return false;
        LLVMTypeRef intType = llvmType("builtin:int", error);
        if (intType == nullptr) return false;
        LLVMBasicBlockRef produce = appendBlock("coro.yield.produce");
        LLVMBasicBlockRef resumed = appendBlock("coro.yield.resumed");
        LLVMBasicBlockRef cancel = appendBlock("coro.yield.cancel");
        LLVMBuildCondBr(builder_, resuming, resumed, produce);
        enterBlock(produce);
        if (!storeYieldValueItem(node, coroutine_.self, error)) return false;
        LLVMBuildStore(
            builder_, LLVMConstInt(intType, static_cast<unsigned long long>(id), 1), coroutine_.self);
        LLVMBuildRet(builder_, LLVMConstInt(LLVMInt1TypeInContext(context_), 1, 0));
        coroutine_.cancelBlocks[id] = cancel;
        if (!emitCoroutineYieldCancel(node, cancel, error)) return false;
        enterBlock(resumed);
        LLVMBuildStore(builder_, LLVMConstInt(intType, 0, 0), coroutine_.resumeState);
        return true;
}

bool OopLlvmImplementation::emitCoroutineYieldCancel(
        const frontend::ProjectOopMirNode* node, LLVMBasicBlockRef cancel, std::string& error)
{
        // EN: An abandoned generator (foreach `break`, `it.Drop()`) unwinds from the yield it is
        //     parked at: the finally blocks and the scoped destructions active there run once,
        //     exactly like `yield break`. The lowering state is restored for the resume path.
        // ES: Un generador abandonado (`break` del foreach, `it.Drop()`) se desenreda desde el
        //     yield donde esta detenido: los finally y las destrucciones activas ahi corren una vez,
        //     igual que `yield break`. El estado del lowering se restaura para la ruta de reanudar.
        enterBlock(cancel);
        const std::vector<std::size_t> savedActive = activeCleanupRegions_;
        const std::vector<FuncletContext> savedFunclets = funcletStack_;
        bool ok = true;
        for (std::size_t index = node->cleanupRegionOrdinals.size(); index > 0 && ok; --index)
        {
            ok = emitOneStructuredCleanup(node->cleanupRegionOrdinals[index - 1], error);
            if (ok && terminated()) break;
        }
        if (ok && !terminated())
        {
            LLVMTypeRef intType = llvmType("builtin:int", error);
            ok = intType != nullptr;
            if (ok)
            {
                LLVMBuildStore(
                    builder_, LLVMConstInt(intType, static_cast<unsigned long long>(-1), 1), coroutine_.self);
                releaseOwnedCaptureCells();
                LLVMBuildRet(builder_, LLVMConstInt(LLVMInt1TypeInContext(context_), 0, 0));
            }
        }
        activeCleanupRegions_ = savedActive;
        funcletStack_ = savedFunclets;
        return ok;
}

bool OopLlvmImplementation::markCoroutineFinished(std::string& error)
{
        // EN: Only a MoveNext needs it: a poll's return already publishes its result and state.
        // ES: Solo un MoveNext lo necesita: el return de un poll ya publica resultado y estado.
        if (!coroutine_.active || currentMirFunction_ == nullptr ||
            currentMirFunction_->returnTypeKey != "builtin:bool") return true;
        LLVMTypeRef intType = llvmType("builtin:int", error);
        if (intType == nullptr) return false;
        LLVMValueRef errored = LLVMConstInt(intType, static_cast<unsigned long long>(-2), 1);
        LLVMValueRef state = LLVMBuildLoad2(builder_, intType, coroutine_.self, "coro.exit.state");
        LLVMValueRef finished = LLVMBuildSelect(
            builder_, LLVMBuildICmp(builder_, LLVMIntEQ, state, errored, "coro.exit.errored"), errored,
            LLVMConstInt(intType, static_cast<unsigned long long>(-1), 1), "coro.exit.final");
        LLVMBuildStore(builder_, finished, coroutine_.self);
        return true;
}

bool OopLlvmImplementation::lowerSyncGeneratorDrop(LLVMValueRef self, std::string& error)
{
        // EN: A generator parked at yield `k` is unwound by its own MoveNext (state
        //     kCancelStateBase - k). One never started, or already finished, holds no live local.
        // ES: Un generador detenido en el yield `k` lo desenreda su propio MoveNext (estado
        //     kCancelStateBase - k). Uno que nunca empezo, o que ya termino, no tiene locales vivos.
        LLVMTypeRef intType = llvmType("builtin:int", error);
        if (intType == nullptr) return false;
        LLVMValueRef state = LLVMBuildLoad2(builder_, intType, self, "drop.gen.state");
        LLVMBasicBlockRef unwind = appendBlock("drop.gen.unwind");
        LLVMBasicBlockRef done = appendBlock("drop.gen.done");
        LLVMBuildCondBr(
            builder_,
            LLVMBuildICmp(builder_, LLVMIntSGT, state, LLVMConstInt(intType, 0, 0), "drop.gen.parked"),
            unwind, done);
        enterBlock(unwind);
        LLVMValueRef cancelState = LLVMBuildSub(
            builder_,
            LLVMConstInt(intType, static_cast<unsigned long long>(OopLlvmCoroutineState::kCancelStateBase), 1),
            state, "drop.gen.cancel.state");
        LLVMBuildStore(builder_, cancelState, self);
        LLVMValueRef moveNextFn = LLVMBuildLoad2(
            builder_, LLVMPointerTypeInContext(context_, 0),
            byteOffset(self, 24, "drop.gen.movenextfn.addr"), "drop.gen.movenextfn");
        LLVMTypeRef fnParams[] = {LLVMPointerTypeInContext(context_, 0)};
        LLVMTypeRef fnType = LLVMFunctionType(LLVMInt1TypeInContext(context_), fnParams, 1, 0);
        LLVMValueRef args[] = {self};
        if (LLVMBuildCall2(builder_, fnType, moveNextFn, args, 1U, "") == nullptr)
            return fail(error, "generator drop could not unwind through MoveNext");
        LLVMBuildBr(builder_, done);
        enterBlock(done);
        return true;
}

// EN: Freeze 3.0 C-14: F$drop releases a future that never ran: the owned parameters its constructor
//     stored in the frame are destroyed, then the frame is freed and the future parked as done. A
//     future cannot be left suspended from source (only await polls it, to completion), so a
//     started one holds no frame of its own to release here.
// ES: Congelamiento 3.0 C-14: F$drop libera un future que nunca corrio: se destruyen los
//     parametros owned que su constructor guardo en el frame, luego se libera el frame y el future
//     queda terminado. Desde el codigo fuente un future no puede quedar suspendido (solo await lo
//     consulta, hasta completar), asi que uno ya iniciado no tiene aqui frame propio que liberar.
bool OopLlvmImplementation::lowerFutureDrop(LLVMValueRef self, std::string& error)
{
        if (currentMirFunction_ == nullptr || !currentMirFunction_->mangledName.ends_with("$drop"))
            return fail(error, "future drop has no owning function");
        const std::string pollName =
            currentMirFunction_->mangledName.substr(0, currentMirFunction_->mangledName.size() - 5) +
            "$poll";
        const frontend::ProjectOopMirFunction* poll = nullptr;
        for (const frontend::ProjectOopMirFunction& candidate : mirModel_.functions())
            if (candidate.mangledName == pollName) poll = &candidate;
        if (poll == nullptr) return fail(error, "future drop cannot find '" + pollName + "'");
        LLVMTypeRef ptr = LLVMPointerTypeInContext(context_, 0);
        LLVMTypeRef intType = llvmType("builtin:int", error);
        if (intType == nullptr) return false;
        LLVMValueRef state = LLVMBuildLoad2(builder_, intType, self, "drop.future.state");
        LLVMValueRef frame = LLVMBuildLoad2(
            builder_, ptr, byteOffset(self, 8, "drop.future.frame.addr"), "drop.future.frame");
        LLVMBasicBlockRef release = appendBlock("drop.future.release");
        LLVMBasicBlockRef done = appendBlock("drop.future.done");
        LLVMValueRef unstarted = LLVMBuildAnd(
            builder_,
            LLVMBuildICmp(builder_, LLVMIntEQ, state, LLVMConstInt(intType, 0, 0), "drop.future.fresh"),
            LLVMBuildICmp(builder_, LLVMIntNE, frame, LLVMConstNull(ptr), "drop.future.framed"),
            "drop.future.unstarted");
        LLVMBuildCondBr(builder_, unstarted, release, done);
        enterBlock(release);
        for (const frontend::ProjectOopMirCleanupRegion& region : poll->cleanupRegions)
        {
            if (region.kind != frontend::ProjectOopMirCleanupKind::OwnedParameter ||
                !region.parameterOrdinal.has_value()) continue;
            const auto* captured = mirModel_.asyncFrameParameter(
                *poll, *region.parameterOrdinal, region.typeKey);
            if (captured == nullptr) continue;
            const auto& [ordinal, offset, typeKey] = *captured;
            if (!destroyCapturedOwnedParameter(
                    byteOffset(frame, offset, "drop.future.param"), typeKey, error)) return false;
        }
        LLVMValueRef freeArgs[] = {frame};
        buildNoUnwindCall(deallocationFunctionType_, deallocationFunction_, freeArgs, 1, "");
        LLVMBuildStore(builder_, LLVMConstNull(ptr), byteOffset(self, 8, "drop.future.frame.null"));
        LLVMBuildStore(builder_, LLVMConstInt(intType, static_cast<unsigned long long>(-1), 1), self);
        LLVMBuildBr(builder_, done);
        enterBlock(done);
        return true;
}

LLVMValueRef OopLlvmImplementation::pollCoroutineAwaitChild(
        const frontend::ProjectOopMirNode* node,
        LLVMValueRef childSlot,
        LLVMTypeRef intType,
        std::string& error)
{
        // EN: A child that is already ready (a completed await re-read while its statement is
        //     replayed) is not polled again.
        // ES: Un hijo ya listo (un await completado que se relee al repetir su sentencia) no se
        //     vuelve a consultar.
        LLVMBasicBlockRef poll = appendBlock("coro.await.poll");
        LLVMBasicBlockRef checked = appendBlock("coro.await.checked");
        LLVMValueRef state = LLVMBuildLoad2(builder_, intType, childSlot, "coro.await.pre.state");
        LLVMBuildCondBr(
            builder_,
            LLVMBuildICmp(
                builder_, LLVMIntEQ, state, LLVMConstInt(intType, static_cast<unsigned long long>(-1), 1),
                "coro.await.pre.ready"),
            checked, poll);
        enterBlock(poll);
        LLVMValueRef pollFunction = nullptr;
        LLVMTypeRef pollFunctionType = nullptr;
        if (!resolveAwaitPollTarget(
                node, childSlot, "coro.await.pollfn", "coro.await.pollfn",
                pollFunction, pollFunctionType, error)) return nullptr;
        LLVMValueRef pollArgs[] = {childSlot};
        if (LLVMBuildCall2(builder_, pollFunctionType, pollFunction, pollArgs, 1U, "") == nullptr)
            return failValue(error, "async poll call failed");
        LLVMBuildBr(builder_, checked);
        enterBlock(checked);
        return childSlot;
}

LLVMValueRef OopLlvmImplementation::lowerCoroutineAwait(
        const frontend::ProjectOopMirNode* node, std::string& error, LLVMValueRef childOverride)
{
        // EN: The child future lives in this await's own frame slot. A fresh flow (resume state
        //     0) creates it; the resume of this id, or the replay of its statement toward a later
        //     point, reuses it.
        // ES: El future hijo vive en el slot propio de este await en el frame. Un flujo nuevo
        //     (estado de reanudacion 0) lo crea; la reanudacion de este id, o la repeticion de su
        //     sentencia hacia un punto posterior, lo reutiliza.
        if (node->children.size() != 1U) return failValue(error, "await future requires one operand");
        std::int64_t id = 0;
        if (coroutineResumeMatches(node, id, error) == nullptr) return nullptr;
        LLVMTypeRef intType = llvmType("builtin:int", error);
        LLVMTypeRef carrier = llvmType(node->elementTypeKey, error);
        if (intType == nullptr || carrier == nullptr) return nullptr;
        if (currentMirFunction_ == nullptr || node->secondaryImmediate >= currentMirFunction_->asyncFrameSize)
            return failValue(error, "await future has no slot in the coroutine frame");
        LLVMValueRef childSlot = byteOffset(coroutine_.frame, node->secondaryImmediate, "coro.await.child");
        bool childFrameOnHeap = false;
        std::uint64_t childFrameSize = 0;
        awaitChildFrameInfo(node, childFrameOnHeap, childFrameSize);
        LLVMValueRef seen = LLVMBuildLoad2(builder_, intType, coroutine_.resumeState, "coro.await.seen");
        LLVMBasicBlockRef create = appendBlock("coro.await.create");
        LLVMBasicBlockRef reuse = appendBlock("coro.await.reuse");
        LLVMBasicBlockRef selected = appendBlock("coro.await.selected");
        LLVMBuildCondBr(
            builder_,
            LLVMBuildICmp(builder_, LLVMIntEQ, seen, LLVMConstInt(intType, 0, 0), "coro.await.fresh"),
            create, reuse);
        enterBlock(create);
        LLVMValueRef fresh = childOverride != nullptr
            ? childOverride
            : lowerExpression(node->children.front().get(), error);
        if (fresh == nullptr) return nullptr;
        LLVMBuildStore(builder_, fresh, childSlot);
        consumeAwaitedFuture(node->children.front().get(), error);
        if (!error.empty()) return nullptr;
        // EN: A heap frame when the constructor left none: the child may outlive this poll.
        // ES: Un frame en el heap si el constructor no dejo ninguno: el hijo puede sobrevivir.
        if (!attachAwaitFrameIfMissing(childSlot, childFrameSize, true, error)) return nullptr;
        LLVMBuildBr(builder_, selected);
        enterBlock(reuse);
        LLVMValueRef mine = LLVMBuildICmp(
            builder_, LLVMIntEQ, seen, LLVMConstInt(intType, static_cast<unsigned long long>(id), 1),
            "coro.await.mine");
        LLVMBuildStore(
            builder_, LLVMBuildSelect(builder_, mine, LLVMConstInt(intType, 0, 0), seen, "coro.await.next"),
            coroutine_.resumeState);
        LLVMBuildBr(builder_, selected);
        enterBlock(selected);
        if (pollCoroutineAwaitChild(node, childSlot, intType, error) == nullptr) return nullptr;
        // EN: Freeze 3.0 C-14: every frame a coroutine child holds is heap (its constructor's, the
        //     one attached above, or its poll's), so it is released when the child completes.
        // ES: Congelamiento 3.0 C-14: todo frame que tiene un hijo de corrutina es heap (el de su
        //     constructor, el adjuntado arriba, o el de su poll), asi que se libera al completar.
        (void)childFrameOnHeap;
        return finishSuspendingAwait(
            node, coroutine_.self, childSlot, intType, carrier, true, error);
}
}
