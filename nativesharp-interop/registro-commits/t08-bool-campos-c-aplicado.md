# T08-006: corrección aplicada para `_Bool` en campos de records C

## Estado

APLICADO EN LA RAMA AISLADA `interop-t08-claude-layout`. No se aplicó en la copia de Codex. Es el parche
preparado en [t08-bool-campos-c-diagnostico.md](t08-bool-campos-c-diagnostico.md), sin cambios
(`git apply` de [t08-bool-campos-c-almacenamiento.patch](t08-bool-campos-c-almacenamiento.patch)). T08-006 sigue
abierto: los casos que el parche no cubre están en
[t08-bool-campos-c-no-cubierto.md](t08-bool-campos-c-no-cubierto.md) y el ítem incluye además el resto de tipos
enteros C.

## Cambio

- Nuevo `src/LLVM/Lowering/CRecordBoolStorage.h`: `cRecordBoolStorage(lookup, node)` reconoce un campo
  `builtin:bool`, o un elemento de un campo `builtin:bool[...]`, de un struct o union C importado (receptor por
  valor o por puntero), con `nameLookup_.cppImportedType(...)->origin`.
- `src/LLVM/Lowering/OopLlvmExpressionMachine.cpp`: `lowerExpressionFrame004FieldLoad` y
  `lowerExpressionFrame030PointerLoad` cargan `i8` y convierten con `trunc` a `i1`.
- `src/LLVM/Lowering/OopLlvmArraysAndStores.cpp`: `storeFromParts` extiende con `zext` a `i8` también para
  `cRecordBoolStorage` (una línea más el `#include`).
- Sin cambios en MIR, NSMETA, NSLink, parser, catálogo C ni pruebas existentes.

## Resultados (build `af83352bcc0269aa`, log `A:\nsc-claude-t08-layout-build\logs\bool-aplicado-1.log`)

| Prueba | Resultado |
|---|---|
| `pruebas-tanda-08-bool-campos-c`: ejecución directa, `Shared` y `Metadata`, O0–O3 | 12 de 12 con código 0 |
| Bytes: escritura NativeSharp de `1`/`0` leída por C como `unsigned char` y como `_Bool`; vecinos | correctos en las 6 ubicaciones |
| Lecturas NativeSharp de valores escritos por C; llamadas C con `_Bool` | correctas |
| IR contra Clang `-O1`, directo / `Shared` / `Metadata` | 8 / 8 / 8 accesos `IGUAL` (`i8`, alineación 1); antes 24 `DEFECTO` |
| IR optimizado de NSC O1, O2, O3 (`BoolAccess`) | 0 accesos `i1` a memoria, 8 `i8` |
| IR sin optimizar (O0, `Shared`, `Metadata`) | los únicos `i1` en memoria son las ranuras locales de parámetros (`%pN.addr`) |
| `validar.cmd` del control | termina con 0 (antes con 1) |
| `pruebas-tanda-08-layout`, copia aislada, O0–O3 y negativos (incluye `CBoolFields`) | pasó; `CBoolFields` usa `load i8`/`store i8` |
| `pruebas-tanda-08-layout-clang` (directo, `Shared`, `Metadata`, O0–O3, IR) | pasó; sin `DEFECTO` ni `SUBALINEADO` |
| `pruebas-tanda-08-record-c`, copia aislada (`_Bool` volátiles globales O0–O3) | pasó |

Los tres caminos de consumo usan el mismo predicado: el consumidor sólo-metadata reconstruye el record C desde el
catálogo como tipo importado con origen `CStruct`/`CUnion`, y su IR quedó igual al directo.

Sondas registradas en la misma ejecución: `ProbeAddress` y `ProbeGlobal` compilan y terminan con 0 y siguen
con `i1`; `ProbeCompound` sigue rechazada por el frontend. Se documentan aparte.

## Conflictos previstos con archivos compartidos

- `src/LLVM/Lowering/OopLlvmArraysAndStores.cpp` (modificado por Codex): el cambio está en la línea que construye
  `storedValue` en `storeFromParts`, junto a la condición `cVolatileBool`, y en los `#include`. Un cambio de Codex
  en esa condición o en ese bloque produce un conflicto de texto; la resolución es conservar la condición de Codex
  y agregar `|| cRecordBoolStorage(nameLookup_, boolTarget)`.
- `src/LLVM/Lowering/OopLlvmExpressionMachine.cpp`: lowering compartido, no está en la lista de archivos de Codex.
  Las cargas cambian después del caso especial `clangMsVolatileGlobalBoolArray`, que queda intacto.
- `src/LLVM/Lowering/CRecordBoolStorage.h`: archivo nuevo, sin conflicto.
- `nativesharp-interop/TRACKING.md`: nota bajo T08-006.

No se ejecutó la batería completa de la tanda 8 ni la suite de sesión (sus fixtures sólo existen bajo
`A:\nsc-interop-build`). Codex revisa la integración.
