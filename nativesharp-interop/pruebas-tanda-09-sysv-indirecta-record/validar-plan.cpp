#include "nativesharp/Interop/CppVariadicAbiValidation.h"

using namespace nativesharp::interop;
using namespace nativesharp::interop::abi_plan;

int main()
{
    Type i32{TypeKind::Integer, 32};
    Type i64{TypeKind::Integer, 64};
    Type pair{TypeKind::Structure};
    pair.elements = {i64, i32};
    Plan plan;
    plan.symbol = "__nsc_pointer_abi_probe";
    plan.targetTriple = "x86_64-pc-windows-msvc";
    plan.dataLayout = "e-m:w";
    plan.callingConvention = 78;
    plan.physicalReturn = i32;
    plan.logicalReturn.passing = Passing::Direct;
    plan.logicalReturn.coercion = i32;
    LogicalValue record;
    record.passing = Passing::Direct;
    record.coercion = pair;
    record.flatten = true;
    record.physicalCount = 2;
    plan.logicalParameters = {record};
    PhysicalParameter first, second;
    first.type = i64;
    second.type = i32;
    plan.physicalParameters = {first, second};
    if (!validateCppSysVFunctionPointerPlan(plan)) return 1;
    auto invalid = plan;
    invalid.logicalParameters[0].coercion->elements[1] = i64;
    if (validateCppSysVFunctionPointerPlan(invalid)) return 2;
    invalid = plan;
    invalid.logicalParameters[0].firstPhysical = 1;
    if (validateCppSysVFunctionPointerPlan(invalid)) return 3;
    invalid = plan;
    invalid.physicalParameters[0].byValue = true;
    if (validateCppSysVFunctionPointerPlan(invalid)) return 4;
    invalid = plan;
    invalid.logicalReturn.coercion = i64;
    if (validateCppSysVFunctionPointerPlan(invalid)) return 5;
    invalid = plan;
    invalid.variadic = true;
    if (validateCppSysVFunctionPointerPlan(invalid)) return 6;
    invalid = plan;
    invalid.logicalParameters.resize(1025);
    if (validateCppSysVFunctionPointerPlan(invalid)) return 7;
    return 0;
}
