#pragma once

namespace nativesharp::language
{
// EN: ForeignImport: the address reaches only imported C functions, which call sites already trust the
//     same way (docs/fixes/contrato-escape-importacion-c.md); appended so stored values keep their numbers.
// ES: ForeignImport: la dirección sólo llega a funciones C importadas, en las que los sitios de llamada ya
//     confían igual; se agrega al final para que los valores guardados conserven su número.
enum class EscapeClass
{
    NoEscape, Return, CallerOwned, Global, Unknown, RefSlotNoEscape, ObjectPointerGlobal, ForeignImport
};
}
