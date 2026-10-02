# Tanda baseline 06H — offsets de `spread` y `with`

**Fecha:** 2026-10-01

**Estado:** terminada; pendiente de repetición del baseline por el usuario

**Alcance:** compilador NativeSharp 3.0, conversión OOP HIR → MIR

**Baseline completo:** no ejecutado por Codex

## Fallo observado

La corrida `20261001-220213-c69913fa` completó CTest con 567/567 pruebas,
pero la matriz ejecutable posterior encontró dos regresiones O0:

- `cc1-c55-ns3-calls-initializers` terminó con 10 en vez de 42.
- `cc1-c56-ns3-null-with` terminó con 5 en vez de 42.

Ambas fixtures compilaban y enlazaban correctamente. La comparación individual
con NSLink y `lld-link` produjo los mismos códigos, por lo que el enlazador quedó
descartado como causa.

## Causa raíz

HIR conservaba correctamente dos datos distintos en cada descriptor de un
`AggregateSpread`:

- `integerValue`: offset absoluto del campo de destino;
- `secondaryIntegerValue`: offset relativo del campo en la fuente.

Al convertir cada `AggregateElement` a MIR, la ruta genérica de metadatos
reutilizaba `secondaryImmediate` para el tamaño escalar. En los descriptores sin
hijos del spread eso reemplazaba el offset fuente por `sizeof(campo)`. Para
campos `int`, todos los offsets fuente terminaban siendo 4.

El LLVM emitido antes del arreglo demostraba el problema:

- `{ ..seed }` copiaba los tres elementos desde el offset 4;
- `Source() with { Y = Mark(2) }` copiaba tanto `X` como `Y` desde el offset 4.

## Cambio

Después de convertir un hijo `AggregateElement` cuyo padre es
`AggregateSpread`, MIR restaura en `secondaryImmediate` el offset fuente que HIR
ya había calculado. La corrección se realiza con conocimiento del padre, por lo
que no cambia el significado histórico de `secondaryImmediate` para:

- descriptores de igualdad/hash de agregados;
- elementos ordinarios de inicializadores;
- bitfields;
- stores escalares;
- layouts ni ABI.

## Archivo modificado

- `src/Frontend/ProjectOopMirModel.cpp`
  - conserva el offset relativo de fuente únicamente para descriptores hijos de
    `AggregateSpread`;
  - documenta por qué esa ruta no puede heredar el uso genérico del campo MIR.

No se crearon archivos de código, formatos, contratos ABI ni nuevos targets de
prueba. Este documento es el único archivo nuevo.

## Validación dirigida

Se reconstruyó incrementalmente sólo `nsc` y los targets de prueba implicados.
No se ejecutó el baseline completo.

- Contrato de enlace estático de `nsc.exe`: aprobado.
- `c55`, NSLink 3.0, O0: compilación 0; ejecución 42.
- `c56`, NSLink 3.0, O0: compilación 0; ejecución 42.
- `c58` records/spread, NSLink 3.0, O0: compilación 0; ejecución 42.
- Dump MIR de arreglo: offsets fuente 0, 4 y 8.
- Dump MIR de struct/with: offsets fuente 0 y 4.
- NativeSharp 3.0 T01 expresiones: 27/27.
- NativeSharp 3.0 T02 agregados: 13/13.
- Contratos CC1 dirigidos c55/c56/c58: 18/18.
- Acceso a bitfields, item 329: 15/15.
- Matriz ABI de agregados, item 342: 2/2.

## Riesgo controlado

El cambio no altera el valor de destino, tamaño, alineación, bit offset,
bit width ni lowering LLVM. Sólo corrige el dato que el backend ya consume como
offset de fuente dentro de `AggregateSpreadStore`. Las pruebas vecinas de
records, bitfields y ABI se ejecutaron explícitamente para comprobar que la
especialización por padre no afectó otras clases de descriptor.

## Resultado esperado en la próxima corrida

Los dos únicos fallos de fixtures del baseline anterior deben devolver 42. El
cierre reducido dejará de heredar esos fallos y podrá continuar con la
certificación, siempre que no aparezca otro defecto independiente.
