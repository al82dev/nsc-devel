#include "../Internal/ClangAbiPlan.h"

#include <clang/AST/ASTContext.h>
#include <clang/AST/Decl.h>
#include <clang/AST/GlobalDecl.h>
#include <clang/CodeGen/CodeGenABITypes.h>
#include <clang/CodeGen/ModuleBuilder.h>
#include <llvm/IR/Attributes.h>
#include <llvm/IR/DerivedTypes.h>
#include <llvm/IR/Function.h>
#include <llvm/IR/Module.h>

namespace nativesharp::interop::abi_plan
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
    else if (const auto* vector = llvm::dyn_cast<llvm::FixedVectorType>(input))
    {
        if (vector->getNumElements() != 2 || !vector->getElementType()->isFloatTy()) return std::nullopt;
        result.kind = TypeKind::Vector;
        result.widthOrCount = 2;
        Type element;
        element.kind = TypeKind::Float;
        result.elements.push_back(std::move(element));
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

std::optional<VariadicCallPlan> copyVariadicCallPlan(
    clang::CodeGenerator& generator, const clang::FunctionDecl& function,
    std::span<const clang::QualType> argumentTypes, std::string& error)
{
    error.clear();
    auto& context = function.getASTContext();
    clang::CanQual<clang::FunctionProtoType> prototype =
        context.getCanonicalType(function.getType()).getAs<clang::FunctionProtoType>();
    if (!prototype || !prototype->isVariadic() || argumentTypes.size() < prototype->getNumParams())
    { error = "variadic prototype or required arguments unavailable"; return std::nullopt; }
    if (context.getLangOpts().CPlusPlus || prototype->getExtInfo().getCC() != clang::CC_X86_64SysV ||
        !generator.GetModule()->getTargetTriple().str().starts_with("x86_64-pc-windows-msvc"))
    { error = "variadic call target or convention not represented"; return std::nullopt; }
    if (prototype->hasExtParameterInfos())
    { error = "extended parameter information not represented"; return std::nullopt; }
    std::vector<clang::CanQualType> canonical;
    canonical.reserve(argumentTypes.size());
    for (std::size_t i = 0; i < argumentTypes.size(); ++i)
    {
        const auto type = argumentTypes[i];
        if (type.isNull() || type->isIncompleteType() || type->isVoidType() ||
            type->isArrayType() || type->isFunctionType())
        { error = "actual argument type not represented"; return std::nullopt; }
        const auto value = context.getCanonicalType(type);
        if (i < prototype->getNumParams() && value != prototype->getParamType(i))
        { error = "fixed argument type mismatch"; return std::nullopt; }
        if (i >= prototype->getNumParams() &&
            (type->isSpecificBuiltinType(clang::BuiltinType::Float) || context.isPromotableIntegerType(type)))
        { error = "variadic argument has not undergone default promotions"; return std::nullopt; }
        canonical.push_back(value);
    }
    if (!generator.GetAddrOfGlobal(clang::GlobalDecl(&function), false))
    { error = "physical declaration unavailable"; return std::nullopt; }
    auto* declaration = generator.GetModule()->getFunction(generator.GetMangledName(clang::GlobalDecl(&function)));
    if (!declaration || !supportedAttributes(declaration->getAttributes().getRetAttrs(), true))
    { error = "return declaration or attribute not represented"; return std::nullopt; }
    for (unsigned i = 0; i < declaration->arg_size(); ++i)
        if (!supportedAttributes(declaration->getAttributes().getParamAttrs(i), false))
        { error = "fixed parameter attribute not represented"; return std::nullopt; }
    const auto& call = clang::CodeGen::arrangeFreeFunctionCall(generator.CGM(),
        context.getCanonicalType(function.getReturnType()), canonical, prototype->getExtInfo(),
        clang::CodeGen::RequiredArgs::forPrototype(prototype));
    auto copiedReturn = copyLogical(call.getReturnInfo());
    if (!copiedReturn) { error = "return plan not represented"; return std::nullopt; }
    VariadicCallPlan result;
    result.symbol = declaration->getName().str();
    result.targetTriple = generator.GetModule()->getTargetTriple().str();
    result.dataLayout = generator.GetModule()->getDataLayoutStr();
    result.callingConvention = declaration->getCallingConv();
    result.requiredArguments = prototype->getNumParams();
    result.result = std::move(*copiedReturn);
    auto physicalReturn = copyType(declaration->getReturnType(), 0);
    if (!physicalReturn) { error = "physical return type not represented"; return std::nullopt; }
    result.physicalReturn = std::move(*physicalReturn);
    result.returnSignExtend = declaration->hasRetAttribute(llvm::Attribute::SExt);
    result.returnZeroExtend = declaration->hasRetAttribute(llvm::Attribute::ZExt);
    if (result.symbol.empty() || result.targetTriple.empty() || result.dataLayout.empty())
    { error = "symbol or target configuration unavailable"; return std::nullopt; }
    std::vector<llvm::Type*> physicalTypes;
    if (call.getReturnInfo().isIndirect())
    {
        if (declaration->arg_size() == 0 || !declaration->hasParamAttribute(0, llvm::Attribute::StructRet))
        { error = "hidden return not represented"; return std::nullopt; }
        PhysicalParameter hidden;
        hidden.type.kind = TypeKind::Pointer;
        hidden.structureReturn = true;
        if (const auto alignment = declaration->getParamAlign(0)) hidden.alignment = alignment->value();
        hidden.pointee = copyType(declaration->getParamStructRetType(0), 0);
        if (!hidden.pointee) { error = "hidden return type not represented"; return std::nullopt; }
        result.physicalParameters.push_back(std::move(hidden));
        physicalTypes.push_back(declaration->getFunctionType()->getParamType(0));
    }
    else if (!call.getReturnInfo().isIgnore() &&
             ((!call.getReturnInfo().isDirect() && !call.getReturnInfo().isExtend()) ||
              call.getReturnInfo().getCoerceToType() != declaration->getReturnType()))
    { error = "physical return mapping not represented"; return std::nullopt; }
    std::size_t ordinal = 0;
    std::size_t fixedPhysicalCount = physicalTypes.size();
    for (const auto& argument : call.arguments())
    {
        const auto& info = argument.info;
        auto value = copyLogical(info);
        if (!value || info.getPaddingType())
        { error = "actual argument plan not represented"; return std::nullopt; }
        value->firstPhysical = physicalTypes.size();
        if (info.isIndirect())
        {
            if (!info.getIndirectByVal()) { error = "indirect argument without byval not represented"; return std::nullopt; }
            auto* memory = clang::CodeGen::convertTypeForMemory(generator.CGM(), argument.type);
            PhysicalParameter parameter;
            parameter.type.kind = TypeKind::Pointer;
            parameter.byValue = true;
            parameter.alignment = static_cast<std::uint64_t>(info.getIndirectAlign().getQuantity());
            parameter.pointee = copyType(memory, 0);
            if (!parameter.pointee) { error = "indirect argument type not represented"; return std::nullopt; }
            result.physicalParameters.push_back(std::move(parameter));
            physicalTypes.push_back(llvm::PointerType::getUnqual(generator.GetModule()->getContext()));
            value->physicalCount = 1;
        }
        else if (info.isDirect() || info.isExtend())
        {
            auto* coercion = info.getCoerceToType();
            if (!coercion || info.getDirectOffset() != 0)
            { error = "actual argument coercion or offset not represented"; return std::nullopt; }
            const auto* structure = llvm::dyn_cast<llvm::StructType>(coercion);
            const auto appendPiece = [&](llvm::Type* part) {
                auto type = copyType(part, 0);
                if (!type) return false;
                PhysicalParameter parameter;
                parameter.type = std::move(*type);
                parameter.signExtend = info.isExtend() && info.isSignExt();
                parameter.zeroExtend = info.isExtend() && info.isZeroExt();
                result.physicalParameters.push_back(std::move(parameter));
                physicalTypes.push_back(part);
                ++value->physicalCount;
                return true;
            };
            if (structure && info.isDirect() && info.getCanBeFlattened())
            {
                for (auto* part : structure->elements())
                    if (!appendPiece(part)) { error = "physical piece not represented"; return std::nullopt; }
            }
            else if (!appendPiece(coercion)) { error = "physical piece not represented"; return std::nullopt; }
        }
        else if (!info.isIgnore())
        { error = "actual argument mapping not represented"; return std::nullopt; }
        result.arguments.push_back(std::move(*value));
        if (++ordinal == result.requiredArguments) fixedPhysicalCount = physicalTypes.size();
    }
    if (fixedPhysicalCount != declaration->arg_size())
    { error = "fixed physical argument count mismatch"; return std::nullopt; }
    for (unsigned i = 0; i < declaration->arg_size(); ++i)
    {
        if (physicalTypes[i] != declaration->getFunctionType()->getParamType(i))
        { error = "fixed physical argument type mismatch"; return std::nullopt; }
        auto& parameter = result.physicalParameters[i];
        if (parameter.structureReturn != declaration->hasParamAttribute(i, llvm::Attribute::StructRet) ||
            parameter.byValue != declaration->hasParamAttribute(i, llvm::Attribute::ByVal) ||
            parameter.signExtend != declaration->hasParamAttribute(i, llvm::Attribute::SExt) ||
            parameter.zeroExtend != declaration->hasParamAttribute(i, llvm::Attribute::ZExt))
        { error = "fixed physical argument attribute mismatch"; return std::nullopt; }
        parameter.structureReturn = declaration->hasParamAttribute(i, llvm::Attribute::StructRet);
        parameter.byValue = declaration->hasParamAttribute(i, llvm::Attribute::ByVal);
        parameter.signExtend = declaration->hasParamAttribute(i, llvm::Attribute::SExt);
        parameter.zeroExtend = declaration->hasParamAttribute(i, llvm::Attribute::ZExt);
        parameter.inRegister = declaration->hasParamAttribute(i, llvm::Attribute::InReg);
        parameter.noAlias = declaration->hasParamAttribute(i, llvm::Attribute::NoAlias);
        if (const auto alignment = declaration->getParamAlign(i)) parameter.alignment = alignment->value();
        const auto* pointee = parameter.structureReturn ? declaration->getParamStructRetType(i) :
            parameter.byValue ? declaration->getParamByValType(i) : nullptr;
        if (pointee)
        {
            parameter.pointee = copyType(pointee, 0);
            if (!parameter.pointee) { error = "fixed pointee type not represented"; return std::nullopt; }
        }
    }
    return result;
}
}
