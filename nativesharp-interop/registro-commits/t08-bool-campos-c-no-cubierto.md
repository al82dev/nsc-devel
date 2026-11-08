# T08-006: casos `_Bool` que la corrección de campos no cubre

## Estado

DOCUMENTADO, NO IMPLEMENTADO. La corrección aplicada
([t08-bool-campos-c-aplicado.md](t08-bool-campos-c-aplicado.md)) cubre sólo lecturas y escrituras de campos
`_Bool` y elementos de arreglos `_Bool` de structs y unions C importados. Cada caso de abajo necesita una decisión
o toca un carril compartido, y no se amplía el parche sin coordinarlo.

Evidencia: sondas de `nativesharp-interop/pruebas-tanda-08-bool-campos-c`, build `af83352bcc0269aa` (con la
corrección), log `A:\nsc-claude-t08-layout-build\logs\bool-aplicado-2.log`, IR en
`run\tanda08-bool-campos-c`. Las sondas sólo registran; no hacen fallar `validar.cmd`.

Este commit agrega la sonda `ProbeVolatile` (campo `volatile _Bool` en el record `BoolVolatile`, con sus accesores
C) y su tabla informativa `tabla-volatile.txt`.

## 1. `bool*` obtenido de un campo (`ProbeAddress`)

```c
bool* flag = &fields->flag; *flag = true; ... if (*flag) ...
```

- Resultado: compila y termina con 0.
- IR de NSC: `store i1 true, ptr %local.load1` y `load i1, ptr %local.load5`. Clang usa `i8` para todo acceso a
  través de `_Bool*`.
- Causa: el puntero `bool*` no conserva el origen C del campo; el predicado de la corrección sólo ve
  `FieldAddress`/`FieldLoad` y `RawElementAddress` cuya raíz es un campo C.
- Corrección posible: que todo `bool` de NativeSharp en memoria use `i8` (locales, campos nativos, arreglos,
  punteros, estáticos). Es una decisión general de representación que afecta todo el lowering, no sólo interop.

## 2. Operadores `|=` y `^=` sobre campos `_Bool` (`ProbeCompound`)

```c
fields->values[1] |= true;
fields->flag ^= true;
```

- Resultado: rechazado por el frontend, `error: compound assignment requires compatible numeric operands in
  Stage 34`, en ambas líneas. C los admite.
- La única excepción vigente es la de globales C volátiles con semántica MS (`pruebas-tanda-08-record-c`,
  compuestos sobre `_Bool` volátiles, que pasaron).
- Admitirlos es una decisión de lenguaje. Si se admiten, el almacenamiento compuesto
  (`compoundStoreFromParts`, `OopLlvmArraysAndStores.cpp`, archivo de Codex) necesitaría el mismo trato `i8`.

## 3. Globales `_Bool` no volátiles (`ProbeGlobal`)

```c
extern _Bool bool_global; extern _Bool bool_global_array[2];
```

- Resultado: compila y termina con 0.
- IR de NSC: `@bool_global = external global i1`, `@bool_global_array = external global [2 x i1]`,
  `store i1`/`load i1` (`static.field.load`, `pointer.load`).
- Clang: `@bool_global = dso_local global i8 0, align 1`, `@bool_global_array = ... global [2 x i8]`.
- Camino: declaración y acceso de globales (`OopLlvmStaticState.cpp`, `OopLlvmExpressionLeaves.cpp`), compartido con
  el trabajo de globales y TLS de Codex. Los globales volátiles y atómicos ya usan `i8`.

## 4. `zeroext` en parámetros y resultados `_Bool`

- IR de NSC: `declare void @bool_c_store(i32, i1)`, `declare void @acc_set_flag(ptr, i1)`,
  `declare i1 @acc_get_flag(ptr)`, sin atributos.
- Clang: `define void @bool_c_store(i32 noundef %0, i1 noundef zeroext %1)`,
  `define zeroext i1 @acc_get_flag(...)`.
- En Win64 Clang sólo extiende `_Bool`; `char`, `short` y `unsigned char` no llevan `signext`/`zeroext`
  (comprobado con `fc(char, short, unsigned char, _Bool)`).
- Con 0 y 1 la ejecución es correcta (códigos 200 y 201 del control no se producen). Sin `zeroext`, LLVM no
  garantiza los bits altos del registro que espera el código de Clang; no se reprodujo un fallo.
- Pertenece al ABI de firmas de funciones C, no al almacenamiento de records.

## 5. Campo `volatile _Bool` en un record (`ProbeVolatile`, nueva)

```c
typedef struct BoolVolatile { char tag; volatile _Bool flag; } BoolVolatile;
```

- Resultado: compila y termina con 0 (escritura NativeSharp leída por C y viceversa).
- Clang (`clang.exe --target=x86_64-pc-windows-msvc`, semántica volátil MS):
  `load atomic volatile i8, ptr %2 acquire, align 1` y `store atomic volatile i8 %3, ptr %4 release, align 1`.
- NSC con la corrección: `load volatile i8` + `trunc` y `zext` + `store volatile i8`, sin orden adquirir/liberar.
  El tipo de memoria ya es `i8` porque el predicado de la corrección no distingue `volatile`; el orden falta.
- El verificador marca `FALTA`/`DEFECTO` porque sus expresiones no reconocen `load atomic`; la comparación de
  arriba es la lectura directa del IR.
- Causa (INFERENCIA por lectura de código): el orden MS para campos sólo existe para punteros a función
  (`clangMsVolatileFunctionFieldAlignment`, `PackedArrayAlignment.h`); afectaría igual a otros escalares volátiles
  en campos de records, no sólo a `_Bool`. No se comprobó con otros tipos.

## Coordinación

| Caso | Archivos o decisión |
|---|---|
| 1 | decisión de representación de `bool` en memoria; lowering general |
| 2 | decisión de lenguaje; `compoundStoreFromParts` en `OopLlvmArraysAndStores.cpp` (Codex) |
| 3 | `OopLlvmStaticState.cpp`, `OopLlvmExpressionLeaves.cpp` (Codex), carril de globales y TLS |
| 4 | atributos de las declaraciones de funciones C; el punto exacto del lowering no se localizó |
| 5 | orden MS de campos volátiles en lowering de campos y `PackedArrayAlignment.h` (Codex) |

T08-006 sigue abierto.
