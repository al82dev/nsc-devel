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
    // Read-only restriction follows the object when a class reference is borrowed.
    bool readonlyBorrowedObject = false;
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
    //
    // EN: LKF-166: superseded by Freeze C-01 (e64631180): a `new` handed to a parameter
    //     without `owned` is rejected with LostOwnership, because nothing would destroy it; this
    //     flag still lets an `owned` parameter take it. The paragraph above is kept as history.
    // ES: LKF-166: sustituido por Congelamiento C-01 (e64631180): un `new` entregado a un
    //     parametro sin `owned` se rechaza con LostOwnership, porque nada lo destruiria; esta
    //     bandera sigue dejando que un parametro `owned` lo reciba. El parrafo anterior se conserva
    //     como historia.
    bool ownershipFreshOwner = false;
    // The previous owner extracted by a supported postfix update is an unnamed
    // full-expression temporary, not an explicit move into the next parameter.
    bool ownedPostfixTemporary = false;
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
    // EN: Freeze (plan item 131a): the value was read from a field of the temporary an owned-returning call
    //     made (`Make().Name`), directly or through more field reads. The temporary is destroyed at the end of
    //     the full expression, so the value cannot be stored or returned.
    // ES: Congelamiento (elemento 131a del plan): el valor se leyo de un campo del temporal que creo una
    //     llamada con retorno owned (`Make().Name`), directo o a traves de mas lecturas de campo. El temporal se
    //     destruye al final de la expresion completa, asi que el valor no puede guardarse ni devolverse.
    // EN: Freeze (plan item 135): the same flag covers every known-life temporary and every form the borrow
    //     goes through: a `new` object (`(new T()).Name`), a getter, a call result that is not a new owner
    //     (`Make().GetName()`, `Id(Make().Name)`), and the union of the branches of `?:` and `??`.
    // ES: Congelamiento (elemento 135 del plan): la misma bandera cubre todo temporal de vida conocida y toda
    //     forma por la que pasa el prestamo: un objeto `new` (`(new T()).Name`), un getter, un resultado de
    //     llamada que no es un owner nuevo (`Make().GetName()`, `Id(Make().Name)`), y la union de las ramas de
    //     `?:` y `??`.
    bool borrowsOwnedTemporary = false;
    // EN: Freeze (plan item 135): a method group whose receiver is an owned temporary or a borrow of one
    //     (`Make().GetName`); the call result inherits the borrow (markCallTemporaryBorrow).
    // ES: Congelamiento (elemento 135 del plan): un grupo de metodos cuyo receptor es un temporal owned o un
    //     prestamo de uno (`Make().GetName`); el resultado de la llamada hereda el prestamo
    //     (markCallTemporaryBorrow).
    bool receiverBorrowsOwnedTemporary = false;
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
    // Qualification of this storage, not of any object reached through a pointer value.
    bool volatileStorage = false;
    // The immediate pointee is volatile; independent of the pointer's storage.
    bool pointerPointeeVolatile = false;
    bool cppVolatilePointeeContract = false;
    // EN: T08-D13. With the C contract: the volatile mode of the C origin (true Microsoft, false ISO, none unknown
    //     or mixed) and the alignment the contract proves or C assumes for the pointee (0 when unknown).
    // ES: T08-D13. Con el contrato C: el modo volatile del origen C (true Microsoft, false ISO, ninguno si se
    //     desconoce o se mezcla) y la alineacion que el contrato demuestra o C supone para el apuntado (0 si no).
    std::optional<bool> cppVolatilePointeeMicrosoft;
    // EN: T08-D11/D12. The value is read directly from C: a C/C++ global or enumerator, a C record field, the
    //     result of a C function, or an element or pointee reached from one of them. It only drives the
    //     diagnostic of integer operations whose NativeSharp result differs from C; it never changes their type.
    // ES: T08-D11/D12. El valor se lee directamente de C: una global o enumerador C/C++, un campo de record C, el
    //     resultado de una funcion C, o un elemento o dato apuntado alcanzado desde ellos. Solo activa el
    //     diagnostico de las operaciones enteras cuyo resultado NativeSharp difiere del de C; nunca cambia su tipo.
    bool cppCOrigin = false;
    std::size_t cppVolatilePointeeAlignment = 0;
    // T08-D07: this expression denotes C `_Atomic` record storage, accessed only as a whole value.
    bool cppAtomicRecordStorage = false;
    // EN: T19-002. This expression denotes C `_Atomic` storage (an atomic C global, or the pointee of a pointer to
    //     one), whose address is a pointer to atomic storage.
    // ES: T19-002. Esta expresion denota almacenamiento C `_Atomic` (una global C atomica, o el apuntado de un
    //     puntero a una); su direccion es un puntero a almacenamiento atomico.
    bool cppAtomicStorage = false;
    // EN: T19-002. The immediate pointee is C `_Atomic` storage (`&a` of an atomic C global, a C `_Atomic T*`): every
    //     access through the pointer is a sequentially consistent atomic access, as in C. Not user syntax.
    // ES: T19-002. El dato apuntado inmediato es almacenamiento C `_Atomic` (`&a` de una global C atomica, un
    //     `_Atomic T*` de C): cada acceso por el puntero es atomico y secuencialmente consistente, como en C. No es
    //     sintaxis de usuario.
    bool cppPointerPointeeAtomic = false;
    // EN: T19-038. With `cppPointerPointeeAtomic`, the atomic storage pointed to holds a pointer to volatile data
    //     (`&a` of an `_Atomic(volatile T*)` C global): `*p` is a `T volatile*`. A `const` base type follows
    //     T08-D17 (`pointerPointeeReadonly` of a pointer to pointer). Not user syntax.
    // ES: T19-038. Con `cppPointerPointeeAtomic`, el almacenamiento atomico apuntado guarda un puntero a dato
    //     volatile (`&a` de una global C `_Atomic(volatile T*)`): `*p` es un `T volatile*`. Un tipo base `const`
    //     sigue T08-D17 (`pointerPointeeReadonly` de un puntero a puntero). No es sintaxis de usuario.
    bool cppAtomicPointeeHoldsVolatile = false;
    // EN: T19-038. This expression names a non-atomic `volatile` C global (scalar or record): `&` gives a pointer to
    //     volatile data, as in C. The storage stays non-addressable for `ref`, `in` and `out`.
    // ES: T19-038. Esta expresion nombra una global C `volatile` no atomica (escalar o record): `&` da un puntero a
    //     dato volatile, como en C. El almacenamiento sigue sin ser direccionable para `ref`, `in` y `out`.
    bool cppVolatileGlobalAddress = false;
    // This expression denotes an imported C-family flexible array member, not a native array.
    bool cppFlexibleArrayMember = false;
    bool cppFlexibleArrayDecaySafe = false;
};
