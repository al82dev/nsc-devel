# Tanda operativa 18 — Huellas separadas y `nsc --api-diff`

Fecha: 2026-09-29

Rama: `development`

Alcance: vertical productivo NS3-T12-F06 y parte verificable de F01/F03/F07

Estado de T12: `IMPLEMENTING`

## Objetivo

Convertir la huella API histórica de un helper aislado en una herramienta del compilador que opere
sobre artefactos NSMETA reales. La herramienta debe separar compatibilidad de fuente y ABI,
explicar cada diferencia, ignorar orden incidental y producir códigos de salida útiles para CI.

El comando nuevo es:

```text
nsc --api-diff <old.nsmeta> <new.nsmeta>
```

## Diagnóstico previo

- `ModuleSemantics::publicApiFingerprintInput` sólo concatenaba registros proporcionados a mano.
- El driver no ofrecía un comando de comparación API.
- NSMETA ya llevaba una huella ABI SHA-256 íntegra, pero una huella distinta no decía si el cambio
  era aditivo, incompatible en fuente, incompatible físicamente o ambas cosas.
- Parámetros nombrados, defaults, ownership y efectos estaban disponibles en metadata, pero no
  existía una proyección fuente independiente.
- Layouts, dispatch tables, exports y símbolos físicos estaban disponibles, pero no existía una
  proyección ABI explicable por identidad.
- Los helpers de imports, reexports, availability y friends continuaban desconectados del pipeline;
  esta tanda no los presenta como lenguaje implementado.

## Diseño implementado

### Dos superficies canónicas

`LibraryApiDiffer` construye mapas ordenados `identity -> contract` y los firma con SHA-256:

- `NativeSharp.source-api.v1`: nombres públicos/protected, firmas, parámetros nombrados, defaults,
  ownership, efectos, namespace functions/state, templates reutilizables y exports C;
- `NativeSharp.abi-surface.v1`: target/ABI versions, layouts, campos y bitfields, firmas físicas,
  sret/passes, mangled names, specializations, dispatch tables, descriptors y exports C.

El orden de registros y el build-id del productor no participan. La cabecera usa identidad de
módulo normalizada para Windows (case-insensitive y `\`/`/` equivalentes).

La huella ABI original de NSMETA también se imprime para trazabilidad, pero no se usa como un
booleano simplista de compatibilidad: agregar un método no virtual cambia el artefacto y su hash,
pero no rompe consumidores existentes.

### Clasificación estructural

Para cada superficie se comparan identidades estables:

- identidad nueva: `additive`;
- identidad eliminada: ruptura de esa superficie;
- misma identidad con contrato diferente: ruptura de esa superficie.

El resumen final puede ser `identical`, `additive`, `source-breaking`, `abi-breaking` o
`source-and-abi-breaking`. Un cambio ownership se clasifica como ruptura semántica de fuente sin
inventar un cambio físico; un cambio de layout privado puede ser ABI-breaking sin cambiar la
superficie fuente.

### Contrato CLI

- metadata ilegible o incompatible: diagnóstico causal y exit 1;
- diff idéntico o solamente aditivo: exit 0;
- cualquier ruptura fuente o ABI: reporte completo y exit 1.

Cada línea incluye impacto, dominio, identidad y operación (`added`, `removed`, `contract changed`).
La salida es estable y puede ser consumida por CI sin analizar texto fuente.

## Archivos modificados

- `CMakeLists.txt`: integra `LibraryApiDiff.cpp` en `nsc_driver` y enlaza el test T12 con el driver.
- `include/nativesharp/Driver/Driver.h`: declara la operación CLI de dos NSMETA.
- `src/Driver/Driver.cpp`: parsea, documenta y ejecuta `--api-diff`.
- `tests/unit/NativeSharp3T12ModuleTests.cpp`: amplía el test dirigido a 14 comprobaciones.
- `docs/3.0/TANDA-12-MODULOS-Y-EVOLUCION-DE-API.md`: actualiza el estado real de F06.

## Archivos nuevos y por qué

- `include/nativesharp/Interop/LibraryApiDiff.h`: contrato público del reporte, impactos y
  clasificador, reutilizable por CLI y tooling.
- `src/Interop/LibraryApiDiff.cpp`: proyecciones canónicas, SHA-256 y comparación estructural.
- `docs/3.0/TANDA-OPERATIVA-18-T12-API-DIFF.md`: expediente de alcance, evidencia y deuda.

No se añade una segunda copia persistida de las huellas a NSMETA 1.24: ambas se derivan de la
metadata verificada. Eso evita subir el formato antes de probar estabilidad y permite comparar
artefactos 2.0/3.0 dentro de la ventana que el codec ya acepta.

## Validación dirigida ejecutada

1. `nsc` Debug/O0 construido en `A:\nsc-devel\build-clang`: correcto.
2. Target único `nsc_ns3_t12_module_tests`: `PASS: 14 checks`.
3. NSMETA v1 contra sí misma: `classification: identical`, exit 0.
4. NSMETA v1 contra una versión con método estático no virtual nuevo:
   `classification: additive`, dos entradas aditivas, exit 0.
5. NSMETA v1 contra una versión que cambia campo/constructor/retorno de `int` a `long`:
   `classification: source-and-abi-breaking`, cambios fuente y ABI explicados, exit 1.
6. Pruebas unitarias separan ownership source-only, layout ABI-only, eliminación en ambos dominios,
   orden de registros e identidad de módulo Windows.
7. `NSC_BUILD_TESTS` quedó restaurado a `OFF`.

No se ejecutó baseline, CTest general, ninguna matriz O1/O2/O3, instalación, deploy ni remotas. El
compilador probado fue exclusivamente `A:\nsc-devel\build-clang\bin\nsc.exe`.

## Mejora concreta

NativeSharp ya puede responder con evidencia de artefacto si una revisión de biblioteca es
compatible en fuente, compatible físicamente o rompe una de las dos. Esto permite gates CI,
versionado y diagnóstico de consumidores sin comparar archivos `.ns` ni asumir que todo cambio de
hash es breaking.

## Deuda explícita

- F01: versiones de módulo, packages y ciclos del graph aún no tienen un contrato público completo;
- F02: imports explícitos, aliases, colisiones y unused diagnostics siguen usando la superficie 2.0;
- F03: module/package/friend no están conectados a parser, lookup y metadata;
- F04: no existe todavía sintaxis/productor de reexports o facades con clausura pública;
- F05: `deprecated/since/removed/target` no atraviesa parser, diagnostics, NSMETA y reexports;
- F06: falta persistir/versionar las dos proyecciones cuando su schema se congele y ampliar reglas
  de compatibilidad por clase de miembro;
- F07: ownership ya participa en el diff fuente, pero faltan contratos genéricos completos de
  move/copy/drop/lifetime/constraints sin fuente;
- incremental recompilation todavía no consume la huella fuente para decidir qué consumers rehacer;
- LibraryReference/ProjectReference, reexports, LSP y docs requieren gates de las tandas 29–30.

Por estas deudas, T12 permanece `IMPLEMENTING`. La tanda operativa 18 queda cerrada cuando este
vertical esté commiteado porque F06 ya opera de extremo a extremo sobre NSMETA real y falla de
forma explicable, sin afirmar que imports/reexports/deprecation ya existen.
