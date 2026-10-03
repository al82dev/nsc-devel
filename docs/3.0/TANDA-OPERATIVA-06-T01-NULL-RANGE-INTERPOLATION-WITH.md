# Tanda operativa 06 — T01 null, índices, interpolación y `with`

## Resultado

Esta tanda conecta con el compilador productivo un vertical seguro de `NS3-T01-F03/F04/F06/F07`:

- `??` y `??=` sobre carriers físicos nullables que ya son punteros en la ABI de NativeSharp;
- `?.` para campos de instancia cuyo resultado también tiene carrier nullable;
- índices `^n` sobre arreglos nativos y cadenas, con una sola evaluación del receptor y de la
  longitud;
- interpolación completamente constante, plegada a la ruta ordinaria de literal global;
- `with` sobre structs con lifetime trivial, mediante copia completa seguida de updates en orden
  textual.

El trabajo no declara completo todo T01. Cuando una forma requiere un protocolo aún inexistente,
el compilador la rechaza con una causa concreta en vez de adivinar un layout, introducir heap
oculto o duplicar efectos. Las dependencias se registran al final de este documento.

## Semántica implementada

### `left ?? fallback`

- El operando izquierdo se analiza y se baja una vez.
- El fallback conserva un estado de flow separado y sólo se ejecuta en la arista nula.
- El fallback debe convertirse implícitamente al tipo físico del lado izquierdo.
- El HIR publica `null-coalescing`; MIR 1.16 publica `null-coalescing`.
- LLVM genera branch y `phi`, no un `select`, por lo que llamadas, traps y moves del fallback son
  realmente lazy incluso en O0.

### `place ??= fallback`

- Sólo admite storage direccionable; no reevalúa propiedades o indexers.
- La dirección se obtiene una vez, el valor actual se carga una vez y el fallback sólo se baja en
  el bloque nulo.
- El store sólo existe en la arista nula y el valor de la expresión sale de un `phi`.
- El HIR publica `null-coalescing-assignment`; MIR 1.16 publica `null-coalescing-store`.

### `receiver?.field`

- El receiver debe ser referencia de clase o interfaz.
- El receiver se evalúa una sola vez.
- El campo sólo se carga en el bloque no nulo y el bloque nulo aporta el valor null al `phi`.
- El resultado debe tener carrier nullable. Un miembro valor necesita `Optional<T>` y un
  property/método necesita conditional-call capturado; esas formas se rechazan hasta que exista
  el protocolo correspondiente.

### Índice desde el final

- `^distance` exige distancia integral y rechaza constantes negativas.
- Para `array[^n]` y `string[^n]`, LLVM reutiliza la longitud ya cargada y calcula
  `length - distance`.
- Los bounds checks existentes siguen siendo la autoridad: `^0` y distancias mayores que la
  longitud fallan como índice de elemento fuera de rango.
- HIR y MIR muestran `index=from-end`, permitiendo inspeccionar que no se perdió la decisión.

### Rangos

- Los endpoints abiertos/cerrados y `^` se tipan como índices integrales.
- No se materializa silenciosamente un segundo arreglo ni una segunda cadena.
- El slicing de heap array se rechaza indicando `Span<T>/ReadOnlySpan<T> Length/Slice`; el slicing
  de string se rechaza indicando `StringView.Slice`.
- El protocolo general `Length/Slice`, sus carriers prestados y su comparación contra loops
  manuales pertenecen a la tanda 21. Aceptarlo antes ligaría el compilador a un layout privado de
  biblioteca y violaría el contrato sin heap.

### Interpolación estática

- El parser compartido separa literales, holes y formatos constantes.
- Holes `bool` e integer literal admiten formato vacío y `D/d/X/x` con ancho constante.
- `StaticInterpolationHandler` es la única autoridad de recorrido: recibe literales y holes en
  orden textual y permite seleccionar en compile time un handler de tipo usuario sin depender de
  `NativeSharp.Core.String` ni de cultura global.
- El resultado se pliega a un `string-literal` internado; HIR/MIR/LLVM usan la ruta existente de
  literal y no `String.Concat`.
- El handler built-in comparte clasificación y rendering entre parser y binder; `-42:D5` produce
  `-0042`, `-0` se canoniza como cero y `X` conserva casing sin consultar locale.
- Los holes runtime se desazucaran a la superficie explícita de `NativeSharp.Core.String` cerrada
  por T29. Un proyecto que no suministra esa superficie falla cerrado en vez de ocultar heap.

### `source with { Field = value }`

- La fuente se evalúa una vez antes de los updates.
- Se valida el nombre de cada campo, duplicados y conversiones.
- Nombrar un campo privado obedece la accesibilidad ordinaria: se acepta dentro del tipo
  declarante y se rechaza fuera. Copiar los bytes no convierte un campo privado en API pública.
- Un campo `readonly` puede recibir su valor durante la construcción del nuevo resultado; el
  valor fuente no se muta.
