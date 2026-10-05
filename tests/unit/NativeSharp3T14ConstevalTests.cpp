#include "nativesharp/Language/NumericSemantics.h"

#include "../support/TestContext.h"

int main()
{
    nativesharp::tests::TestContext tests;
    using namespace nativesharp::language;
    std::string error;

    IntegerArithmeticRequest arithmetic{IntegerArithmeticOperation::Add, 40, 2,
        {32, true}, OverflowPolicy::Checked, NumericEvaluationDomain::Runtime};
    const auto runtime = evaluateIntegerArithmetic(arithmetic, error);
    const auto constant = evaluateConstantIntegerArithmetic(arithmetic, error);
    tests.expect(runtime.has_value() && constant.has_value() &&
            runtime->bits == constant->bits && constant->bits == 42,
        "runtime and comptime use the same integer arithmetic semantics");

    arithmetic = {IntegerArithmeticOperation::Add, 127, 1, {8, true},
        OverflowPolicy::Unchecked, NumericEvaluationDomain::Runtime};
    tests.expect(evaluateIntegerArithmetic(arithmetic, error).has_value() &&
            !evaluateConstantIntegerArithmetic(arithmetic, error).has_value() &&
            error.find("undefined integer overflow") != std::string::npos,
        "undefined integer behavior is diagnosed during constant evaluation");

    ConstantMemory memory;
    const auto allocation = memory.allocate(16, 8, error);
    const auto field = allocation.has_value()
        ? memory.offset(*allocation, 8, error)
        : std::nullopt;
    tests.expect(field.has_value() && memory.validateDereference(*field, 8, error),
        "constant pointers retain allocation provenance through in-bounds arithmetic");
    tests.expect(allocation.has_value() && !memory.offset(*allocation, 17, error).has_value() &&
            error.find("escaped") != std::string::npos,
        "constant pointer arithmetic cannot escape its allocation");
    const ConstantPointer forged{999, 0};
    tests.expect(!memory.validateDereference(forged, 1, error) &&
            error.find("provenance") != std::string::npos,
        "forged constant pointers fail closed without provenance");
    tests.expect(allocation.has_value() && memory.deallocate(allocation->allocation, error) &&
            !memory.validateDereference(*allocation, 1, error),
        "constant memory rejects use after deterministic deallocation");

    ConstantUnionState taggedUnion{"", 8};
    tests.expect(activateConstantUnionMember(taggedUnion, "integer", 8, error) &&
            readConstantUnionMember(taggedUnion, "integer", error) &&
            !readConstantUnionMember(taggedUnion, "floating", error),
        "constant unions track the active member and diagnose inactive reads");

    const auto loop = executeConstantIntegerLoop(1, 2, 5, 5,
        IntegerArithmeticOperation::Multiply, {32, true}, OverflowPolicy::Checked, error);
    tests.expect(loop.has_value() && loop->bits == 32,
        "bounded constant loops produce stable values suitable for const generics");
    tests.expect(!executeConstantIntegerLoop(0, 1, 11, 10,
            IntegerArithmeticOperation::Add, {32, true}, OverflowPolicy::Checked, error).has_value() &&
            error.find("iteration budget") != std::string::npos,
        "constant loops obey a deterministic execution budget");

    tests.expect(floatingPointContract(FloatingPointMode::Strict).constevalMatchesRuntime &&
            !floatingPointContract(FloatingPointMode::Fast).constevalMatchesRuntime,
        "floating-point consteval parity follows the same explicit mode contract as runtime");
    return tests.result();
}
