# T08-006: orden MS de campos volátiles de records C — diagnóstico y parche para integrar

## Estado

PARCHE PREPARADO Y PROBADO SOBRE `fd8ed5c1a`, NO APLICADO EN NINGUNA RAMA. Toca
`OopLlvmExpressionMachine.cpp` y `OopLlvmArraysAndStores.cpp`, compartidos con Codex, así que se entrega como
[t08-volatile-campos-c-orden-ms.patch](t08-volatile-campos-c-orden-ms.patch) para que él lo integre. No toca
NSMETA, NSLink, parser, MIR, HIR ni pruebas existentes. Control:
[t08-volatile-campos-c-control.md](t08-volatile-campos-c-control.md).

## Causa (HECHO COMPROBADO por lectura de código, árbol `fd8ed5c1a`)

1. La sesión C informa el modo volátil de Clang (`ClangSession::microsoftVolatileSemantics`). La proyección de
   records lo copia a cada campo (`CppSemanticRecord.cpp`, `imported.microsoftVolatileSemantics`) y el miembro lo
   conserva (`ProjectMemberModel.cpp:459`, `cppMicrosoftVolatileSemantics`); HIR y MIR lo llevan al nodo del campo.
2. El catálogo C de NSMETA ya persiste ese modo por record (`CppCRecordMetadataCodec.cpp:331` y `458`) y la
   reconstrucción precompilada lo copia a cada campo (`CppSemanticRecord.cpp`, `projected.microsoftVolatileSemantics`).
   Por eso el camino precompilado tiene la misma información y no hace falta cambiar NSMETA.
3. En el lowering, un campo volátil sólo recibe orden `acquire`/`release` si es puntero a función
   (`clangMsVolatileFunctionFieldAlignment`, `PackedArrayAlignment.h`). Los demás escalares de campos, y los
   elementos de arreglos de campos que no son enum, quedan con `load/store volatile` sin orden. Los casos
   especiales existentes cubren globales (`isClangMsVolatileScalarGlobal`, `clangMsVolatileGlobalBoolArray`,
   `clangMsVolatileGlobalScalarArrayAlignment`), punteros directos (`clangVolatileDirectCPointeeAlignment`) y
   elementos enum (`clangMsVolatileEnumElementAlignment`).

## Corrección propuesta (3 archivos, +83 líneas, sólo adiciones)

- Nuevo `src/LLVM/Lowering/CVolatileRecordField.h`: `clangMsVolatileRecordFieldWidth(memberModel, lookup, node)`
  devuelve el ancho atómico cuando:
  - el nodo es `FieldAddress`/`FieldLoad`, o `RawElementAddress` cuya raíz es uno de ellos, con `volatileAccess`;
  - el miembro raíz (por `memberOrdinal`) es externo, declarado `volatile`, no es campo de bits, tiene semántica
    MS y pertenece a un struct o union C (`CppImportedTypeOrigin::CStruct`/`CUnion`);
  - el tipo es `bool`, entero, `float`, `double`, puntero o enum de 1, 2, 4 u 8 bytes (los punteros a función
    conservan su regla actual);
  - la alineación demostrada de la dirección (`fieldOrArrayAddressAlignment`) es al menos ese ancho.
- La volatilidad heredada del receptor no cuenta, porque se exige que el miembro esté declarado `volatile`; así lo
  hace Clang (`ProbeInherited`, campos de `volatile VInner`).
- `OopLlvmExpressionMachine.cpp`: `lowerExpressionFrame004FieldLoad` y `lowerExpressionFrame030PointerLoad` ponen
  `acquire` y alineación = ancho en la carga (antes del `trunc` de `_Bool`).
- `OopLlvmArraysAndStores.cpp`: `storeFromParts` pone `release` en el almacenamiento; en `compoundStoreFromParts`,
  la rama sin campo de bits pone `acquire` en la carga previa y `release` en el almacenamiento.

## Resultados con el parche (worktree separado en `fd8ed5c1a`, build `d66b213fd0a89122`)

Log `A:\nsc-claude-t08-layout-build\logs\volatile-main-parche-1.log`:

| Prueba | Resultado |
|---|---|
| `pruebas-tanda-08-volatile-campos-c`, ejecución directa / `Shared` / `Metadata`, O0–O3 | 12 de 12 con código 0 |
| IR, directo / `Shared` / `Metadata` / O1 / O2 / O3 | 34 de 34 `IGUAL` en cada informe (antes 28 `DEFECTO`); `validar.cmd` termina con 0 |
| `ProbeVolatile` de `pruebas-tanda-08-bool-campos-c` | `load atomic volatile i8 ... acquire` y `store atomic volatile i8 ... release`, como Clang |
| `ProbeInherited` | sigue `volatile` sin orden, como Clang |
| `pruebas-tanda-08-bool-campos-c` del árbol de Codex | pasó |
| `pruebas-tanda-08-layout-clang` | pasó |
| `pruebas-tanda-08-layout`, `pruebas-tanda-08-record-c` (copias aisladas) | pasaron |
| `pruebas-tanda-08-bitfields-c`: `validar-volatile`, `validar-volatile-metadata`, `validar-matriz-volatile` (copias aisladas) | pasaron |

Línea base de las mismas regresiones sin el parche (build `80d2c61cb947e44f`): todas pasaban; sólo fallaba el
control volátil.

El parche se guardó con `git diff`, el worktree de prueba volvió a `fd8ed5c1a` limpio y `git apply --check` lo
acepta sobre `fd8ed5c1a`. Con un índice temporal también aplica sobre la cabeza actual de `development-cpp-clang`
(`d73c71420`); sobre esa cabeza no se construyó ni probó.

## Conflictos previstos

- `src/LLVM/Lowering/OopLlvmExpressionMachine.cpp`: dos bloques nuevos, después de `LLVMSetVolatile(load, 1)` en
  `FieldLoad` y en `PointerLoad`, y un `#include`.
- `src/LLVM/Lowering/OopLlvmArraysAndStores.cpp`: tres bloques nuevos, después de `LLVMSetVolatile(previous/store, 1)`
  en `compoundStoreFromParts` (carga y almacenamiento) y en `storeFromParts`, y un `#include`.
- `src/LLVM/Lowering/CVolatileRecordField.h`: nuevo. No modifica `PackedArrayAlignment.h`.

## Hallazgos fuera del parche (no implementados)

1. Campo `volatile` de tipo enum y campo `volatile` de tipo record: NSC no los importa (`has no compatible member
   named 'mood'`/`'inner'`); para records la exclusión es explícita (`!field.type.isVolatile` en
   `CppSemanticRecord.cpp`). Clang trata el enum volátil como `atomic i32`. Cuando se importen, el predicado ya
   cubre el enum por su tipo subyacente.
2. Divergencia directo/precompilado: si el encabezado de una biblioteca contiene un record con un campo que no se
   importa, el consumidor directo lo omite, pero el consumidor precompilado falla por completo con
   `C record field cannot be reconstructed` (`ProbeOddConsumer`), aunque use sólo otros campos. Está en la
   reconstrucción del catálogo (`CppSemanticRecord.cpp`), terreno de identidad/catálogo de Codex.
3. `clangMsVolatileFunctionFieldAlignment` usa `volatileAccess` del nodo, que también es verdadero por
   volatilidad heredada; con un puntero a función dentro de un record volátil podría emitir orden donde Clang no lo
   hace (INFERENCIA por lectura; no se probó).
4. Campos de bits volátiles: fuera de este parche; tienen su propio camino y sus pruebas pasaron sin cambios.

## Autorización necesaria

Integrar el parche en la rama de Codex. T08-006 sigue abierto.
