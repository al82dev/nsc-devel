#include "nativesharp/Frontend/ProjectMethodBodyModel.h"

#include "nativesharp/Frontend/ProjectGenericModel.h"
#include "nativesharp/Frontend/ProjectGenericConstraintModel.h"

namespace nativesharp::frontend
{
// ---------------------------------------------------------------------------------------------
// C7 "definite assignment"
//
// The hole this closes: before it, `V v;` followed by a read of `v` compiled to an `alloca` with
// no store, and the read returned whatever was on the stack. No diagnostic, no zeroing, no
// warning -- for EVERY local type, not just aggregates. The fix has two halves that must be read
// together, because each one alone would be the wrong language:
//
//  1. The BACKEND gives every local without an initializer defined storage: it zeroes the slot at
//     the declaration point (see ProjectOopLlvmLowerer's LocalDeclaration case). NativeSharp
//     therefore has no uninitialized-read undefined behaviour left anywhere, at any optimization
//     level. In a program this analysis accepts every zeroing store is dead and SROA deletes it,
//     so the guarantee is free in exactly the programs that do not need it.
//
//  2. This analysis reports, as a hard error, the reads where the zero is CERTAINLY not what the
//     programmer meant -- a scalar, a pointer, a reference or a delegate read on a path where
//     nothing ever wrote it. That is the C# rule and it catches real bugs.
//
// The one deliberate difference from C#: an aggregate local (a struct value or a fixed `T[N]`
// array) is never reported. That is not a weakening, it is the language Alberto asked for --
// "struct sin metodos se comporta como en C". `Color c; c.tag = 1;` is a legitimate C idiom, and
// piecewise filling of an aggregate through loops and branches is precisely the shape a
// path-sensitive MUST-analysis cannot follow without rejecting correct programs. Half 1 is what
// makes that safe: the unwritten fields read as zero, deterministically, rather than as garbage.
bool ProjectMethodBodyModel::typeKeyHasDefinedDefaultStorage(std::string_view typeKey) const noexcept
{
    // A fixed-size array `T[N]` is storage, not a reference: `int[4] values;` reserves sixteen
    // bytes exactly like C does, and the backend zeroes them. A DYNAMIC array `T[]` is a heap
    // object behind a reference and is tracked like any other reference -- which is precisely
    // what makes `int[] a; a[0] = 1;` reportable.
    if (ProjectMemberModel::fixedArrayElementTypeKey(typeKey).has_value())
    {
        return true;
    }
    if (ProjectTypeConversion::isTuple(typeKey)) return true;
    if (!typeKey.starts_with("type:") || nameLookup_ == nullptr)
    {
        return false;
    }
    const std::string_view identity = typeKey.substr(5);
    for (const NamedTypeDeclaration& type : nameLookup_->namedTypes())
    {
        if (type.identity.key() == identity)
        {
            return type.kind == NamedTypeDeclarationKind::Struct;
        }
    }
    return false;
}

// NSC-LIMIT-004: la clave nombra un struct y nada mas. Misma busqueda que aggregateIsUnion,
// justo debajo, pero mirando la CLASE de la declaracion en vez de su texto auxiliar.
bool ProjectMethodBodyModel::typeKeyIsStruct(std::string_view typeKey) const noexcept
{
    if (ProjectTypeConversion::isTuple(typeKey)) return true;
    if (nameLookup_ == nullptr || !typeKey.starts_with("type:")) return false;
    const std::string_view identity = typeKey.substr(5);
    for (const NamedTypeDeclaration& type : nameLookup_->namedTypes())
    {
        if (type.identity.key() != identity) continue;
        return type.kind == NamedTypeDeclarationKind::Struct;
    }
    return false;
}

// C8: does this type key name a union? Reads the same layout marker the parser wrote onto the
// declaration and ProjectObjectLayoutModel reads back (see parseStructLayoutOptions there).

// SHR-013: una vista, o un arreglo de vistas de cualquier rango. `StringView[]` es lo que un
// lexer quiere de verdad -- una lista de lexemas sin copiar ni uno -- y precisamente por eso
// tiene que contar como vista alla donde una vista no puede ir.
bool ProjectMethodBodyModel::typeKeyIsViewLike(std::string_view typeKey) const noexcept
{
    std::string_view key = typeKey;
    while (key.ends_with("[]")) key = key.substr(0, key.size() - 2);
    return aggregateIsBorrowedView(key);
}

bool ProjectMethodBodyModel::typeKeyCarriesLifetime(std::string_view typeKey) const noexcept
{
    if (typeKey.empty() || typeKey == "<error>") return false;
    if (typeKey.ends_with("*")) return true;
    if (typeKey.ends_with("[]")) return true;
    if (typeKey == "builtin:string") return true;
    if (typeKeyIsViewLike(typeKey)) return true;
    return isReferenceTypeKey(typeKey);
}

bool ProjectMethodBodyModel::expressionUsesExpandedParameterPack(
    const ast::AstNode* expression,
    std::size_t callableOrdinal) const
{
    if (expression == nullptr) return false;
    std::vector<const ast::AstNode*> pending{expression};
    for (std::size_t cursor = 0; cursor < pending.size(); ++cursor)
    {
        const ast::AstNode* current = pending[cursor];
        const MethodCallBinding* methodCall = callBinding(current, callableOrdinal);
        if (methodCall != nullptr &&
            methodCall->resolution == MethodCallResolutionKind::Resolved &&
            methodCall->selectedUsesExpandedParameterPack)
        {
            return true;
        }
        const NamespaceFunctionCallBinding* functionCall = namespaceFunctionCall(current);
        if (functionCall != nullptr &&
            functionCall->resolution == MethodCallResolutionKind::Resolved &&
            functionCall->selectedUsesExpandedParameterPack)
        {
            return true;
        }

        if (current->first() != nullptr)
        {
            pending.push_back(current->first());
        }
        if (current->second() != nullptr)
        {
            pending.push_back(current->second());
        }
        if (current->third() != nullptr)
        {
            pending.push_back(current->third());
        }
        for (const ast::AstNode* child : current->children())
        {
            if (child != nullptr)
            {
                pending.push_back(child);
            }
        }
    }
    return false;
}

// SHR-013 "vistas y tiempo de vida" -- EL ANALISIS DE ENRAIZAMIENTO.
//
// La pregunta: esta expresion toma prestado de un local de ESTE metodo? Si la respuesta es que
// si, el valor no puede salir del metodo, porque ese local muere al volver.
//
// Como contesta: recorre el arbol de la expresion y se queda con el PRIMER identificador que
// nombre un local cuyo tipo lleve un tiempo de vida -- una cadena, un arreglo, un puntero, un
// duenno `raii`, u otra vista ya enraizada en un local. Un `int` no cuenta: un indice no es un
// prestamo. Un parametro, un campo y un literal tampoco cuentan, y ese es justo el punto: los
// tres sobreviven a la llamada, asi que una vista sacada de ellos puede salir.
//
// POR QUE UNA VISTA LOCAL SE CONSULTA Y NO SE ASUME. `StringView v = StringView.Of(texto);
// return v;` con `texto` de parametro es CORRECTO, y una regla que dijera "un local de tipo
// vista contamina" lo rechazaria. Por eso cada local de tipo vista lleva grabado de que se
// enraiza (`viewRootLocalName`), y aqui se le pregunta en vez de suponerlo.
//
// LO QUE NO HACE, dicho para que nadie lo lea como mas de lo que es: no sigue el puntero. Una
// vista que pasa por una llamada cuyo argumento es un local se considera enraizada en ese local
// aunque la llamada no lo use. Es conservador a proposito: de los dos errores posibles, rechazar
// un programa correcto se ve al compilar y se arregla; aceptar uno incorrecto es un
// uso-despues-de-liberar que no se ve nunca.
}
