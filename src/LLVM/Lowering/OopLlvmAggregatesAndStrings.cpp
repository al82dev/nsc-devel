#include "../Internal/OopLlvmImplementation.h"
#include "../Internal/DropCase.h"
#include "../Internal/LlvmExpressionFrame.h"
#include "../Internal/MatchValuePop.h"
#include "OopLlvmSupport.h"

namespace nativesharp::llvm_backend
{
using namespace llvm_api;

LLVMValueRef OopLlvmImplementation::spillAggregate(
    const frontend::ProjectOopMirNode* node,
    std::size_t index,
    std::string& error)
{
        LLVMValueRef value = lowerExpression(node->children[index].get(), error);
        if (value == nullptr) return nullptr;
        LLVMTypeRef carrier = llvmType(node->elementTypeKey, error);
        if (carrier == nullptr) return nullptr;
        LLVMValueRef slot = entryBlockAlloca(
            carrier, static_cast<unsigned>(node->alignment), "struct.root.operand");
        if (slot == nullptr) return failValue(error, "C8 could not reserve struct operand storage");
        LLVMValueRef store = LLVMBuildStore(builder_, value, slot);
        if (node->alignment > 1U) LLVMSetAlignment(store, static_cast<unsigned>(node->alignment));
        return slot;
    }

LLVMValueRef OopLlvmImplementation::loadLeafBits(
        LLVMValueRef base,
        const frontend::ProjectOopMirNode* leaf,
        std::string_view stem,
        std::string& error)
{
        LLVMTypeRef i64 = LLVMInt64TypeInContext(context_);
        LLVMTypeRef loadType = nullptr;
        switch (leaf->secondaryImmediate)
        {
            case 1: loadType = LLVMInt8TypeInContext(context_); break;
            case 2: loadType = LLVMInt16TypeInContext(context_); break;
            case 4: loadType = LLVMInt32TypeInContext(context_); break;
            case 8: loadType = i64; break;
            default: break;
        }
        if (loadType == nullptr)
        {
            return failValue(
                error,
                "C8 struct leaf '" + leaf->typeKey + "' has no scalar width the root members can read");
        }
        const std::string stemText(stem);
        LLVMValueRef address = byteOffset(base, leaf->immediate, (stemText + ".addr").c_str());
        LLVMValueRef value = LLVMBuildLoad2(builder_, loadType, address, stemText.c_str());
        // ALINEACION 1, Y NO EL ANCHO DEL CAMPO.
        //
        // Esta linea decia `LLVMSetAlignment(value, leaf->secondaryImmediate)` -- la alineacion
        // NATURAL del escalar-- y eso es una PROMESA, no una descripcion. En un struct `pack=1`
        // un `int` puede vivir en el offset 1, de modo que el modulo afirmaba `load i32 align 4`
        // sobre una direccion impar: comportamiento indefinido, y de la clase peor, porque el
        // codigo sale bien en O0 y el optimizador tiene derecho a romperlo en O2.
        //
        // La alineacion REAL de una hoja depende de dos cosas que aqui no se conocen: la del
        // agregado --que `pack(N)` puede bajar hasta 1-- y el offset. Sin las dos, el unico valor
        // que nunca miente es 1.
        //
        // Y no cuesta nada donde importa: en x86-64 una carga escalar desalineada es la misma
        // instruccion `mov`, asi que lo unico que se pierde es que el optimizador funda varias
        // cargas en una vectorial -- en una funcion de igualdad de cuatro hojas, nada. Prometer
        // de menos es gratis; prometer de mas es un miscompile.
        LLVMSetAlignment(value, 1);
        if (leaf->secondaryImmediate == 8) return value;
        return LLVMBuildZExt(builder_, value, i64, (stemText + ".bits").c_str());
    }

LLVMValueRef OopLlvmImplementation::lowerAggregateEquals(const frontend::ProjectOopMirNode* node, std::string& error)
{
        if (node->argumentCount != 2 || node->children.size() < 2 || node->alignment == 0)
            return failValue(error, "C8 struct Equals is malformed");
        LLVMValueRef left = spillAggregate(node, 0, error);
        if (left == nullptr) return nullptr;
        LLVMValueRef right = spillAggregate(node, 1, error);
        if (right == nullptr) return nullptr;

        LLVMValueRef equal = nullptr;
        for (std::size_t index = node->argumentCount; index < node->children.size(); ++index)
        {
            const frontend::ProjectOopMirNode* leaf = node->children[index].get();
            if (leaf == nullptr || leaf->kind != frontend::ProjectOopMirNodeKind::AggregateElementStore)
                return failValue(error, "C8 struct Equals has a malformed leaf");
            LLVMValueRef leftBits = loadLeafBits(left, leaf, "struct.eq.left", error);
            if (leftBits == nullptr) return nullptr;
            LLVMValueRef rightBits = loadLeafBits(right, leaf, "struct.eq.right", error);
            if (rightBits == nullptr) return nullptr;
            LLVMValueRef same = LLVMBuildICmp(
                builder_, LLVMIntEQ, leftBits, rightBits, "struct.eq.leaf");
            equal = equal == nullptr ? same : LLVMBuildAnd(builder_, equal, same, "struct.eq.and");
        }
        // A straight-line AND rather than an early-out chain: the leaf loads have no side effects
        // and cannot fault (both operands are live stack slots), so short-circuiting would only
        // add branches for the optimizer to undo.
        return equal != nullptr
            ? equal
            : LLVMConstInt(LLVMInt1TypeInContext(context_), 1, 0);
    }

LLVMValueRef OopLlvmImplementation::lowerAggregateHash(const frontend::ProjectOopMirNode* node, std::string& error)
{
        if (node->argumentCount != 1 || node->children.empty() || node->alignment == 0)
            return failValue(error, "C8 struct GetHashCode is malformed");
        LLVMValueRef value = spillAggregate(node, 0, error);
        if (value == nullptr) return nullptr;

        LLVMTypeRef i64 = LLVMInt64TypeInContext(context_);
        // FNV-1a's offset basis and prime, over 64 bits, applied one leaf at a time. Chosen for
        // the same reason it is chosen everywhere: it is two instructions per leaf and it spreads
        // low-entropy inputs (small integers, mostly-zero structs) across the whole word, which a
        // plain XOR or sum would not.
        LLVMValueRef hash = LLVMConstInt(i64, 0xCBF29CE484222325ULL, 0);
        for (std::size_t index = node->argumentCount; index < node->children.size(); ++index)
        {
            const frontend::ProjectOopMirNode* leaf = node->children[index].get();
            if (leaf == nullptr || leaf->kind != frontend::ProjectOopMirNodeKind::AggregateElementStore)
                return failValue(error, "C8 struct GetHashCode has a malformed leaf");
            LLVMValueRef bits = loadLeafBits(value, leaf, "struct.hash.leaf", error);
            if (bits == nullptr) return nullptr;
            hash = LLVMBuildXor(builder_, hash, bits, "struct.hash.xor");
            hash = LLVMBuildMul(
                builder_, hash, LLVMConstInt(i64, 0x100000001B3ULL, 0), "struct.hash.mul");
        }
        LLVMValueRef folded = LLVMBuildXor(
            builder_, hash, LLVMBuildLShr(builder_, hash, LLVMConstInt(i64, 32, 0), "struct.hash.shift"),
            "struct.hash.fold");
        return LLVMBuildTrunc(builder_, folded, LLVMInt32TypeInContext(context_), "struct.hash");
    }

LLVMValueRef OopLlvmImplementation::internTypeNameConstant(const std::string& globalName, const std::string& text)
{
        return materializeTypeNameConstant(module_, context_, typeNameGlobals_, globalName, text);
    }

LLVMValueRef OopLlvmImplementation::lowerObjectTypeName(const frontend::ProjectOopMirNode* node, std::string& error)
{
        if (node->children.size() != 1 || descriptorType_ == nullptr)
            return failValue(error, "C8 RuntimeHelpers.TypeName requires one reference operand");
        LLVMValueRef value = lowerExpression(node->children.front().get(), error);
        if (value == nullptr) return nullptr;
        LLVMTypeRef ptr = LLVMPointerTypeInContext(context_, 0);

        LLVMValueRef isNull = LLVMBuildICmp(
            builder_, LLVMIntEQ, value, LLVMConstNull(ptr), "typename.isnull");
        LLVMBasicBlockRef nullBlock = appendBlock("typename.null");
        LLVMBasicBlockRef valueBlock = appendBlock("typename.value");
        LLVMBasicBlockRef doneBlock = appendBlock("typename.done");
        LLVMBuildCondBr(builder_, isNull, nullBlock, valueBlock);

        LLVMPositionBuilderAtEnd(builder_, nullBlock);
        LLVMBuildBr(builder_, doneBlock);
        LLVMBasicBlockRef nullIncoming = LLVMGetInsertBlock(builder_);

        LLVMPositionBuilderAtEnd(builder_, valueBlock);
        LLVMValueRef descriptor = LLVMBuildLoad2(builder_, ptr, value, "typename.descriptor");
        LLVMValueRef nameAddress = LLVMBuildStructGEP2(
            builder_, descriptorType_, descriptor, 6, "descriptor.type_name.addr");
        LLVMValueRef name = LLVMBuildLoad2(builder_, ptr, nameAddress, "descriptor.type_name");
        LLVMBuildBr(builder_, doneBlock);
        LLVMBasicBlockRef valueIncoming = LLVMGetInsertBlock(builder_);

        LLVMPositionBuilderAtEnd(builder_, doneBlock);
        LLVMValueRef phi = LLVMBuildPhi(builder_, ptr, "typename.phi");
        LLVMValueRef incomingValues[] = {LLVMConstNull(ptr), name};
        LLVMBasicBlockRef incomingBlocks[] = {nullIncoming, valueIncoming};
        LLVMAddIncoming(phi, incomingValues, incomingBlocks, 2);
        return phi;
    }

LLVMValueRef OopLlvmImplementation::lowerObjectReferenceHash(
    const frontend::ProjectOopMirNode* node,
    std::string& error)
{
        if (node->children.size() != 1)
            return failValue(error, "C8 RuntimeHelpers.ReferenceHash requires one reference operand");
        LLVMValueRef value = lowerExpression(node->children.front().get(), error);
        if (value == nullptr) return nullptr;
        LLVMTypeRef i64 = LLVMInt64TypeInContext(context_);
        LLVMTypeRef i32 = LLVMInt32TypeInContext(context_);
        LLVMValueRef bits = LLVMBuildPtrToInt(builder_, value, i64, "refhash.bits");
        LLVMValueRef shifted = LLVMBuildLShr(
            builder_, bits, LLVMConstInt(i64, 33, 0), "refhash.shift0");
        LLVMValueRef mixed = LLVMBuildXor(builder_, bits, shifted, "refhash.mix0");
        mixed = LLVMBuildMul(
            builder_, mixed, LLVMConstInt(i64, 0xFF51AFD7ED558CCDULL, 0), "refhash.mul0");
        shifted = LLVMBuildLShr(builder_, mixed, LLVMConstInt(i64, 29, 0), "refhash.shift1");
        mixed = LLVMBuildXor(builder_, mixed, shifted, "refhash.mix1");
        return LLVMBuildTrunc(builder_, mixed, i32, "refhash");
    }

LLVMValueRef OopLlvmImplementation::lowerEnumName(const frontend::ProjectOopMirNode* node, std::string& error)
{
        if (node->children.size() < 2U || (node->children.size() % 2U) != 0U)
        {
            return failValue(error, "ID 129 enum name node needs a value, a fallback and pairs");
        }
        LLVMValueRef value = lowerExpression(node->children[0].get(), error);
        if (value == nullptr) return nullptr;

        // EL ANCHO DEL TIPO QUE GOBIERNA, para deduplicar por el PATRON DE BITS y no por el numero
        // escrito. Es el mismo ancho con el que `IntegerConstant` construye cada constante
        // (LLVMConstInt trunca al ancho del tipo), asi que dos enumeradores que aqui salgan
        // iguales son exactamente los que LLVM veria como el mismo caso. Deduplicar por el valor
        // de 64 bits sin enmascarar dejaria pasar un par que colisiona al truncar, y eso no seria
        // un nombre equivocado: seria un `switch` con dos casos iguales, o sea IR invalido.
        const std::string& receiverTypeKey = node->children[0]->typeKey;
        if (llvmType(receiverTypeKey, error) == nullptr) return nullptr;
        const frontend::builtins::IntegerTypeInfo* receiverEnum =
            findEnumUnderlyingInfo(nameLookup_, receiverTypeKey);
        const unsigned governingWidth =
            receiverEnum != nullptr ? receiverEnum->width : integerWidth(receiverTypeKey);
        if (governingWidth == 0 || governingWidth > 64)
        {
            return failValue(
                error, "ID 129 enum name receiver is not an integer carrier: " + receiverTypeKey);
        }
        const std::uint64_t mask = enumNameMask(governingWidth);

        LLVMTypeRef i64 = LLVMInt64TypeInContext(context_);
        LLVMValueRef governingBits = value;
        if (governingWidth < 64)
        {
            governingBits = LLVMBuildZExt(builder_, value, i64, "enum.name.bits");
        }

        // LOS PARES QUE SOBREVIVEN A LA DEDUPLICACION, en orden de declaracion. Se guarda el
        // indice del par y no el valor bajado porque las constantes y los nombres se bajan mas
        // abajo, ya dentro del bloque que les toca.
        const std::vector<std::pair<std::uint64_t, std::size_t>> cases =
            collectEnumNameCases(node, mask);

        LLVMTypeRef stringType = LLVMPointerTypeInContext(context_, 0);
        LLVMBasicBlockRef fallbackBlock = appendBlock("enum.name.fallback");
        std::vector<LLVMBasicBlockRef> caseBlocks;
        caseBlocks.reserve(cases.size());
        for (std::size_t index = 0; index < cases.size(); ++index)
        {
            caseBlocks.push_back(appendBlock("enum.name.case"));
        }
        LLVMBasicBlockRef joinBlock = appendBlock("enum.name.join");

        LLVMValueRef instruction =
            LLVMBuildSwitch(
                builder_, governingBits, fallbackBlock, static_cast<unsigned>(cases.size()));
        for (std::size_t index = 0; index < cases.size(); ++index)
        {
            LLVMValueRef constant = LLVMConstInt(i64, cases[index].first, 0);
            LLVMAddCase(instruction, constant, caseBlocks[index]);
        }

        std::vector<LLVMValueRef> incomingValues;
        std::vector<LLVMBasicBlockRef> incomingBlocks;
        incomingValues.reserve(cases.size() + 1U);
        incomingBlocks.reserve(cases.size() + 1U);

        LLVMPositionBuilderAtEnd(builder_, fallbackBlock);
        LLVMValueRef fallback = lowerExpression(node->children[1].get(), error);
        if (fallback == nullptr) return nullptr;
        incomingValues.push_back(fallback);
        incomingBlocks.push_back(LLVMGetInsertBlock(builder_));
        LLVMBuildBr(builder_, joinBlock);

        for (std::size_t index = 0; index < cases.size(); ++index)
        {
            LLVMPositionBuilderAtEnd(builder_, caseBlocks[index]);
            LLVMValueRef name =
                lowerExpression(node->children[cases[index].second + 1U].get(), error);
            if (name == nullptr) return nullptr;
            incomingValues.push_back(name);
            // El bloque que se anota es el que el constructor tiene ABIERTO, no `caseBlocks[i]`:
            // bajar el nombre podria haber abierto bloques nuevos. Hoy no lo hace --un literal de
            // cadena es un global, no una instruccion-- pero anotar el bloque de partida seria un
            // `phi` que nombra un predecesor que no lo es, y eso es IR invalido en cuanto deje de
            // ser cierto. Es el mismo cuidado que ya toma lowerStringEquals.
            incomingBlocks.push_back(LLVMGetInsertBlock(builder_));
            LLVMBuildBr(builder_, joinBlock);
        }

        LLVMPositionBuilderAtEnd(builder_, joinBlock);
        LLVMValueRef result = LLVMBuildPhi(builder_, stringType, "enum.name");
        LLVMAddIncoming(
            result,
            incomingValues.data(),
            incomingBlocks.data(),
            static_cast<unsigned>(incomingValues.size()));
        return result;
    }

std::vector<std::pair<std::uint64_t, std::size_t>> OopLlvmImplementation::collectEnumNameCases(
        const frontend::ProjectOopMirNode* node,
        std::uint64_t mask) const
{
        std::vector<std::pair<std::uint64_t, std::size_t>> cases;
        cases.reserve((node->children.size() - 2U) / 2U);
        for (std::size_t index = 2U; index + 1U < node->children.size(); index += 2U)
        {
            const std::uint64_t bits = node->children[index]->integerValue & mask;
            bool alias = false;
            for (const auto& existing : cases)
            {
                if (existing.first != bits) continue;
                alias = true;
                break;
            }
            if (!alias) cases.emplace_back(bits, index);
        }
        return cases;
    }

std::uint64_t OopLlvmImplementation::enumNameMask(unsigned governingWidth) noexcept
{
        return governingWidth >= 64
            ? ~std::uint64_t{0}
            : ((std::uint64_t{1} << governingWidth) - 1U);
    }

LLVMValueRef OopLlvmImplementation::internStringLiteral(std::string_view value, std::string& error)
{
        const auto found = stringGlobals_.find(std::string(value));
        if (found != stringGlobals_.end()) return found->second;
        LLVMTypeRef i64 = LLVMInt64TypeInContext(context_);
        LLVMTypeRef i8 = LLVMInt8TypeInContext(context_);
        LLVMTypeRef bytesType = LLVMArrayType2(i8, value.size() + 1U);
        const std::string typeName = "__nsc_string_literal_type_" + std::to_string(stringSequence_);
        LLVMTypeRef literalType = LLVMStructCreateNamed(context_, typeName.c_str());
        LLVMTypeRef fields[] = {i64, bytesType};
        LLVMStructSetBody(literalType, fields, 2, 0);
        LLVMValueRef values[] = {
            LLVMConstInt(i64, static_cast<std::uint64_t>(value.size()), 0),
            LLVMConstStringInContext2(context_, value.data(), value.size(), 0)};
        LLVMValueRef initializer = LLVMConstNamedStruct(literalType, values, 2);
        const std::string name = "__nsc_str_" + std::to_string(stringSequence_++);
        LLVMValueRef global = LLVMAddGlobal(module_, literalType, name.c_str());
        if (global == nullptr || initializer == nullptr)
            return failValue(error, "Stage 41 failed to materialize native string literal");
        LLVMSetInitializer(global, initializer);
        LLVMSetGlobalConstant(global, 1);
        // A literal belongs to this LLVM module, not to the NativeSharp public
        // symbol contract. Positional external names such as __nsc_str_0 collide
        // as soon as an executable links a static library that also contains a
        // literal. Private linkage lets each COFF object retain its own interned
        // bytes without publishing a process-wide definition.
        LLVMSetLinkage(global, LLVMPrivateLinkage);
        stringGlobals_.emplace(value, global);
        return global;
    }

LLVMValueRef OopLlvmImplementation::lowerStringLiteral(const frontend::ProjectOopMirNode* node, std::string& error)
{
        return internStringLiteral(node->stringValue, error);
    }

LLVMValueRef OopLlvmImplementation::lowerUtf8StringLiteral(const frontend::ProjectOopMirNode* node, std::string& error)
{
        // NS304-T10-F01: u8"..." is a ReadOnlySpan<byte> { byte* start, int count } over the
        // interned UTF-8 bytes. The string literal global is {i64 length, [N+1 x i8] bytes}, so the
        // span's start points at the byte array field and count is the literal's byte length.
        LLVMValueRef global = internStringLiteral(node->stringValue, error);
        if (global == nullptr) return nullptr;
        // The literal global is {i64 length, [N+1 x i8] bytes}; its byte array begins at offset 8.
        LLVMValueRef bytesField = byteOffset(global, 8, "utf8.span.bytes");
        LLVMValueRef count = LLVMConstInt(
            LLVMInt64TypeInContext(context_), node->integerValue, 0);
        LLVMTypeRef carrier = llvmType(node->typeKey, error);
        if (carrier == nullptr) return nullptr;
        const unsigned alignment = structAlignmentFor(node->typeKey);
        LLVMValueRef slot = entryBlockAlloca(carrier, alignment, "utf8.span.init");
        if (slot == nullptr)
            return failValue(error, "utf8 literal could not reserve span storage");
        LLVMBuildStore(builder_, bytesField, byteOffset(slot, 0, "utf8.span.start.addr"));
        LLVMBuildStore(builder_, count, byteOffset(slot, 8, "utf8.span.count.addr"));
        LLVMValueRef loaded = LLVMBuildLoad2(builder_, carrier, slot, "utf8.span.init.value");
        if (alignment > 1U) LLVMSetAlignment(loaded, alignment);
        return loaded;
    }

LLVMValueRef OopLlvmImplementation::nullableStringLength(LLVMValueRef value, std::string_view stem)
{
        LLVMTypeRef ptr = LLVMPointerTypeInContext(context_, 0);
        LLVMTypeRef i64 = LLVMInt64TypeInContext(context_);
        LLVMValueRef isNull = LLVMBuildICmp(
            builder_, LLVMIntEQ, value, LLVMConstNull(ptr),
            (std::string(stem) + ".isnull").c_str());
        LLVMBasicBlockRef nullBlock = appendBlock(std::string(stem) + ".null");
        LLVMBasicBlockRef valueBlock = appendBlock(std::string(stem) + ".value");
        LLVMBasicBlockRef doneBlock = appendBlock(std::string(stem) + ".done");
        LLVMBuildCondBr(builder_, isNull, nullBlock, valueBlock);

        LLVMPositionBuilderAtEnd(builder_, nullBlock);
        LLVMValueRef zero = LLVMConstInt(i64, 0, 0);
        LLVMBuildBr(builder_, doneBlock);
        LLVMBasicBlockRef nullIncoming = LLVMGetInsertBlock(builder_);

        LLVMPositionBuilderAtEnd(builder_, valueBlock);
        LLVMValueRef length = LLVMBuildLoad2(
            builder_, i64, value, (std::string(stem) + ".length").c_str());
        LLVMBuildBr(builder_, doneBlock);
        LLVMBasicBlockRef valueIncoming = LLVMGetInsertBlock(builder_);

        LLVMPositionBuilderAtEnd(builder_, doneBlock);
        LLVMValueRef phi = LLVMBuildPhi(builder_, i64, (std::string(stem) + ".length.phi").c_str());
        LLVMValueRef incomingValues[] = {zero, length};
        LLVMBasicBlockRef incomingBlocks[] = {nullIncoming, valueIncoming};
        LLVMAddIncoming(phi, incomingValues, incomingBlocks, 2);
        return phi;
    }

LLVMValueRef OopLlvmImplementation::dynamicByteOffset(
        LLVMValueRef base,
        LLVMValueRef offset,
        const char* name)
{
        LLVMValueRef indices[] = {offset};
        return LLVMBuildGEP2(
            builder_, LLVMInt8TypeInContext(context_), base, indices, 1, name);
    }

bool OopLlvmImplementation::emitMemcpyBytes(
        LLVMValueRef destination,
        LLVMValueRef source,
        LLVMValueRef length,
        std::string_view stem,
        std::string& error)
{
        if (destination == nullptr || source == nullptr || length == nullptr || memcpyFunction_ == nullptr)
            return fail(error, "Stage 51 cannot emit native string byte copy");
        LLVMValueRef hasBytes = LLVMBuildICmp(
            builder_, LLVMIntNE, length,
            LLVMConstInt(LLVMInt64TypeInContext(context_), 0, 0),
            (std::string(stem) + ".hasbytes").c_str());
        LLVMBasicBlockRef copyBlock = appendBlock(std::string(stem) + ".copy");
        LLVMBasicBlockRef doneBlock = appendBlock(std::string(stem) + ".done");
        LLVMBuildCondBr(builder_, hasBytes, copyBlock, doneBlock);
        LLVMPositionBuilderAtEnd(builder_, copyBlock);
        LLVMValueRef args[] = {
            destination,
            source,
            length,
            LLVMConstInt(LLVMInt1TypeInContext(context_), 0, 0)};
        buildNoUnwindCall(memcpyFunctionType_, memcpyFunction_, args, 4, "");
        LLVMBuildBr(builder_, doneBlock);
        LLVMPositionBuilderAtEnd(builder_, doneBlock);
        return true;
    }

LLVMValueRef OopLlvmImplementation::allocateNativeString(
        LLVMValueRef length,
        std::string_view stem,
        std::string& error)
{
        LLVMTypeRef i64 = LLVMInt64TypeInContext(context_);
        LLVMTypeRef i8 = LLVMInt8TypeInContext(context_);
        LLVMValueRef fits = LLVMBuildICmp(
            builder_, LLVMIntULE, length, LLVMConstInt(i64, 0x7fffffffULL, 0),
            (std::string(stem) + ".length.fits").c_str());
        if (!emitGuard(fits, LlvmTrapReason::AllocationSizeOverflow,
                std::string(stem) + ".length", error))
        {
            return nullptr;
        }
        LLVMValueRef bytes = LLVMBuildAdd(
            builder_, length, LLVMConstInt(i64, 9, 0),
            (std::string(stem) + ".bytes").c_str());
        LLVMValueRef args[] = {bytes};
        LLVMValueRef memory = buildNoUnwindCall(
            allocationFunctionType_, allocationFunction_, args, 1,
            (std::string(stem) + ".memory").c_str());
        memory = checkedStage42Allocation(memory, std::string(stem) + ".alloc", error);
        if (memory == nullptr) return nullptr;
        LLVMBuildStore(builder_, length, memory);
        LLVMValueRef data = byteOffset(memory, 8, (std::string(stem) + ".data").c_str());
        LLVMValueRef terminator = dynamicByteOffset(
            data, length, (std::string(stem) + ".nul.addr").c_str());
        LLVMBuildStore(builder_, LLVMConstInt(i8, 0, 0), terminator);
        return memory;
    }

LLVMValueRef OopLlvmImplementation::lowerStringConcat(
        const frontend::ProjectOopMirNode* node,
        std::string& error)
{
        if (node->children.size() != 2)
            return failValue(error, "Stage 51 string concat requires two operands");
        LLVMValueRef left = lowerExpression(node->children[0].get(), error);
        LLVMValueRef right = lowerExpression(node->children[1].get(), error);
        if (left == nullptr || right == nullptr) return nullptr;
        LLVMValueRef leftLength = nullableStringLength(left, "string.concat.left");
        LLVMValueRef rightLength = nullableStringLength(right, "string.concat.right");
        LLVMValueRef total = LLVMBuildAdd(
            builder_, leftLength, rightLength, "string.concat.length");
        LLVMValueRef noOverflow = LLVMBuildICmp(
            builder_, LLVMIntUGE, total, leftLength, "string.concat.nooverflow");
        if (!emitGuard(noOverflow, LlvmTrapReason::AllocationSizeOverflow, "string.concat.overflow", error))
            return nullptr;
        LLVMValueRef result = allocateNativeString(total, "string.concat", error);
        if (result == nullptr) return nullptr;
        LLVMValueRef resultData = byteOffset(result, 8, "string.concat.result.data");
        LLVMValueRef leftData = byteOffset(left, 8, "string.concat.left.data");
        if (!emitMemcpyBytes(resultData, leftData, leftLength, "string.concat.copy.left", error))
            return nullptr;
        LLVMValueRef rightDestination = dynamicByteOffset(
            resultData, leftLength, "string.concat.right.dest");
        LLVMValueRef rightData = byteOffset(right, 8, "string.concat.right.data");
        if (!emitMemcpyBytes(rightDestination, rightData, rightLength, "string.concat.copy.right", error))
            return nullptr;
        return result;
    }

void OopLlvmImplementation::emitEqualLengthStringComparison(
        LLVMValueRef left,
        LLVMValueRef right,
        LLVMValueRef leftLength,
        LLVMBasicBlockRef doneBlock,
        LLVMBasicBlockRef& byteMismatchIncoming,
        LLVMBasicBlockRef& equalIncoming)
{
        LLVMTypeRef i64 = LLVMInt64TypeInContext(context_);
        LLVMTypeRef i8 = LLVMInt8TypeInContext(context_);
        LLVMBasicBlockRef loopBlock = appendBlock("string.eq.loop");
        LLVMBasicBlockRef compareBlock = appendBlock("string.eq.compare");
        LLVMBasicBlockRef matchBlock = appendBlock("string.eq.match");
        LLVMBasicBlockRef mismatchByteBlock = appendBlock("string.eq.byte.mismatch");
        LLVMBasicBlockRef equalBlock = appendBlock("string.eq.equal");
        LLVMBuildBr(builder_, loopBlock);
        LLVMBasicBlockRef loopEntryIncoming = LLVMGetInsertBlock(builder_);

        LLVMPositionBuilderAtEnd(builder_, loopBlock);
        LLVMValueRef index = LLVMBuildPhi(builder_, i64, "string.eq.index");
        LLVMValueRef zero = LLVMConstInt(i64, 0, 0);
        LLVMValueRef initialValues[] = {zero};
        LLVMBasicBlockRef initialBlocks[] = {loopEntryIncoming};
        LLVMAddIncoming(index, initialValues, initialBlocks, 1);
        LLVMValueRef finished = LLVMBuildICmp(builder_, LLVMIntEQ, index, leftLength, "string.eq.finished");
        LLVMBuildCondBr(builder_, finished, equalBlock, compareBlock);

        LLVMPositionBuilderAtEnd(builder_, compareBlock);
        LLVMValueRef leftData = byteOffset(left, 8, "string.eq.left.data");
        LLVMValueRef rightData = byteOffset(right, 8, "string.eq.right.data");
        LLVMValueRef leftAddress = dynamicByteOffset(leftData, index, "string.eq.left.addr");
        LLVMValueRef rightAddress = dynamicByteOffset(rightData, index, "string.eq.right.addr");
        LLVMValueRef leftByte = LLVMBuildLoad2(builder_, i8, leftAddress, "string.eq.left.byte");
        LLVMValueRef rightByte = LLVMBuildLoad2(builder_, i8, rightAddress, "string.eq.right.byte");
        LLVMValueRef byteEqual = LLVMBuildICmp(builder_, LLVMIntEQ, leftByte, rightByte, "string.eq.byte.equal");
        LLVMBuildCondBr(builder_, byteEqual, matchBlock, mismatchByteBlock);

        LLVMPositionBuilderAtEnd(builder_, mismatchByteBlock);
        LLVMBuildBr(builder_, doneBlock);
        byteMismatchIncoming = LLVMGetInsertBlock(builder_);

        LLVMPositionBuilderAtEnd(builder_, matchBlock);
        LLVMValueRef next = LLVMBuildAdd(builder_, index, LLVMConstInt(i64, 1, 0), "string.eq.next");
        LLVMBuildBr(builder_, loopBlock);
        LLVMBasicBlockRef matchIncoming = LLVMGetInsertBlock(builder_);
        LLVMValueRef nextValues[] = {next};
        LLVMBasicBlockRef nextBlocks[] = {matchIncoming};
        LLVMAddIncoming(index, nextValues, nextBlocks, 1);

        LLVMPositionBuilderAtEnd(builder_, equalBlock);
        LLVMBuildBr(builder_, doneBlock);
        equalIncoming = LLVMGetInsertBlock(builder_);
    }

LLVMValueRef OopLlvmImplementation::lowerStringEquals(
        const frontend::ProjectOopMirNode* node,
        std::string& error)
{
        if (node->children.size() != 2)
            return failValue(error, "Stage 51 string equals requires two operands");
        LLVMTypeRef ptr = LLVMPointerTypeInContext(context_, 0);
        LLVMTypeRef i64 = LLVMInt64TypeInContext(context_);
        LLVMValueRef left = lowerExpression(node->children[0].get(), error);
        LLVMValueRef right = lowerExpression(node->children[1].get(), error);
        if (left == nullptr || right == nullptr) return nullptr;
        LLVMValueRef samePointer = LLVMBuildICmp(builder_, LLVMIntEQ, left, right, "string.eq.sameptr");
        LLVMBasicBlockRef sameBlock = appendBlock("string.eq.same");
        LLVMBasicBlockRef distinctBlock = appendBlock("string.eq.distinct");
        LLVMBasicBlockRef doneBlock = appendBlock("string.eq.done");
        LLVMBuildCondBr(builder_, samePointer, sameBlock, distinctBlock);
        LLVMPositionBuilderAtEnd(builder_, sameBlock);
        LLVMBuildBr(builder_, doneBlock);
        LLVMBasicBlockRef sameIncoming = LLVMGetInsertBlock(builder_);
        LLVMPositionBuilderAtEnd(builder_, distinctBlock);
        LLVMValueRef leftNull = LLVMBuildICmp(builder_, LLVMIntEQ, left, LLVMConstNull(ptr), "string.eq.leftnull");
        LLVMValueRef rightNull = LLVMBuildICmp(builder_, LLVMIntEQ, right, LLVMConstNull(ptr), "string.eq.rightnull");
        LLVMValueRef anyNull = LLVMBuildOr(builder_, leftNull, rightNull, "string.eq.anynull");
        LLVMBasicBlockRef nullBlock = appendBlock("string.eq.null");
        LLVMBasicBlockRef lengthBlock = appendBlock("string.eq.length");
        LLVMBuildCondBr(builder_, anyNull, nullBlock, lengthBlock);
        LLVMPositionBuilderAtEnd(builder_, nullBlock);
        LLVMBuildBr(builder_, doneBlock);
        LLVMBasicBlockRef nullIncoming = LLVMGetInsertBlock(builder_);
        LLVMPositionBuilderAtEnd(builder_, lengthBlock);
        LLVMValueRef leftLength = LLVMBuildLoad2(builder_, i64, left, "string.eq.left.length");
        LLVMValueRef rightLength = LLVMBuildLoad2(builder_, i64, right, "string.eq.right.length");
        LLVMValueRef sameLength = LLVMBuildICmp(builder_, LLVMIntEQ, leftLength, rightLength, "string.eq.samelen");
        LLVMBasicBlockRef mismatchLengthBlock = appendBlock("string.eq.length.mismatch");
        LLVMBasicBlockRef loopEntryBlock = appendBlock("string.eq.loop.entry");
        LLVMBuildCondBr(builder_, sameLength, loopEntryBlock, mismatchLengthBlock);
        LLVMPositionBuilderAtEnd(builder_, mismatchLengthBlock);
        LLVMBuildBr(builder_, doneBlock);
        LLVMBasicBlockRef lengthMismatchIncoming = LLVMGetInsertBlock(builder_);
        LLVMPositionBuilderAtEnd(builder_, loopEntryBlock);
        LLVMBasicBlockRef byteMismatchIncoming = nullptr;
        LLVMBasicBlockRef equalIncoming = nullptr;
        emitEqualLengthStringComparison(
            left, right, leftLength, doneBlock, byteMismatchIncoming, equalIncoming);

        LLVMPositionBuilderAtEnd(builder_, doneBlock);
        LLVMValueRef result = LLVMBuildPhi(builder_, LLVMInt1TypeInContext(context_), "string.eq.result");
        LLVMValueRef trueValue = LLVMConstInt(LLVMInt1TypeInContext(context_), 1, 0);
        LLVMValueRef falseValue = LLVMConstInt(LLVMInt1TypeInContext(context_), 0, 0);
        LLVMValueRef values[] = {trueValue, falseValue, falseValue, falseValue, trueValue};
        LLVMBasicBlockRef blocks[] = {
            sameIncoming, nullIncoming, lengthMismatchIncoming, byteMismatchIncoming, equalIncoming};
        LLVMAddIncoming(result, values, blocks, 5);
        if (node->operation == lexer::TokenKind::BangEqual)
            return LLVMBuildNot(builder_, result, "string.ne.result");
        return result;
    }

LLVMValueRef OopLlvmImplementation::lowerStringSubstring(
        const frontend::ProjectOopMirNode* node,
        std::string& error)
{
        if (node->children.size() != 3)
            return failValue(error, "Stage 51 Substring requires value, startIndex and length");
        LLVMTypeRef i64 = LLVMInt64TypeInContext(context_);
        LLVMValueRef value = lowerExpression(node->children[0].get(), error);
        if (value == nullptr) return nullptr;
        value = checkedReceiver(value, error);
        if (value == nullptr) return nullptr;
        LLVMValueRef start = normalizeNativeIndex(node->children[1].get(), error);
        LLVMValueRef count = normalizeNativeIndex(node->children[2].get(), error);
        if (start == nullptr || count == nullptr) return nullptr;
        if (isSignedInteger(node->children[1]->typeKey))
        {
            LLVMValueRef valid = LLVMBuildICmp(
                builder_, LLVMIntSGE, start, LLVMConstInt(i64, 0, 0), "string.substring.start.nonnegative");
            if (!emitGuard(valid, LlvmTrapReason::BoundsViolation, "string.substring.start", error)) return nullptr;
        }
        if (isSignedInteger(node->children[2]->typeKey))
        {
            LLVMValueRef valid = LLVMBuildICmp(
                builder_, LLVMIntSGE, count, LLVMConstInt(i64, 0, 0), "string.substring.count.nonnegative");
            if (!emitGuard(valid, LlvmTrapReason::BoundsViolation, "string.substring.count", error)) return nullptr;
        }
        LLVMValueRef sourceLength = LLVMBuildLoad2(builder_, i64, value, "string.substring.source.length");
        LLVMValueRef startFits =
            LLVMBuildICmp(builder_, LLVMIntULE, start, sourceLength, "string.substring.start.fits");
        if (!emitGuard(
                startFits, LlvmTrapReason::BoundsViolation, "string.substring.start.bounds", error)) return nullptr;
        LLVMValueRef remaining = LLVMBuildSub(builder_, sourceLength, start, "string.substring.remaining");
        LLVMValueRef countFits = LLVMBuildICmp(builder_, LLVMIntULE, count, remaining, "string.substring.count.fits");
        if (!emitGuard(
                countFits, LlvmTrapReason::BoundsViolation, "string.substring.count.bounds", error)) return nullptr;
        LLVMValueRef result = allocateNativeString(count, "string.substring", error);
        if (result == nullptr) return nullptr;
        LLVMValueRef sourceData = byteOffset(value, 8, "string.substring.source.data");
        sourceData = dynamicByteOffset(sourceData, start, "string.substring.source.start");
        LLVMValueRef resultData = byteOffset(result, 8, "string.substring.result.data");
        if (!emitMemcpyBytes(resultData, sourceData, count, "string.substring.copy", error)) return nullptr;
        return result;
    }

LLVMValueRef OopLlvmImplementation::lowerStringFromCharArray(
        const frontend::ProjectOopMirNode* node,
        std::string& error)
{
        if (node->children.size() != 1)
            return failValue(error, "Stage 51 FromCharArray requires one char[] operand");
        LLVMTypeRef i64 = LLVMInt64TypeInContext(context_);
        LLVMValueRef array = lowerExpression(node->children.front().get(), error);
        if (array == nullptr) return nullptr;
        array = checkedReceiver(array, error);
        if (array == nullptr) return nullptr;
        LLVMValueRef length = LLVMBuildLoad2(builder_, i64, array, "string.fromchars.length");
        LLVMValueRef result = allocateNativeString(length, "string.fromchars", error);
        if (result == nullptr) return nullptr;
        LLVMValueRef sourceData = byteOffset(array, 8, "string.fromchars.source.data");
        LLVMValueRef resultData = byteOffset(result, 8, "string.fromchars.result.data");
        if (!emitMemcpyBytes(resultData, sourceData, length, "string.fromchars.copy", error)) return nullptr;
        return result;
    }

}
