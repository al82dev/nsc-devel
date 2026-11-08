# T08-006: diagnóstico de `_Bool` en campos de records C y parche propuesto

## Estado

DIAGNÓSTICO Y PARCHE PREPARADOS, NO APLICADOS. La corrección necesita cambiar el almacenamiento en
`src/LLVM/Lowering/OopLlvmArraysAndStores.cpp`, archivo que Codex está modificando. Por eso el parche queda en
[t08-bool-campos-c-almacenamiento.patch](t08-bool-campos-c-almacenamiento.patch) y las fuentes de la rama están en
HEAD. No toca NSMETA, NSLink, el parser, MIR ni el catálogo C. El control que lo mide está en
[t08-bool-campos-c-control.md](t08-bool-campos-c-control.md).

## IR de cada lado (HECHO COMPROBADO)

Clang 23.1.0, `Api.c` del fixture:

```llvm
; -O0
%5 = load i8, ptr %4, align 1
%6 = trunc i8 %5 to i1
; store
%3 = zext i1 %1 to i8
store i8 %3, ptr %4, align 1
; -O1
%3 = load i8, ptr %2, align 1, !range !{i8 0, i8 2}, !noundef
%4 = icmp ne i8 %3, 0
```

NSC sin corrección (`project.ll`, igual en `Shared.ll` y `Metadata.ll`; igual de O0 a O3):

```llvm
%field.load = load i1, ptr %field.addr, align 1          ; campo escalar
%pointer.load = load i1, ptr %raw.element.addr, align 1  ; elemento del arreglo fijo
store i1 %p1, ptr %field.addr, align 1
```

## Memoria frente a valor lógico

- Representación en memoria: igual en los dos lados. Un byte, offset y alineación de Clang, valor 0 o 1. El
  control lo comprueba leyendo el byte crudo desde C después de cada escritura de NativeSharp.
- Conversión del valor lógico: Clang la hace explícita en el IR (`trunc`/`icmp ne` al leer, `zext` al guardar).
  NSC no la hace: usa `i1` como tipo de valor y también como tipo de memoria, y deja la conversión al
  legalizador de x86, que hoy escribe y lee el byte completo con 0 o 1.
- Por qué es un defecto aunque la ejecución pase: según la LangRef de LLVM, para tipos que no ocupan bytes enteros
  (como `i1`), una carga cuyo valor no se guardó con un almacenamiento del mismo tipo da resultado indefinido, y
  un almacenamiento deja sin especificar los bits sobrantes del byte. El byte lo escribe C con `store i8` y NSC lo
  lee con `load i1`; NSC lo escribe con `store i1` y C lo lee como `i8` con `!range [0,2)` y `!noundef`. Con
  valores válidos no se observa fallo en O0–O3 (12 de 12 ejecuciones), pero el contrato de Clang no se cumple.

## Ruta causal (HECHO COMPROBADO por lectura de código)

1. `src/LLVM/Lowering/Support/LlvmTypeMapping.cpp:67`: `builtin:bool` se traduce a `i1`; el mismo tipo sirve de
   valor y de memoria.
2. `OopLlvmExpressionMachine.cpp`, `lowerExpressionFrame004FieldLoad`: `LLVMBuildLoad2(builder_, type, ...)` con
   `type = llvmType(current->typeKey)`.
3. `OopLlvmExpressionMachine.cpp`, `lowerExpressionFrame030PointerLoad`: lectura de un elemento de arreglo fijo
   (`RawElementAddress` cuya raíz es el `FieldAddress` del arreglo).
4. `OopLlvmArraysAndStores.cpp`, `storeFromParts`: guarda el valor `i1` sin extender, salvo cuando
   `cVolatileBool` es verdadero.
5. Los casos `i8` que ya existen cubren sólo globales C volátiles con semántica MS (`clangMsVolatileGlobalBoolArray`
   y `cVolatileBool`), atómicos (`cppAtomicAccess`) y campos de bits (`bitfield.bool.widen`). Ningún campo de
   record usa `i8`.
6. MIR no marca el origen C de un acceso no volátil, pero el lowering ya lo deduce del tipo receptor con
   `nameLookup_.cppImportedType(...)->origin` (precedentes en `OopLlvmStatements.cpp:246-254` y
   `LlvmTypeLayout.cpp:24-26`). El camino precompilado reconstruye el mismo tipo importado, así que la misma
   consulta cubre directo y metadata.

## Cambio mínimo propuesto (parche, 3 archivos)

- Nuevo `src/LLVM/Lowering/CRecordBoolStorage.h`: `cRecordBoolStorage(lookup, node)` es verdadero para un
  `FieldAddress`/`FieldLoad` de tipo `builtin:bool`, o un `RawElementAddress` de `builtin:bool` cuya raíz es un
  campo `builtin:bool[...]`, cuando el receptor es un struct o union C importado (por valor o por puntero). Los
  campos de bits no entran (otra clase de nodo).
- `OopLlvmExpressionMachine.cpp`: en las dos cargas, `load i8` y después `trunc` a `i1`, igual que Clang `-O0`.
  La alineación, `volatile` y el orden atómico siguen aplicándose a la carga.
- `OopLlvmArraysAndStores.cpp`: en `storeFromParts`, la condición que ya produce `zext` a `i8` para `cVolatileBool`
  incluye también `cRecordBoolStorage`. Es una línea más el `#include`.
- No se agregan `!range` ni `!noundef`: son pistas de optimización de Clang, no cambian el acceso.

## Resultados con el parche (build `cc99db0f1dd600b4`)

