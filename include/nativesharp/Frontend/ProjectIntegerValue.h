#pragma once

#include <cstdint>
#include <string>

namespace nativesharp::frontend
{
// CC1-102 "un unico evaluador integral tipado".
//
// POR QUE EXISTE. Antes de esto el compilador tenia DOS evaluadores de constantes enteras: el de
// Stage 37, completo y tipado, encerrado dentro de `isCompileTimeConstantExpression` que solo
// devuelve `bool`; y el que `switch` tuvo que escribir para plegar sus etiquetas, que operaba todo
// sobre `std::int64_t` sin consultar el tipo semantico. El segundo produjo cuatro defectos P1 de
// una sola vez (C-001..C-004), y no por descuido puntual: un evaluador que no conoce el tipo del
// valor que evalua NO PUEDE dar la respuesta correcta para division sin signo, para shift
// aritmetico vs logico, ni para un cast que estrecha.
//
// EL INVARIANTE. Un valor entero constante es SIEMPRE el par (bits, tipo). Nunca `int64_t` suelto.
// Toda operacion normaliza su resultado al tipo del resultado antes de devolverlo, de modo que los
// bits que salen de aqui son exactamente los que el backend materializara. Eso es lo que permite
// comparar dos etiquetas de `switch` con la certeza de que LLVM no las va a volver a colisionar.
struct ProjectIntegerValue final
{
    // Patron de bits YA normalizado a `typeKey`: truncado a su ancho, y con el signo extendido a 64
    // bits si el tipo tiene signo. Guardarlo asi -- y no como `int64_t` -- es lo que hace que la
    // comparacion de igualdad entre dos constantes sea la misma comparacion que hara el hardware.
    std::uint64_t bits = 0;
    std::string typeKey;

    std::int64_t asSigned() const noexcept;
};
}
