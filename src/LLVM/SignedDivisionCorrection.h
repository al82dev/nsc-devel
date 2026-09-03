#pragma once

#include "LlvmCApi.h"

#include <cstdint>
#include <string>
#include <string_view>

namespace nativesharp::llvm_backend
{
// La API C de LLVM que este proyecto declara a mano vive en `nativesharp::llvm_api`; los dos
// bajadores la abren con un `using namespace` DESPUES de incluir esta cabecera, asi que aqui hay
// que abrirla otra vez -- dentro de este namespace, no a nivel global.
using namespace llvm_api;

// LA CORRECCION DE `MinValue / -1`, ESCRITA UNA SOLA VEZ.
//
// POR QUE EXISTE ESTA CABECERA, y es la correccion del 2026-09-03. El proyecto tiene DOS
// bajadores a LLVM y los dos emiten division con signo:
//
//   * src/LLVM/ProjectOopLlvmLowerer.cpp -- el de proyectos, por el que pasan las fixtures.
//   * src/LLVM/LlvmLowerer.cpp           -- el de fichero unico, vivo todavia: lo usan
//                                           --program, --check y --dump-llvm desde Driver.cpp.
//
// Y NO HACIAN LO MISMO. El viejo emitia una guarda que ABORTABA con
// LlvmTrapReason::SignedDivisionOverflow; el nuevo emitia un `sdiv` desnudo. El plegador de
// constantes hacia una tercera cosa: devolver nullopt, o sea rechazar el programa.
// Tres respuestas distintas a la misma pregunta -- exactamente el defecto que el doc 100
// describe. Ahora la regla esta aqui y en ningun otro sitio.
//
// QUE MIDE LA FIXTURE c30, que es lo que descubrio esto:
//
//     -O0  ->  exit 3221225621 == 0xC0000095 == STATUS_INTEGER_OVERFLOW
//     -O2  ->  exit 19, es decir NO aborta y devuelve otra cosa
//
// LOS DOS SINTOMAS SON EL MISMO BUG y explican por que la correccion tiene que estar EN EL IR y
// no puede dejarse al hardware:
//
//   * En -O0 el `idiv` de x86-64 lanza una excepcion de hardware en este caso exacto.
//   * En -O2 no llega a ejecutarse ningun `idiv`: en LLVM, `sdiv` con MinValue y -1 es POISON,
//     asi que el optimizador puede plegarlo a lo que quiera. Y lo hace.
//
// O sea que "el hardware ya envuelve" nunca habria sido una respuesta: el IR es UB antes de que
// el hardware vea nada.
//
// COMO SE CORRIGE, SIN RAMA. Si el caso malo se da, se divide por 1 en vez de por -1:
//
//     MinValue / 1  ==  MinValue     <- el resultado envuelto, exacto
//     MinValue % 1  ==  0            <- el resto correcto, exacto
//
// Dos comparaciones, un `and` y un `select`. Sin salto, sin bloque nuevo, y `sdiv` deja de ser
// poison porque el caso que lo hacia poison ya no le llega.
//
// EL CERO NO SE TOCA. Dividir por cero sigue siendo un fallo del programa (doc 110 §2.5) y lo
// atrapa la guarda que cada bajador emite ANTES de llamar aqui.
//
// POR QUE ENVOLVER Y NO ABORTAR, que es apartarse de lo que hacia el bajador viejo. El doc 110
// §2.1 dice que el desbordamiento con signo esta DEFINIDO y envuelve; `MinValue / -1` es un
// desbordamiento con signo igual que `MaxValue + 1`, y hacer que uno aborte y el otro envuelva
// es un caso especial que hay que recordar. Java hace exactamente esto (Integer.MIN_VALUE / -1
// da Integer.MIN_VALUE), C# lanza y C tiene UB. Envolver es lo unico que cumple el criterio del
// proyecto de que -O0 y -O2 se comporten igual sin pagar un salto por division.
//
// Devuelve el divisor QUE HAY QUE PASARLE a sdiv/srem. `width` es el ancho en bits del tipo del
// operando; `integerType` su tipo LLVM.
inline LLVMValueRef correctedSignedDivisor(
    LLVMBuilderRef builder,
    LLVMTypeRef integerType,
    LLVMValueRef dividend,
    LLVMValueRef divisor,
    unsigned width,
    std::string_view stem)
{
    if (builder == nullptr || integerType == nullptr || dividend == nullptr || divisor == nullptr)
    {
        return divisor;
    }

    const std::uint64_t minimumBits = width >= 64U
        ? (std::uint64_t{1} << 63)
        : (std::uint64_t{1} << (width - 1U));
    const std::uint64_t minusOneBits = width >= 64U
        ? ~std::uint64_t{0}
        : ((std::uint64_t{1} << width) - 1U);

    LLVMValueRef minimum = LLVMConstInt(integerType, minimumBits, 0);
    LLVMValueRef minusOne = LLVMConstInt(integerType, minusOneBits, 0);
    LLVMValueRef one = LLVMConstInt(integerType, 1, 0);

    const std::string base(stem);
    LLVMValueRef isMinimum =
        LLVMBuildICmp(builder, LLVMIntEQ, dividend, minimum, (base + ".is_min").c_str());
    LLVMValueRef isMinusOne =
        LLVMBuildICmp(builder, LLVMIntEQ, divisor, minusOne, (base + ".is_minus_one").c_str());
    LLVMValueRef overflow =
        LLVMBuildAnd(builder, isMinimum, isMinusOne, (base + ".overflow").c_str());
    return LLVMBuildSelect(builder, overflow, one, divisor, (base + ".divisor").c_str());
}
}
