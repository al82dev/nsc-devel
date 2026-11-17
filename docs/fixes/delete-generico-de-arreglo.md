# `delete` genérico de un elemento cuyo tipo es un arreglo dinámico

## Problema

Tras agregar los `using` que faltaban en Saru, `SaruDX11` (que usa `List<byte[]>` y
`Dictionary<string, byte[]>`) se detenía en las plantillas transportadas de
`NativeSharp.Core.Collections.Generic`:

```
...NativeSharpCoreCollectionsGeneric.nsmeta.public-template-40.ns:710:17: error: a dynamic array
is released with delete[], not with scalar delete
                    delete items[i];
```

`List`, `Dictionary`, `Queue` y `Stack` liberan cada elemento con un `delete` escrito sobre `T`.
Es el contrato Stage 52 documentado en `lowerScalarDeleteExpression` (`HirExceptionAndCleanup.cpp`):
el código de limpieza de un contenedor genérico libera cada elemento de forma uniforme para todo `T`
(un tipo valor es un no-op legal). Con `T = byte[]`, la especialización queda como un `delete`
escalar sobre un arreglo:

- Antes de LKF-005 (`3a88577bb`), ese `delete` bajaba a no-op: el contenedor no liberaba los
  arreglos de sus elementos.
- Desde LKF-005, que rechaza el `delete` escalar escrito sobre un arreglo, el consumidor no compila.
  El error se reporta en la plantilla de la biblioteca, donde el consumidor no puede corregirlo.

## Cambio

- `ConstructionAndLifetimeExpressions.cpp`: dentro de una especialización genérica (método genérico
  especializado o miembro de un tipo genérico cerrado, `callableBelongsToGenericTypeSpecialization`),
  un `delete` escalar cuyo operando es un arreglo dinámico no se rechaza. Se registra en
  `scalarDeleteReleasesArray_`.
- `ProjectCallableResultMerger.cpp`: fusiona ese conjunto desde los shards.
- HIR (`HirExceptionAndCleanup.cpp`, `HirCompletionTerminalFrames.cpp`,
  `HirInitializationValueFrames.cpp`): un sitio registrado se baja con la forma de arreglo, igual
  que `delete[]`, incluido el direccionamiento del slot, que se vacía antes de liberar. La propiedad de
  los elementos es la misma que tendría un `delete[]` escrito sobre ese operando.

## Comportamiento conservado

- Un `delete` escalar escrito directamente sobre un arreglo fuera de una especialización genérica
  sigue rechazado.
- `delete` de referencias, cadenas, punteros y valores (incluido el drop de structs) no cambia.

## Límites y hallazgo aparte

- `delete[]` de una fila `owned Payload[]` dentro de un `owned Payload[][]` no destruye los
  `Payload`, tanto escrito a mano como a través de este cambio (sonda en
  `A:\nsc-devel-fixes-evidencia\fuga-control`). Este cambio iguala esa semántica existente; no la
  corrige. Queda como hallazgo no implementado.
- La prueba nueva no mide fugas: un control que fuga 25 GB terminó con éxito porque Windows compromete
  memoria de forma perezosa.

## Pruebas

Nueva: `tests/integration/generic_array_element_delete.py`.

## Evidencia

En `A:\nsc-devel-fixes-evidencia`, build `compilar-nsc-delete.log` (sin errores ni warnings):

- Prueba nueva: con este build, `delete` escalar fuera de genéricos se rechaza y el contenedor genérico
  con `byte[]`, `int` y una clase sale con 42 en O0, O1, O2 y O3 (`delete-generico-nuevo2.log`). Con el
  nsc instalado (build `bef53b07d4af0012`) falla con el mismo error que Saru (`delete-generico-instalado.log`).
- `SaruDX11`: el chequeo de cuerpos con este build da `ok` (`saru-check-delete.log`).
- Existentes sin modificar: `frame_pointer_delete_delegate_rules.py` (la prueba de LKF-005, incluido
  el rechazo de `delete` escalar de `T[]`) y `array_and_stack_limits.py` terminan con 0
  (`delete-rules-nuevo.log`, `array-limits-nuevo.log`); `t06.escape-summaries` `PASS: 575 checks`;
  `Stage58` `PASS: 71 checks` (`unit-*-delete.log`).

No verificado aquí: la emisión completa de `SaruDX11`. Llega a una fase posterior, antes inalcanzable,
y se detiene en siete diagnósticos de vida de referencias a elementos de listas de clases, sin relación
con este cambio (`saru-nuevo\emit-O0.log`).
