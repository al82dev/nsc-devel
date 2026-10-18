# H02S16B — Deconstruction declarations iterativas

## SCOPE LOCK

- Revisión base: `bc00fb92a13528be1f7589270ba3b70985a7a510`.
- Alcance único: retirar la reentrada C++ de `DeconstructionDeclarationStatement` hacia su source/receiver e integrarla al worklist común creado por H02S16A.
- Rutas cerradas: entrada statement ordinaria, `LoopExpression`/`LoopBlock` y declaraciones de arms de `MatchExpression`.
- Contratos congelados: evaluación única del source; source antes de layout; forma `Block(LocalDeclaration..., TupleDeconstruct)`; preorder de hojas; offsets absolutos; discard sin nodo; metadata de conversión; receiver class/struct/temporal/implícito; diagnósticos, rangos y rollback.
- Protegidos: sintaxis, semántica, parser, MIR, LLVM, ABI, NSMETA, ownership, borrow, cleanup y las hojas H02S16D en adelante.
- Validación autorizada: compilar sólo `nsc_project_oop_lowering_tests` y ejecutar sólo `hir-deconstruction-declaration-depth`.
- Prohibido: CTest, suite, minibaseline, baseline, pruebas vecinas y tracking global.

## Causa

La rama histórica de `lowerStatement` llamaba `lowerDeconstructionSource`, que a su vez llamaba `lowerExpression` o `lowerReceiver`. Un source que contenía un `LoopExpression` o un `MatchExpression` podía regresar a otra deconstruction declaration y formar el SCC recursivo statement→expression→statement. El walker de patrones `visitHirTuplePatternLeaves` ya era iterativo y no era la causa.

## Diseño implementado

`HirExpressionMachine.cpp` incorpora `IterativeExpressionKind::DeconstructionDeclaration` y un estado estrecho que conserva el binding, la fase del source, el resultado bajado y la necesidad de materializar un struct temporal.

La preparación conserva el orden histórico:

1. valida disponibilidad de layout/name lookup;
2. recupera el binding semántico;
3. programa el source tuple como valor;
4. programa un receiver class como valor y un struct addressable como dirección;
5. programa un struct temporal como valor y lo envuelve posteriormente en `TemporaryAddress`;
6. sintetiza receiver implícito/base sin reentrar a otro lowerer.

Al volver el child, el frame forma `InstanceCall` cuando existe `selectedMemberOrdinal`. Sólo entonces el finalizador consulta shape/layout, reutiliza `visitHirTuplePatternLeaves`, crea locales y `TupleDeconstructElement`, omite `_` y publica el bloque completo. El finalizador no llama a `lowerExpression`, `lowerReceiver`, `lowerStatement` ni `lowerBlock`.

`HirStatementMachine.cpp` conserva únicamente una delegación al entry point iterativo. `LoopBlock` y `Match` reconocen la declaración antes de su fallback recursivo y devuelven el resultado al contenedor mediante la misma pila explícita.

## Contratos preservados

- El source ocupa exactamente `TupleDeconstruct.children[0]` y aparece una sola vez.
- Los `LocalDeclaration` nombrados preceden al `TupleDeconstruct`.
- `_` no crea local, address, element, store ni cleanup.
- El walker mantiene preorder izquierda→derecha y acumula offsets absolutos.
- Cada element conserva `typeKey`, `elementTypeKey`, `integerValue` y `classifyImplicit` históricos.
- No se crean `BorrowValue`; ownership y cleanup de las hojas permanecen neutros.
- Class receiver produce valor; struct lvalue produce dirección; struct temporal produce `TemporaryAddress`; base/implícito produce `ThisValue`.
- Los errores de layout/binding/source/shape/nested/local conservan texto, precedencia y rango aplicable.
- Un error posterior destruye el árbol provisional sin publicarlo en el padre.

## Prueba focal registrada

Selector: `hir-deconstruction-declaration-depth`.

El selector contiene:

- contrafactual de 32,768 niveles que atraviesa deconstruction declaration→loop→block→deconstruction declaration;
- satélite de declaraciones de arm de `MatchExpression`;
- tuple nativo, patrón anidado, offsets absolutos, preorder, discard y `NumericWidening`;
- evidencia estructural de una sola aparición/evaluación del source;
- ausencia explícita de borrow, ownership y cleanup;
- receivers class, struct lvalue, struct temporal e implícito/base;
- precedencia y textos de errores pre-source y post-source;
- rango interior del fallo de source y ausencia de publicación parcial.

El HIR profundo se destruye con `destroyHirTreeIteratively`.

## Evidencia de validación

Compilación exacta autorizada:

```text
cmake --build A:\NSC-devel-paralelo\builds\agente-1-s14a --config Release --target nsc_project_oop_lowering_tests
resultado: PASS, exit 0
log: A:\NSC-devel-paralelo\logs\agente-1\h02s16b-build-final.log
SHA256: 54C256EEFAC8330659FD3437605AE3ACDF5D889C2CFB10C4EE72A7EE4650CD11
```

Selector único autorizado:

```text
NSC_PROJECT_OOP_LOWERING_FOCUS=hir-deconstruction-declaration-depth
A:\NSC-devel-paralelo\builds\agente-1-s14a\tests\nsc_project_oop_lowering_tests.exe
resultado: PASS: 17 checks, exit 0
log: A:\NSC-devel-paralelo\logs\agente-1\h02s16b-focus-final.log
SHA256: B0AC284C772556078B24A1376FEEE4CA4C171C6EB401CD44A4946F6BFB0F05A8
```

El primer intento de compilación no constituye evidencia: el fixture usaba el helper local `range` de S16A fuera de su ámbito. Se añadió el constructor de rango propio de S16B y se repitió únicamente el mismo target, que terminó correctamente. No se ejecutó ninguna prueba durante el intento fallido.

Auditoría estática final:

- la rama `DeconstructionDeclarationStatement` de `lowerStatement` sólo delega al entry point iterativo;
- root, `LoopBlock` y `Match` discriminan la declaración antes de sus fallbacks;
- el frame agenda el source como child y su finalizador no llama a lowerers;
- la única llamada residual a `lowerDeconstructionSource` pertenece al fallback histórico de deconstruction assignment, fuera de S16B;
- una revisión independiente declaró PASS estático del producto, dispatcher y fixture antes de la validación.

## Límites

Esta hoja no demuestra paridad de otras statements ni autoriza afirmar éxito de suite o baseline. La batería completa queda reservada para la GUI del usuario al terminar la tanda.
