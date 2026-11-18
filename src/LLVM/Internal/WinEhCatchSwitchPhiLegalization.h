#pragma once

#include <cstddef>

namespace nativesharp::llvm_backend
{
// EN: Works around a defect of LLVM's WinEHPrepare (llvm/lib/CodeGen/WinEHPrepare.cpp, LLVM 23.1).
//     A phi on a `catchswitch` block cannot get a reload in its own block, so WinEHPrepare reloads
//     it before each user, except a phi on another EH pad, which it leaves for that phi's own
//     demotion. That demotion then stores the catchswitch phi at the end of the incoming block and,
//     once every EH phi is demoted, replaces the catchswitch phi with poison: the store becomes
//     `store poison` and code generation drops it. The spill slot read by the cleanup funclet
//     keeps whatever was there before.
//     The shape appears at -O1 and above when a `catch` body leaves by an exception (rethrow, a new
//     throw or a throwing call) while a cleanup-owning local declared before the `try` is still
//     alive: SROA turns the local's owner slot into a phi on the catchswitch (null on the edge
//     where the try already destroyed it) and a phi on the catch body's cleanuppad that takes that
//     value from the catch body's blocks. The funclet then destroyed nothing (frame object) or
//     destroyed a stale pointer (heap object).
//     This rewrite runs right before code generation. For each catchswitch phi with such a use, it
//     stores the phi's incoming values in a stack slot at the end of the catchswitch's
//     predecessors (exactly where WinEHPrepare puts its own stores) and makes each affected EH-pad
//     phi take a load of that slot at the end of its incoming block. The catchswitch phi keeps
//     every other use, which WinEHPrepare demotes correctly. Returns how many phi entries were
//     rewritten; a module without the shape is left untouched.
// ES: Rodea un defecto de WinEHPrepare de LLVM (llvm/lib/CodeGen/WinEHPrepare.cpp, LLVM 23.1).
//     Una phi en un bloque `catchswitch` no puede recargarse en su propio bloque, asi que
//     WinEHPrepare la recarga antes de cada usuario, salvo una phi de otro bloque EH, que deja para
//     la degradacion de esa phi. Esa degradacion guarda la phi del catchswitch al final del bloque
//     de entrada y, cuando todas las phis EH estan degradadas, sustituye la phi del catchswitch por
//     poison: el guardado queda como `store poison` y la generacion de codigo lo elimina. La ranura
//     que lee el funclet de limpieza conserva lo que tuviera antes.
//     La forma aparece desde -O1 cuando el cuerpo de un `catch` sale por una excepcion (relanzar,
//     lanzar una nueva o una llamada que lanza) con vivo un local con limpieza declarado antes del
//     `try`: SROA convierte la ranura de dueno del local en una phi del catchswitch (null en la
//     arista donde el try ya lo destruyo) y en una phi del cleanuppad del cuerpo del catch que toma
//     ese valor desde los bloques del cuerpo. El funclet no destruia nada (objeto de marco) o
//     destruia un puntero viejo (objeto en heap).
//     Esta reescritura corre justo antes de generar codigo. Para cada phi de catchswitch con ese
//     uso, guarda los valores de entrada de la phi en una ranura de pila al final de los
//     predecesores del catchswitch (justo donde WinEHPrepare pone los suyos) y hace que cada phi EH
//     afectada tome una carga de esa ranura al final de su bloque de entrada. La phi del catchswitch
//     conserva los demas usos, que WinEHPrepare degrada bien. Devuelve cuantas entradas de phi se
//     reescribieron; un modulo sin la forma queda intacto.
//
// `module` is an LLVMModuleRef. / `module` es un LLVMModuleRef.
std::size_t legalizeWinEhCatchSwitchPhiUses(void* module) noexcept;
}
