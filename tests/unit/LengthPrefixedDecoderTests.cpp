#include "nativesharp/Frontend/ProjectTypeConversion.h"
#include "nativesharp/Language/AbiSemantics.h"
#include "nativesharp/Language/CallableSemantics.h"
#include "nativesharp/Language/OwnershipSemantics.h"

#include "../support/TestContext.h"

#include <string>

// EN: LKF-399, LKF-412, LKF-413, LKF-423. Length-prefixed readers compared `cursor + length` with the
//     input size, and the SIMD contract compared `laneBits * laneCount` with 512. A length or a lane
//     count near SIZE_MAX wrapped the sum or the product, so the check passed: the reader then took
//     the rest of the input as one field or indexed past the end, and the SIMD contract accepted a
//     vector of 2^61 lanes. Every case below is rejected now, and each has a well-formed control.
// ES: LKF-399, LKF-412, LKF-413, LKF-423. Los lectores con prefijo de longitud comparaban
//     `cursor + length` con el tamaño de la entrada, y el contrato SIMD comparaba `laneBits * laneCount`
//     con 512. Una longitud o una cantidad de lanes cercana a SIZE_MAX desbordaba la suma o el producto
//     y la comprobación pasaba: el lector tomaba el resto de la entrada como un campo o indexaba fuera
//     del final, y el contrato SIMD aceptaba un vector de 2^61 lanes. Cada caso se rechaza ahora, y
//     cada uno tiene un control bien formado.
int main()
{
    nativesharp::tests::TestContext tests;
    using namespace nativesharp;
    const std::string huge = "18446744073709551615";

    // LKF-399
    const auto tuple = frontend::ProjectTypeConversion::tupleElementTypeKeys("tuple:(11:builtin:int,12:builtin:long)");
    tests.expect(tuple.has_value() && tuple->size() == 2 && (*tuple)[1] == "builtin:long",
        "a well-formed tuple key yields its element keys");
    tests.expect(!frontend::ProjectTypeConversion::tupleElementTypeKeys("tuple:(" + huge + ":builtin:int)").has_value(),
        "a tuple element length that wraps the cursor is rejected");
    // EN: 2^64 - 22 moves the wrapped cursor back onto the first comma, so the old reader parsed the
    //     second element again forever.
    // ES: 2^64 - 22 lleva el cursor desbordado de vuelta a la primera coma, así que el lector anterior
    //     volvía a leer el segundo elemento sin fin.
    tests.expect(!frontend::ProjectTypeConversion::tupleElementTypeKeys("tuple:(1:a,18446744073709551594:b)").has_value(),
        "a tuple element length that wraps back onto an earlier comma is rejected");

    // LKF-413
    language::FunctionType function;
    function.parameters = {"builtin:int"};
    function.result = "builtin:int";
    const std::string identity = language::functionTypeIdentity(function);
    tests.expect(language::parseFunctionTypeIdentity(identity).has_value(),
        "a well-formed function type identity round-trips");
    std::string corrupt = identity;
    const std::size_t parameter = corrupt.find("11#builtin:int");
    if (parameter != std::string::npos) corrupt.replace(parameter, 2, huge);
    tests.expect(parameter != std::string::npos && !language::parseFunctionTypeIdentity(corrupt).has_value(),
        "a function type field length that wraps the cursor is rejected");

    // LKF-423
    language::EscapeSummary summary;
    std::string error;
    tests.expect(language::decodeEscapeSummary("escape-v1|3:abc|0,", summary, error) &&
            summary.functionIdentity == "abc" && summary.parameters.size() == 1,
        "a well-formed escape summary decodes");
    tests.expect(!language::decodeEscapeSummary("escape-v1|" + huge + ":abc|0,", summary, error) &&
            error == "corrupt escape summary length",
        "an escape summary name length that wraps is rejected");

    // LKF-412
    tests.expect(language::validateSimd(language::SimdContract{32, 16, false}, error),
        "a 512-bit SIMD contract is accepted");
    tests.expect(!language::validateSimd(language::SimdContract{8, std::size_t{1} << 61, false}, error),
        "a SIMD lane count whose total width wraps is rejected");
    tests.expect(!language::parsePortableSimdType("simd<builtin:byte,2305843009213693952>").has_value(),
        "a parsed SIMD identity with a wrapping lane count is rejected");
    return tests.result();
}
