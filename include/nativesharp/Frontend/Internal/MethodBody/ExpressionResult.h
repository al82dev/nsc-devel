#pragma once

struct ExpressionResult final
{
    std::string typeKey = "<error>";
    bool valid = false;
    bool methodGroup = false;
    // T08-F05. Set only by move(receiver.Method). The method group remains target-typed, but
    // its delegate cell becomes the unique owner of the receiver and must clear a named source.
    bool methodGroupMovesReceiver = false;
    bool boundMethodBorrowsReceiver = false;
    bool assignable = false;
    // True only for storage-backed expressions (locals, parameters, fields, raw elements,
    // and dereferences). Properties may be assignable through a setter but are not memory
    // locations and therefore cannot satisfy an inline-assembly `m` constraint.
    bool addressable = false;
    // True when the value is a struct location reached through `in`, a readonly field, or
    // the implicit receiver of a readonly struct member. Reads remain legal; writes and calls
    // to mutating instance members do not.
    bool readonlyReceiver = false;
    // Source qualifier for raw pointers. Kept outside typeKey because LLVM pointers are
    // opaque and the qualifier has no ABI/layout effect; semantic assignment/dereference
    // still observes it.
    bool pointerPointeeReadonly = false;
    bool pointerNullable = true;
    bool pointerHasProvenance = true;
    std::size_t pointerKnownAlignment = 1;
    unsigned pointerAddressSpace = 0;
    bool onePastEndAddress = false;
    bool onePastEndPointer = false;
    std::vector<std::size_t> methodCandidates;
    std::optional<ProjectIntegerConstant> integerConstant;
    std::optional<language::CallableTrait> callableTrait;
    sema::OwnershipKind ownership = sema::OwnershipKind::None;
    bool ownershipTransfer = false;
    // NSC-LIMIT-010. ESTA EXPRESION ACABA DE CREAR UN OWNER que no pertenece a nadie todavia:
    // `new T(...)`. No es lo mismo que una transferencia -- no hay fuente con nombre de la que
    // salga, y por eso `move` no se puede ni escribir sobre ella.
    //
    // Es una tercera situacion y no un caso de las otras dos, y hace falta porque un parametro
    // `owned` exigia «ownership == Owned && ownershipTransfer», lo que dejaba fuera
    // `new Caja(new Carga(7))` -- no habia de donde mover.
    //
    // SOLO RELAJA, NUNCA ENDURECE. Este comentario llego a afirmar que un parametro SIN
    // calificador rechazaba un owner recien creado; era falso y ademas seria un cambio de
    // lenguaje, no una comprobacion: `Insert(new Student(...))` sobre un parametro sin
    // calificador es la forma mas ordinaria que hay --cinco ejemplos del libro la usan-- y
    // `Student s = new Student(...); Insert(s);` hace lo mismo con el mismo objeto. Un
    // parametro sin calificador no rastrea la vida del valor, y eso vale igual para uno
    // recien creado que para uno que ya existia. Ver classifyOwnershipArgument.
    bool ownershipFreshOwner = false;
    std::optional<std::size_t> ownershipSourceLocalOrdinal;
    std::optional<std::size_t> ownershipSourceParameterOrdinal;
    bool ownsStorageDestination = false;
    sema::OwnershipKind objectLifetimeOwnership = sema::OwnershipKind::None;
    sema::OwnershipKind storageOwnership = sema::OwnershipKind::None;
    // ES: Array cuyo inicializador guardó literales de cadena: esos slots no le pertenecen (los
    // literales duran todo el proceso), así que `delete[]` libera sólo el bloque.
    // EN: Array whose initializer stored string literals: it does not own those slots (literals
    // last for the whole process), so `delete[]` releases the block only.
    bool borrowedElementSlots = false;
    std::optional<std::size_t> borrowedLocalOrdinal;
    // EN: Freeze 3.0 (plan item 131a): the value was read from a field of the temporary an owned-returning call
    //     made (`Make().Name`), directly or through more field reads. The temporary is destroyed at the end of
    //     the full expression, so the value cannot be stored or returned.
    // ES: Congelamiento 3.0 (elemento 131a del plan): el valor se leyo de un campo del temporal que creo una
    //     llamada con retorno owned (`Make().Name`), directo o a traves de mas lecturas de campo. El temporal se
    //     destruye al final de la expresion completa, asi que el valor no puede guardarse ni devolverse.
    bool borrowsOwnedTemporary = false;
    const ast::AstNode* receiverSyntax = nullptr;
    std::string receiverTypeKey;
    std::optional<std::size_t> receiverTypeOrdinal;
    bool baseReceiver = false;
    bool extensionMethodGroup = false;
    std::optional<std::size_t> eventMemberOrdinal;
    std::optional<std::size_t> assignmentMemberOrdinal;
    // C4 "language references": the `ref`/`out`/`in` keyword written at THIS argument
    // position, if any. Value for every ordinary argument. It is checked against the
    // parameter's own pass kind after overload resolution, so the call site and the signature
    // must agree -- which is the point of requiring the keyword at all: a reader of the call
    // can see that the argument may change without going to read the signature.
    ParameterPassKind argumentPassKind = ParameterPassKind::Value;
    bool flowProvenNonNull = false;
    bool flowIntegerExcludesZero = false;
    std::optional<std::int64_t> flowIntegerMinimum;
    std::optional<std::int64_t> flowIntegerMaximum;
    std::optional<std::size_t> flowActiveVariantCase;
    bool flowRuntimeCheckElided = false;
};