| Prueba | Log | Resultado |
|---|---|---|
| `pruebas-tanda-08-bool-campos-c` | `bool-parche-1.log`, `bool-parche-2.log` | 12 de 12 ejecuciones; IR 24 de 24 `IGUAL`; termina con 0 |
| `pruebas-tanda-08-layout` (copia aislada, O0–O3, negativos; incluye `CBoolFields`) | `bool-regresion.log` | pasó; `CBoolFields` emite `load i8`/`store i8` |
| `pruebas-tanda-08-layout-clang` | `bool-regresion.log` | pasó; 144 comparaciones `IGUAL`, campos de bits informativos sin cambio |
| `pruebas-tanda-08-record-c` (copia aislada `validar-record-c-claude.cmd`) | `bool-reg-record-c.log` | pasó; `_Bool` volátiles O0–O3 y "68 byte accesses" sin cambio |

IR resultante (O0 y O2):

```llvm
%c.bool.element.load = load i8, ptr %raw.element.addr, align 1
%c.bool.value = trunc i8 %c.bool.element.load to i1
%c.bool.storage = zext i1 %p1 to i8
store i8 %c.bool.storage, ptr %field.addr, align 1
```

El parche se guardó con `git diff` (6212 bytes), se restauraron las fuentes y `git apply --check` lo acepta sobre
HEAD. La carpeta de compilación se reconstruyó desde HEAD (build `365e1864c3703bb3`; el hash no coincide con
`7fb3fcdc61f265b4` aunque las fuentes son las de HEAD, INFERENCIA: depende de datos del árbol que `git checkout`
renovó). Con ese build el control vuelve a dar 24 `DEFECTO` y termina con 1 (`bool-restaurado.log`).

## Consumidores y pruebas que pueden verse afectados

- Todo IR que lea o escriba un `_Bool` de un record C importado pasa de `i1` a `i8` más `trunc`/`zext`. El valor y
  el layout no cambian. MIR no cambia; el objeto emitido sí, así que la caché de artefactos no debe reutilizar
  objetos de un compilador anterior (INFERENCIA: la identidad del compilador ya forma parte de la clave).
- `pruebas-tanda-08-layout` (`CBoolFields`, commits `8be3c138f` y `fe2fddfca`): pasa con el parche.
- `pruebas-tanda-08-flexible-c` (parche de arreglo flexible, no aplicado): con los dos parches, `FlexBools` usaría
  `i8` porque la raíz es `builtin:bool[*]`; la fila `bools` de su tabla podría dejar de ser informativa. No se
  combinaron los dos parches en esta rama.
- `pruebas-tanda-08-record-c`, `precompiled-bool`, `dll-bool` y `pruebas-tanda-08-tls-c`: usan globales, no campos;
  el parche no toca ese camino (`record-c` comprobado).
- Pruebas unitarias: no hay aserciones sobre `load i1`/`store i1` de campos (la única coincidencia,
  `NativeSharp3T07ErrorTests.cpp:593`, es de una variable local). La suite de sesión no se puede ejecutar en esta
  rama porque sus fixtures sólo existen bajo `A:\nsc-interop-build`.

## Fuera del parche (hallazgos, no implementados)

1. Puntero tomado de un campo (`bool* q = &p->flag`): el puntero `bool*` no conserva el origen C y sigue con `i1`
   (`ProbeAddress`). Clang usa `i8` para todo `_Bool*`. Cubrirlo exige que todo `bool` de NativeSharp en memoria
   use `i8`, una decisión general de representación que afecta locales, campos nativos, arreglos y punteros.
2. Asignación compuesta (`p->flag ^= v`, `p->values[1] |= v`): NativeSharp la rechaza en el frontend; C la admite.
   Hoy sólo se admite para globales C volátiles. Es una decisión de lenguaje; no se cambia.
3. Globales `_Bool` no volátiles y sus arreglos: `external global i1`/`[2 x i1]` con `load/store i1`; Clang define
   `global i8`/`[2 x i8]` (`ProbeGlobal`). Camino de declaración y carga de globales (`OopLlvmStaticState.cpp`,
   `OopLlvmExpressionLeaves.cpp`), compartido con el trabajo de globales y TLS. No se toca.
4. ABI de llamada: NSC declara `declare void @acc_set_flag(ptr, i1)` y `declare i1 @acc_get_flag(ptr)` sin
   `zeroext`; Clang define `i1 noundef zeroext`. En Win64 Clang sólo extiende `_Bool` (`char` y `short` no;
   comprobado con `fc(char, short, unsigned char, _Bool)`). Con 0 y 1 la ejecución pasa. Pertenece al ABI de
   firmas, no a campos de records.
5. Campo `volatile _Bool` en un record con semántica MS: no se probó; el acceso adquirir/liberar en campos sólo
   existe hoy para punteros a función.

## Archivos que requieren coordinación

- `src/LLVM/Lowering/OopLlvmArraysAndStores.cpp` (modificado por Codex): una línea en `storeFromParts` junto a
  `cVolatileBool` y un `#include`.
- `src/LLVM/Lowering/OopLlvmExpressionMachine.cpp` (lowering compartido, no está en la lista de Codex).
- `src/LLVM/Lowering/CRecordBoolStorage.h` (nuevo).

## Autorización necesaria

Aplicar el parche coordinado con Codex, reconstruir y ejecutar `pruebas-tanda-08-bool-campos-c`, que hoy termina
con 1 y con el parche termina con 0. T08-006 sigue abierto en cualquier caso: falta el resto de los tipos
enteros C del ítem y los hallazgos anteriores.
