#include "ClangAbiSnapshot.h"

#include <clang/AST/ASTContext.h>
#include <clang/AST/Decl.h>
#include <clang/AST/GlobalDecl.h>
#include <clang/CodeGen/CodeGenABITypes.h>
#include <clang/CodeGen/ModuleBuilder.h>
#include <llvm/IR/Attributes.h>
#include <llvm/IR/DerivedTypes.h>
#include <llvm/IR/Function.h>
#include <llvm/IR/Module.h>

namespace t09_probe
{
namespace
{
bool supportedAttributes(llvm::AttributeSet attributes, bool isReturn)
{
    for (const auto attribute : attributes)
    {
        if (attribute.isStringAttribute()) return false;
        switch (attribute.getKindAsEnum())
        {
            case llvm::Attribute::NoUndef:
            case llvm::Attribute::SExt:
            case llvm::Attribute::ZExt:
                break;
            case llvm::Attribute::NoAlias:
            case llvm::Attribute::InReg:
            case llvm::Attribute::StructRet:
            case llvm::Attribute::ByVal:
            case llvm::Attribute::Alignment:
            case llvm::Attribute::DeadOnUnwind:
            case llvm::Attribute::DeadOnReturn:
            case llvm::Attribute::Writable:
                if (isReturn) return false;
                break;
            default:
                return false;
        }
    }
    return true;
}

std::optional<Type> copyType(const llvm::Type* input, unsigned depth)
{
    if (input == nullptr || depth > 32) return std::nullopt;
    Type result;
    if (input->isVoidTy()) result.kind = TypeKind::Void;
    else if (input->isIntegerTy())
    {
        result.kind = TypeKind::Integer;
        result.widthOrCount = input->getIntegerBitWidth();
    }
    else if (input->isFloatTy()) result.kind = TypeKind::Float;
    else if (input->isDoubleTy()) result.kind = TypeKind::Double;
    else if (input->isPointerTy())
    {
        result.kind = TypeKind::Pointer;
        result.widthOrCount = input->getPointerAddressSpace();
    }
    else if (const auto* structure = llvm::dyn_cast<llvm::StructType>(input))
    {
        if (structure->isOpaque()) return std::nullopt;
        result.kind = TypeKind::Structure;
        result.packed = structure->isPacked();
        for (const llvm::Type* member : structure->elements())
        {
            auto copied = copyType(member, depth + 1);
            if (!copied) return std::nullopt;
            result.elements.push_back(std::move(*copied));
        }
    }
    else if (const auto* array = llvm::dyn_cast<llvm::ArrayType>(input))
    {
        result.kind = TypeKind::Array;
        result.widthOrCount = array->getNumElements();
        auto element = copyType(array->getElementType(), depth + 1);
        if (!element) return std::nullopt;
        result.elements.push_back(std::move(*element));
    }
    else return std::nullopt;
    return result;
}

std::optional<LogicalValue> copyLogical(const clang::CodeGen::ABIArgInfo& input)
{
    LogicalValue result;
    if (input.isDirect() || input.isExtend())
    {
        result.passing = input.isDirect() ? Passing::Direct : Passing::Extend;
        result.offset = input.getDirectOffset();
        result.alignment = input.getDirectAlign();
        if (input.isDirect()) result.flatten = input.getCanBeFlattened();
        if (input.isExtend())
        {
            result.signExtend = input.isSignExt();
            result.zeroExtend = input.isZeroExt();
        }
        if (input.getCoerceToType())
        {
            result.coercion = copyType(input.getCoerceToType(), 0);
            if (!result.coercion) return std::nullopt;
        }
    }
    else if (input.isIndirect())
    {
        result.passing = Passing::Indirect;
        result.alignment = static_cast<std::uint64_t>(input.getIndirectAlign().getQuantity());
        result.byValue = input.getIndirectByVal();
    }
    else if (input.isIgnore()) result.passing = Passing::Ignore;
    else return std::nullopt;
    return result;
}
}

std::optional<Plan> copyPlan(clang::CodeGenerator& generator,
                             const clang::FunctionDecl& function,
                             std::string& error)
{
    error.clear();
    const auto canonical = function.getASTContext().getCanonicalType(function.getType());
    clang::CanQual<clang::FunctionProtoType> prototype = canonical.getAs<clang::FunctionProtoType>();
    if (!prototype) { error = "function prototype unavailable"; return std::nullopt; }
    auto* physical = clang::CodeGen::convertFreeFunctionType(generator.CGM(), &function);
    if (!physical || !generator.GetAddrOfGlobal(clang::GlobalDecl(&function), false))
    { error = "physical function unavailable"; return std::nullopt; }
    auto* declaration = generator.GetModule()->getFunction(generator.GetMangledName(clang::GlobalDecl(&function)));
    if (!declaration || declaration->getFunctionType() != physical)
    { error = "physical declaration unavailable"; return std::nullopt; }
    if (!supportedAttributes(declaration->getAttributes().getRetAttrs(), true))
    { error = "return attribute not represented"; return std::nullopt; }
    const auto& logical = clang::CodeGen::arrangeFreeFunctionType(generator.CGM(), prototype);
    Plan result;
    result.symbol = declaration->getName().str();
    result.targetTriple = generator.GetModule()->getTargetTriple().str();
    result.dataLayout = generator.GetModule()->getDataLayoutStr();
    if (result.symbol.empty() || result.targetTriple.empty() || result.dataLayout.empty())
    { error = "symbol or target configuration unavailable"; return std::nullopt; }
    result.callingConvention = declaration->getCallingConv();
    result.variadic = physical->isVarArg();
    if (result.variadic) { error = "variadic call plan not represented"; return std::nullopt; }
    result.returnSignExtend = declaration->hasRetAttribute(llvm::Attribute::SExt);
    result.returnZeroExtend = declaration->hasRetAttribute(llvm::Attribute::ZExt);
    auto returnType = copyType(physical->getReturnType(), 0);
    auto returnInfo = copyLogical(logical.getReturnInfo());
    if (!returnType || !returnInfo) { error = "return plan not represented"; return std::nullopt; }
    result.physicalReturn = std::move(*returnType);
    result.logicalReturn = std::move(*returnInfo);
    for (const auto& argument : logical.arguments())
    {
        auto value = copyLogical(argument.info);
        if (!value) { error = "logical argument plan not represented"; return std::nullopt; }
        result.logicalParameters.push_back(std::move(*value));
    }
    for (unsigned index = 0; index < physical->getNumParams(); ++index)
    {
        if (!supportedAttributes(declaration->getAttributes().getParamAttrs(index), false))
        { error = "parameter attribute not represented"; return std::nullopt; }
        PhysicalParameter parameter;
        auto type = copyType(physical->getParamType(index), 0);
        if (!type) { error = "physical parameter type not represented"; return std::nullopt; }
        parameter.type = std::move(*type);
        parameter.structureReturn = declaration->hasParamAttribute(index, llvm::Attribute::StructRet);
        parameter.byValue = declaration->hasParamAttribute(index, llvm::Attribute::ByVal);
        parameter.signExtend = declaration->hasParamAttribute(index, llvm::Attribute::SExt);
        parameter.zeroExtend = declaration->hasParamAttribute(index, llvm::Attribute::ZExt);
        parameter.inRegister = declaration->hasParamAttribute(index, llvm::Attribute::InReg);
        parameter.noAlias = declaration->hasParamAttribute(index, llvm::Attribute::NoAlias);
        if (const auto alignment = declaration->getParamAlign(index)) parameter.alignment = alignment->value();
        const llvm::Type* pointee = parameter.structureReturn ? declaration->getParamStructRetType(index) :
            parameter.byValue ? declaration->getParamByValType(index) : nullptr;
        if (pointee)
        {
            parameter.pointee = copyType(pointee, 0);
            if (!parameter.pointee) { error = "physical pointee type not represented"; return std::nullopt; }
        }
        result.physicalParameters.push_back(std::move(parameter));
    }
    unsigned next = 0;
    if (logical.getReturnInfo().isIndirect())
    {
        if (result.physicalParameters.empty() || !result.physicalParameters[0].structureReturn)
        { error = "hidden return mapping not represented"; return std::nullopt; }
        next = 1;
    }
    else if (!logical.getReturnInfo().isIgnore())
    {
        const auto& ret = logical.getReturnInfo();
        if ((!ret.isDirect() && !ret.isExtend()) || ret.getCoerceToType() != physical->getReturnType())
        { error = "direct return mapping not represented"; return std::nullopt; }
    }
    for (unsigned ordinal = 0; ordinal < logical.arg_size(); ++ordinal)
    {
        const auto& info = logical.arguments()[ordinal].info;
        auto& value = result.logicalParameters[ordinal];
        value.firstPhysical = next;
        if (info.getPaddingType()) { error = "argument padding not represented"; return std::nullopt; }
        if (info.isIgnore()) continue;
        if (info.isIndirect())
        {
            if (next >= physical->getNumParams() || !physical->getParamType(next)->isPointerTy() ||
                result.physicalParameters[next].byValue != info.getIndirectByVal())
            { error = "indirect argument mapping not represented"; return std::nullopt; }
            value.physicalCount = 1;
            ++next;
            continue;
        }
        if (!info.isDirect() && !info.isExtend())
        { error = "argument mapping not represented"; return std::nullopt; }
        auto* coerce = info.getCoerceToType();
        if (!coerce) { error = "argument coercion unavailable"; return std::nullopt; }
        if (const auto* structure = llvm::dyn_cast<llvm::StructType>(coerce);
            structure && info.isDirect() && info.getCanBeFlattened())
        {
            for (llvm::Type* part : structure->elements())
            {
                if (next >= physical->getNumParams() || physical->getParamType(next) != part)
                { error = "flattened argument mapping not represented"; return std::nullopt; }
                ++next;
                ++value.physicalCount;
            }
        }
        else
        {
            if (next >= physical->getNumParams() || physical->getParamType(next) != coerce)
            { error = "direct argument mapping not represented"; return std::nullopt; }
            ++next;
            value.physicalCount = 1;
        }
    }
    if (next != physical->getNumParams())
    { error = "unmapped physical parameters"; return std::nullopt; }
    return result;
}
}
