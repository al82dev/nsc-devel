#include "nativesharp/MIR/MirArena.h"
#include "nativesharp/MIR/MirInstruction.h"
#include "nativesharp/MIR/MirOpcode.h"
#include "nativesharp/MIR/MirValue.h"
#include "nativesharp/Passes/ConstantFolder.h"
#include "nativesharp/Sema/ConstantValue.h"
#include "nativesharp/Sema/TypeContext.h"
#include "nativesharp/Sema/TypeKind.h"
#include "../support/TestContext.h"

#include <cstdint>
#include <limits>
#include <unordered_map>

int main()
{
    using nativesharp::mir::MirArena;
    using nativesharp::mir::MirOpcode;
    using nativesharp::passes::ConstantFolder;
    using nativesharp::sema::ConstantValue;
    using nativesharp::sema::TypeContext;
    using nativesharp::sema::TypeKind;
    using nativesharp::tests::TestContext;

    TestContext tests;
    TypeContext types;
    MirArena arena;

    auto* intType = types.builtin(TypeKind::Int);
    auto* uintType = types.builtin(TypeKind::UInt);
    auto* doubleType = types.builtin(TypeKind::Double);

    auto* left = arena.makeValue(intType);
    auto* right = arena.makeValue(intType);
    auto* result = arena.makeValue(intType);
    auto* add = arena.makeInstruction(MirOpcode::Add, {});
    add->setResult(result);
    add->addOperand(left);
    add->addOperand(right);

    std::unordered_map<const nativesharp::mir::MirValue*, ConstantValue> known;
    known.emplace(left, ConstantValue::signedInteger(std::numeric_limits<std::int32_t>::max()));
    known.emplace(right, ConstantValue::signedInteger(1));
    const auto wrapped = ConstantFolder::fold(add, known);
    tests.expect(wrapped.has_value() && wrapped->kind() == nativesharp::sema::ConstantKind::SignedInteger &&
            wrapped->signedInteger() == std::numeric_limits<std::int32_t>::min(),
        "signed int constant folding follows NativeSharp modulo-2^32 wrap semantics");

    auto* uleft = arena.makeValue(uintType);
    auto* uright = arena.makeValue(uintType);
    auto* uresult = arena.makeValue(uintType);
    auto* uadd = arena.makeInstruction(MirOpcode::Add, {});
    uadd->setResult(uresult);
    uadd->addOperand(uleft);
    uadd->addOperand(uright);
    known.clear();
    known.emplace(uleft, ConstantValue::unsignedInteger(std::numeric_limits<std::uint32_t>::max()));
    known.emplace(uright, ConstantValue::unsignedInteger(1));
    const auto unsignedWrapped = ConstantFolder::fold(uadd, known);
    tests.expect(unsignedWrapped.has_value() && unsignedWrapped->unsignedInteger() == 0,
        "unsigned int constant folding wraps modulo 2^32");

    auto* minValue = arena.makeValue(intType);
    auto* minusOne = arena.makeValue(intType);
    auto* divResult = arena.makeValue(intType);
    auto* divide = arena.makeInstruction(MirOpcode::Divide, {});
    divide->setResult(divResult);
    divide->addOperand(minValue);
    divide->addOperand(minusOne);
    known.clear();
    known.emplace(minValue, ConstantValue::signedInteger(std::numeric_limits<std::int32_t>::min()));
    known.emplace(minusOne, ConstantValue::signedInteger(-1));
    tests.expect(!ConstantFolder::fold(divide, known).has_value(),
        "constant folder refuses signed MIN/-1 because NativeSharp requires a runtime trap");

    auto* one = arena.makeValue(intType);
    auto* zero = arena.makeValue(intType);
    auto* zeroDivResult = arena.makeValue(intType);
    auto* divideZero = arena.makeInstruction(MirOpcode::Divide, {});
    divideZero->setResult(zeroDivResult);
    divideZero->addOperand(one);
    divideZero->addOperand(zero);
    known.clear();
    known.emplace(one, ConstantValue::signedInteger(1));
    known.emplace(zero, ConstantValue::signedInteger(0));
    tests.expect(!ConstantFolder::fold(divideZero, known).has_value(),
        "constant folder refuses integer divide-by-zero instead of erasing defined trap semantics");

    auto* shiftValue = arena.makeValue(intType);
    auto* shiftCount = arena.makeValue(intType);
    auto* shiftResult = arena.makeValue(intType);
    auto* shift = arena.makeInstruction(MirOpcode::ShiftLeft, {});
    shift->setResult(shiftResult);
    shift->addOperand(shiftValue);
    shift->addOperand(shiftCount);
    known.clear();
    known.emplace(shiftValue, ConstantValue::signedInteger(1));
    known.emplace(shiftCount, ConstantValue::signedInteger(32));
    tests.expect(!ConstantFolder::fold(shift, known).has_value(),
        "constant folder refuses invalid shift count that must trap");

    auto* floating = arena.makeValue(doubleType);
    auto* converted = arena.makeValue(intType);
    auto* convert = arena.makeInstruction(MirOpcode::Convert, {});
    convert->setResult(converted);
    convert->addOperand(floating);
    known.clear();
    known.emplace(floating, ConstantValue::floating(std::numeric_limits<double>::infinity()));
    tests.expect(!ConstantFolder::fold(convert, known).has_value(),
        "constant folder refuses non-finite floating-to-integer conversion that must trap");

    tests.expect(!ConstantFolder::equal(ConstantValue::floating(0.0), ConstantValue::floating(-0.0)),
        "floating constant equality preserves +0 and -0 bit-pattern distinction");

    return tests.result();
}
