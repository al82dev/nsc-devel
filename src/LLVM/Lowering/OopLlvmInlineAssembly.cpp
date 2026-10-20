#include "../Internal/OopLlvmImplementation.h"
#include "../Internal/DropCase.h"
#include "../Internal/LlvmExpressionFrame.h"
#include "../Internal/MatchValuePop.h"
#include "OopLlvmSupport.h"

namespace nativesharp::llvm_backend
{
using namespace llvm_api;

bool OopLlvmImplementation::collectInlineAssemblyRecords(
        const frontend::ProjectOopMirNode* node,
        std::vector<const frontend::ProjectOopMirNode*>& lines,
        std::vector<const frontend::ProjectOopMirNode*>& inputs,
        std::vector<const frontend::ProjectOopMirNode*>& outputs,
        std::vector<const frontend::ProjectOopMirNode*>& clobbers,
        std::string& error)
{
        for (const auto& childOwner : node->children)
        {
            const frontend::ProjectOopMirNode* child = childOwner.get();
            if (child == nullptr) return fail(error, "inline assembly MIR contains a null record");
            switch (child->kind)
            {
                case frontend::ProjectOopMirNodeKind::InlineAssemblyLine: lines.push_back(child); break;
                case frontend::ProjectOopMirNodeKind::InlineAssemblyInput: inputs.push_back(child); break;
                case frontend::ProjectOopMirNodeKind::InlineAssemblyOutput: outputs.push_back(child); break;
                case frontend::ProjectOopMirNodeKind::InlineAssemblyClobber: clobbers.push_back(child); break;
                default: return fail(error, "inline assembly MIR contains a non-contract child");
            }
        }
        const auto byOrdinal = [](const auto* left, const auto* right)
        {
            return left->integerValue < right->integerValue;
        };
        std::stable_sort(inputs.begin(), inputs.end(), byOrdinal);
        std::stable_sort(outputs.begin(), outputs.end(), byOrdinal);
        std::stable_sort(clobbers.begin(), clobbers.end(), byOrdinal);
        const auto ordinalsAreDense = [](const auto& records)
        {
            for (std::size_t index = 0; index < records.size(); ++index)
                if (records[index]->integerValue != index) return false;
            return true;
        };
        if (!ordinalsAreDense(inputs) || !ordinalsAreDense(outputs) || !ordinalsAreDense(clobbers))
            return fail(error, "inline assembly MIR operand or clobber ordinals are not dense");
        return true;
    }

bool OopLlvmImplementation::collectInlineAssemblyOutputs(
        const std::vector<const frontend::ProjectOopMirNode*>& outputs,
        std::vector<std::string>& constraintParts,
        std::vector<LLVMTypeRef>& resultTypes,
        std::vector<LLVMValueRef>& resultAddresses,
        std::vector<LLVMTypeRef>& parameterTypes,
        std::vector<LLVMValueRef>& arguments,
        std::vector<std::pair<std::size_t, LLVMTypeRef>>& indirectElementTypes,
        std::vector<std::pair<LLVMValueRef, LLVMTypeRef>>& readWriteMemoryInputs,
        std::string& error)
{
        for (const frontend::ProjectOopMirNode* output : outputs)
        {
            if (output->children.size() != 1 || output->children.front() == nullptr)
                return fail(error, "inline assembly output has no storage address");
            LLVMValueRef address = lowerExpression(output->children.front().get(), error);
            LLVMTypeRef valueType = llvmType(output->typeKey, error);
            if (address == nullptr || valueType == nullptr) return false;
            std::string constraint = output->stringValue;
            const bool memory = constraint == "=m" || constraint == "+m";
            const bool readWrite = !constraint.empty() && constraint.front() == '+';
            if (memory)
            {
                constraintParts.push_back("=*m");
                parameterTypes.push_back(LLVMPointerTypeInContext(context_, 0));
                arguments.push_back(address);
                indirectElementTypes.emplace_back(arguments.size() - 1, valueType);
                if (readWrite) readWriteMemoryInputs.emplace_back(address, valueType);
                continue;
            }
            constraintParts.push_back(std::move(constraint));
            resultTypes.push_back(valueType);
            resultAddresses.push_back(address);
            if (readWrite)
            {
                parameterTypes.push_back(valueType);
                arguments.push_back(LLVMBuildLoad2(
                    builder_, valueType, address, "asm.output.input"));
            }
        }
        return true;
    }

bool OopLlvmImplementation::collectInlineAssemblyInputs(
        const std::vector<const frontend::ProjectOopMirNode*>& inputs,
        std::vector<std::string>& constraintParts,
        std::vector<LLVMTypeRef>& parameterTypes,
        std::vector<LLVMValueRef>& arguments,
        std::vector<std::pair<std::size_t, LLVMTypeRef>>& indirectElementTypes,
        std::string& error)
{
        for (const frontend::ProjectOopMirNode* input : inputs)
        {
            if (input->children.size() != 1 || input->children.front() == nullptr)
                return fail(error, "inline assembly input has no lowered operand");
            LLVMValueRef value = lowerExpression(input->children.front().get(), error);
            if (value == nullptr) return false;
            std::string constraint = input->stringValue;
            const bool memory = constraint == "m";
            if (memory) constraint.insert(0, "*");
            LLVMTypeRef parameterType = memory
                ? LLVMPointerTypeInContext(context_, 0) : llvmType(input->typeKey, error);
            if (parameterType == nullptr) return false;
            constraintParts.push_back(std::move(constraint));
            parameterTypes.push_back(parameterType);
            arguments.push_back(value);
            if (!memory) continue;
            LLVMTypeRef elementType = llvmType(input->typeKey, error);
            if (elementType == nullptr) return false;
            indirectElementTypes.emplace_back(arguments.size() - 1, elementType);
        }
        return true;
    }

bool OopLlvmImplementation::collectInlineAssemblyClobbers(
        const frontend::ProjectOopMirNode* node,
        const std::vector<const frontend::ProjectOopMirNode*>& clobbers,
        std::vector<std::string>& constraintParts,
        std::string& error)
{
        bool sawMemoryClobber = false;
        bool sawFlagsClobber = false;
        for (const frontend::ProjectOopMirNode* clobber : clobbers)
        {
            sawFlagsClobber = sawFlagsClobber || clobber->elementTypeKey == "flags";
            sawMemoryClobber = sawMemoryClobber || clobber->elementTypeKey == "memory";
            const std::string name = clobber->elementTypeKey == "flags" ? "cc"
                : clobber->elementTypeKey == "memory" ? "memory" : clobber->stringValue;
            constraintParts.push_back("~{" + name + "}");
        }
        if (sawMemoryClobber != node->inlineAssemblyClobbersMemory ||
            sawFlagsClobber != node->inlineAssemblyClobbersFlags)
            return fail(error, "inline assembly MIR barrier summary disagrees with its clobbers");
        return true;
    }

bool OopLlvmImplementation::rewriteInlineAssemblyPlaceholders(
        std::string_view source,
        std::size_t outputCount,
        const std::vector<std::size_t>& inputConstraintOrdinals,
        std::string& rewritten,
        std::string& error)
{
        for (std::size_t index = 0; index < source.size();)
        {
            if (source[index] != '$')
            {
                rewritten.push_back(source[index++]);
                continue;
            }
            if (index + 1 < source.size() && source[index + 1] == '$')
            {
                rewritten.append("$$");
                index += 2;
                continue;
            }
            const bool isOutput = source.substr(index + 1, 3) == "out";
            const std::size_t digits = index + (isOutput ? 4 : 1);
            if (digits >= source.size() || !std::isdigit(static_cast<unsigned char>(source[digits])))
                return fail(error, "inline assembly contains an invalid operand placeholder");
            std::size_t end = digits;
            std::size_t ordinal = 0;
            while (end < source.size() && std::isdigit(static_cast<unsigned char>(source[end])))
            {
                ordinal = ordinal * 10 + static_cast<std::size_t>(source[end] - '0');
                ++end;
            }
            if ((isOutput && ordinal >= outputCount) ||
                (!isOutput && ordinal >= inputConstraintOrdinals.size()))
                return fail(error, "inline assembly operand placeholder is out of range");
            const std::size_t llvmOrdinal = isOutput ? ordinal : inputConstraintOrdinals[ordinal];
            rewritten.push_back('$');
            rewritten.append(std::to_string(llvmOrdinal));
            index = end;
        }
        return true;
    }

bool OopLlvmImplementation::buildInlineAssemblyText(
        const std::vector<const frontend::ProjectOopMirNode*>& lines,
        std::size_t outputCount,
        const std::vector<std::size_t>& inputConstraintOrdinals,
        std::string& assembly,
        std::string& error)
{
        const auto appendAssemblyLine = [&](std::string_view text)
        {
            if (!assembly.empty()) assembly.push_back('\n');
            assembly.append(text);
        };
        for (const frontend::ProjectOopMirNode* line : lines)
        {
            std::string rewritten;
            if (!rewriteInlineAssemblyPlaceholders(
                    line->stringValue, outputCount, inputConstraintOrdinals,
                    rewritten, error)) return false;
            if (debugInformation_ != DebugInformationKind::None && sourceManager_ != nullptr &&
                line->syntax != nullptr && line->syntax->range().isValid())
            {
                const SourceLocation location = line->syntax->range().begin();
                const unsigned fileNumber = debugCodeViewFileNumber(location);
                if (fileNumber != 0)
                {
                    const source::SourcePosition position = sourceManager_->getPosition(location);
                    appendAssemblyLine(
                        ".cv_loc 0 " + std::to_string(fileNumber) + " " +
                        std::to_string(position.line) + " " + std::to_string(position.column));
                }
            }
            constexpr std::string_view labelPrefix = "label:";
            constexpr std::string_view branchPrefix = "branch:";
            if (line->elementTypeKey.starts_with(labelPrefix))
            {
                const std::string label = line->elementTypeKey.substr(labelPrefix.size());
                appendAssemblyLine(".Lnsc_" + label + "_${:uid}:");
            }
            else if (line->elementTypeKey.starts_with(branchPrefix))
            {
                const std::size_t separator = line->elementTypeKey.find(':', branchPrefix.size());
                if (separator == std::string::npos)
                    return fail(error, "inline assembly branch record is incomplete");
                const std::string mnemonic = line->elementTypeKey.substr(
                    branchPrefix.size(), separator - branchPrefix.size());
                const std::string label = line->elementTypeKey.substr(separator + 1);
                appendAssemblyLine(mnemonic + " .Lnsc_" + label + "_${:uid}");
            }
            else appendAssemblyLine(rewritten);
        }
        return true;
    }

bool OopLlvmImplementation::emitInlineAssemblyCall(
        const frontend::ProjectOopMirNode* node,
        const std::vector<std::string>& constraintParts,
        const std::vector<LLVMTypeRef>& resultTypes,
        const std::vector<LLVMValueRef>& resultAddresses,
        const std::vector<LLVMTypeRef>& parameterTypes,
        const std::vector<LLVMValueRef>& arguments,
        const std::vector<std::pair<std::size_t, LLVMTypeRef>>& indirectElementTypes,
        std::string_view assembly,
        std::string& error)
{
        std::string constraints;
        for (std::size_t index = 0; index < constraintParts.size(); ++index)
        {
            if (index != 0) constraints.push_back(',');
            constraints.append(constraintParts[index]);
        }
        LLVMTypeRef returnType = LLVMVoidTypeInContext(context_);
        if (resultTypes.size() == 1) returnType = resultTypes.front();
        else if (resultTypes.size() > 1)
            returnType = LLVMStructTypeInContext(
                context_, resultTypes.data(), static_cast<unsigned>(resultTypes.size()), 0);
        LLVMTypeRef functionType = LLVMFunctionType(
            returnType, parameterTypes.empty() ? nullptr : parameterTypes.data(),
            static_cast<unsigned>(parameterTypes.size()), 0);
        LLVMValueRef inlineAssembly = LLVMGetInlineAsm(
            functionType, assembly.data(), assembly.size(), constraints.data(), constraints.size(),
            node->inlineAssemblyHasSideEffects ? 1 : 0, 0, LLVMInlineAsmDialectIntel, 0);
        if (inlineAssembly == nullptr)
            return fail(error, "LLVM rejected the integrated assembly contract");
        LLVMValueRef call = LLVMBuildCall2(
            builder_, functionType, inlineAssembly,
            arguments.empty() ? nullptr : arguments.data(),
            static_cast<unsigned>(arguments.size()), resultTypes.empty() ? "" : "asm.result");
        if (call == nullptr) return fail(error, "LLVM could not build the integrated assembly call");
        const unsigned elementTypeKind = LLVMGetEnumAttributeKindForName("elementtype", 11);
        for (const auto& [argumentIndex, elementType] : indirectElementTypes)
        {
            LLVMAddCallSiteAttribute(
                call, static_cast<LLVMAttributeIndex>(argumentIndex + 1),
                LLVMCreateTypeAttribute(context_, elementTypeKind, elementType));
        }
        for (std::size_t index = 0; index < resultAddresses.size(); ++index)
        {
            LLVMValueRef value = resultTypes.size() == 1 ? call :
                LLVMBuildExtractValue(builder_, call, static_cast<unsigned>(index), "asm.output");
            LLVMBuildStore(builder_, value, resultAddresses[index]);
        }
        return true;
    }

bool OopLlvmImplementation::lowerInlineAssembly(const frontend::ProjectOopMirNode* node, std::string& error)
{
        if (node == nullptr || node->kind != frontend::ProjectOopMirNodeKind::InlineAssembly)
            return fail(error, "inline assembly lowering requires an assembly MIR operation");

        std::vector<const frontend::ProjectOopMirNode*> lines;
        std::vector<const frontend::ProjectOopMirNode*> inputs;
        std::vector<const frontend::ProjectOopMirNode*> outputs;
        std::vector<const frontend::ProjectOopMirNode*> clobbers;
        if (!collectInlineAssemblyRecords(
                node, lines, inputs, outputs, clobbers, error)) return false;
        if (!node->inlineAssemblyHasSideEffects)
            return fail(error, "inline assembly MIR lost its mandatory side-effect barrier");

        std::vector<std::string> constraintParts;
        std::vector<LLVMTypeRef> resultTypes;
        std::vector<LLVMValueRef> resultAddresses;
        std::vector<LLVMTypeRef> parameterTypes;
        std::vector<LLVMValueRef> arguments;
        std::vector<std::pair<std::size_t, LLVMTypeRef>> indirectElementTypes;
        std::vector<std::pair<LLVMValueRef, LLVMTypeRef>> readWriteMemoryInputs;
        constraintParts.reserve(outputs.size() + inputs.size() + clobbers.size());

        if (!collectInlineAssemblyOutputs(
                outputs, constraintParts, resultTypes, resultAddresses, parameterTypes,
                arguments, indirectElementTypes, readWriteMemoryInputs, error)) return false;

        for (const auto& [address, elementType] : readWriteMemoryInputs)
        {
            constraintParts.push_back("*m");
            parameterTypes.push_back(LLVMPointerTypeInContext(context_, 0));
            arguments.push_back(address);
            indirectElementTypes.emplace_back(arguments.size() - 1, elementType);
        }

        if (!collectInlineAssemblyInputs(
                inputs, constraintParts, parameterTypes, arguments,
                indirectElementTypes, error) ||
            !collectInlineAssemblyClobbers(node, clobbers, constraintParts, error)) return false;

        std::vector<std::size_t> inputConstraintOrdinals(inputs.size());
        for (std::size_t index = 0; index < inputs.size(); ++index)
            inputConstraintOrdinals[index] = outputs.size() + readWriteMemoryInputs.size() + index;

        std::string assembly;
        if (!buildInlineAssemblyText(
                lines, outputs.size(), inputConstraintOrdinals, assembly, error)) return false;
        return emitInlineAssemblyCall(
            node, constraintParts, resultTypes, resultAddresses, parameterTypes,
            arguments, indirectElementTypes, assembly, error);
    }

}