- HIR usa un `aggregate-spread` de todos los campos y después los updates en orden textual.
- MIR publica `aggregate-spread-store`; LLVM copia a un slot final y actualiza in-place.
- Sólo se aceptan agregados triviales. Un aggregate owning/move-only requiere clone/copy strategy y
  drop flags; esa extensión queda ligada a las tandas 13–14.

## Contratos físicos y de seguridad

- No se inventó una ABI para `Optional<T>`: hoy es un struct genérico de `NativeSharp.Core` con
  layout privado y métodos `HasValue`/`UncheckedValue`.
- No se inventó un tipo range builtin ni un slice owner. Los views continúan siendo
  `Span<T>`, `ReadOnlySpan<T>` y `StringView` de biblioteca.
- No se introduce array temporal, boxing ni `String.Concat` en las formas aceptadas.
- Toda forma aceptada tiene nodo HIR/MIR visible; toda forma aún no representable falla antes de
  LLVM con diagnóstico causal.
- El schema OOP MIR pasa de 1.15 a 1.16 por los nuevos nodos explícitos.

## Archivos nuevos

- `tests/cc1/c56_ns3_null_with/c56_ns3_null_with.nsproj`: proyecto Language 3 mínimo y permanente.
- `tests/cc1/c56_ns3_null_with/Main.ns`: fixture ejecutable para laziness, evaluación única,
  `??=`, `?.`, `^`, interpolación constante y `with`.
- `docs/3.0/TANDA-OPERATIVA-06-T01-NULL-RANGE-INTERPOLATION-WITH.md`: expediente de esta tanda;
  separa soporte real, rechazos y dependencias posteriores.

## Archivos modificados

- `include/nativesharp/Frontend/ProjectMethodBodyModel.h`
- `include/nativesharp/Frontend/ProjectOopHirModel.h`
- `include/nativesharp/Frontend/ProjectOopMirModel.h`
- `src/Frontend/ProjectMethodBodyModel.cpp`
- `src/Frontend/ProjectOopHirModel.cpp`
- `src/Frontend/ProjectOopHirModelDumper.cpp`
- `src/Frontend/ProjectOopMirModel.cpp`
- `src/Frontend/ProjectOopMirModelDumper.cpp`
- `src/LLVM/ProjectOopLlvmLowerer.cpp`
- `tests/unit/Cc1CorrectnessTests.cpp`
- `docs/3.0/TANDA-01-EXPRESIONES-Y-ERGONOMIA.md`

## Mejora obtenida

- Las operaciones null-aware aceptadas ya conservan laziness y evaluación única hasta LLVM O0.
- `^` deja de ser sólo sintaxis y usa los bounds checks existentes sin una segunda lectura del
  receptor.
- `with` trivial reutiliza la construcción in-place y evita temporales de heap.
- La interpolación totalmente constante desaparece en compile time como literal ordinario.
- Los dumps HIR/MIR hacen auditables las decisiones y los rechazos impiden semántica parcial
  silenciosa.

## Validación dirigida

- Build exclusivo: `cmake --build A:\nsc-devel\build-clang --target nsc --config Debug --parallel 12`.
- Compilador probado: `A:\nsc-devel\build-clang\bin\nsc.exe`.
- Fixture `c56_ns3_null_with`, O0: compila/enlaza y termina con código **42**.
- Dumps HIR: `null-coalescing`, `null-coalescing-assignment`, `null-conditional-field-value`,
  `aggregate-spread`, `index=from-end` y literal plegado presentes.
- Dumps MIR schema 1.16: `null-coalescing`, `null-coalescing-store`,
  `null-conditional-field-load`, `aggregate-spread-store`, `index=from-end` y literal presentes.
- Helper sintáctico T01: **27/27** checks.
- Cuatro negativas dirigidas comprueban el texto causal para runtime interpolation, value `?.`,
  heap-array slicing y owning `with`; también quedan registradas permanentemente en el bloque
  `--rest` de `Cc1CorrectnessTests`.
- `NSC_BUILD_TESTS` quedó restaurado a `OFF`.
- No se ejecutó CTest, baseline, matriz O1/O2/O3, instalación ni despliegue.

## Deuda explícitamente asignada

| Pendiente | Tanda dueña | Motivo |
|---|---:|---|
| resultado valor de `?.` | 17 | `Optional<T> ??` ya usa el protocolo público; falta producir el carrier desde acceso condicional |
| properties/métodos/cadenas de `?.` | 17 | necesita conditional-call + `Optional<T>` estable |
| protocolo general `Length/Slice` y ranges prestados | 21 | depende de iterators/ranges y views con lifetime comprobable |
| holes runtime y formatting handler stack/noheap | cerrada en T29/F06 | runtime usa Core explícito; compile time usa `StaticInterpolationHandler` sin Core |
| `with` sobre owning/move-only y cleanup en fallo | 13–14 | depende de move/borrow/drop flags y clone explícito |
| tooling completo de todas las formas | 29 | LSP/debugger deben consumir la semántica final, no inferirla |

T01 permanece `IMPLEMENTING` hasta cerrar esas filas. La tanda operativa 06 sí queda cerrada porque
su alcance ejecutable y sus límites son completos, probados y no prometen carriers inexistentes.
