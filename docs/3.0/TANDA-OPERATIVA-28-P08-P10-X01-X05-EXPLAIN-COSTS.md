# Tanda operativa 28 — P08–P10/X01–X05: contratos de optimización y `nsc explain`

Fecha: 2026-09-29

Rama: `development`

Estado: contrato O0–O3 y explorador de costos productivos; capacidades transversales restantes
reportadas sin falsos verdes

## Objetivo

Hacer observables dos decisiones que antes estaban dispersas: la identidad exacta de los pipelines
O0–O3 que participa en caché, y los costos ocultos que ya existen en el MIR de cada función.
Además, introducir `nsc explain` como entrada estable para explicar build, caché, semántica, API,
ABI, sesión, paquetes y debug allocator con su estado real.

## Cambios funcionales

### `NSOPTIMIZATION 1.0`

`nsc explain optimization` publica los cuatro pipelines, su versión, intención, estabilidad ABI y
regla de equivalencia observable. Tanto la clave previa a LLVM como la clave del objeto optimizado
incluyen ahora nombre y versión del pipeline; sus schemas avanzaron de 2 a 3 para invalidar entradas
que fueron calculadas antes de este contrato.

### `NSCOST 1.0`

`nsc explain costs <file.nsproj>` compila únicamente hasta MIR y reporta por función:

- límite inferior estático de stack;
- sitios visibles de heap, copies, moves y drops;
- despacho virtual/interface/delegate;
- locks/primitivas de sincronización y atomics;
- unwind/personality;
- monomorfizaciones;
- cadena causal basada en el nodo MIR o región de cleanup que originó el costo.

`--output-format=json` entrega el mismo modelo de forma determinista para tooling. El término
`mir-static-lower-bound` evita presentar el reporte como medición runtime o tamaño final de frame.

### Temas adicionales de `nsc explain`

Los temas `build`, `cache`, `overload`, `trait`, `effect`, `api`, `abi`, `session`, `packages` y
`debug-allocator` declaran `available`, `partial` o `missing`, junto con evidencia presente y deuda.
En particular, no se confunde visibilidad de alloc/drop en MIR con un debug allocator existente.

## Archivos modificados

- `include/nativesharp/LLVM/LlvmOptimizationLevel.h` y
  `src/LLVM/LlvmOptimizationLevel.cpp`: versión y manifiesto de pipelines.
- `src/LLVM/CompilationArtifactCache.cpp`: identidad de pipeline en ambas claves de caché.
- `src/Driver/Driver.cpp`: acciones internas de contrato/costos y ayuda CLI.
- `src/Driver/ProjectCommands.cpp`: comando público `nsc explain` y temas honestos.
- `CMakeLists.txt`: integra el explorador y su prueba dirigida.

## Archivos nuevos y por qué

- `include/nativesharp/Frontend/ProjectCostExplorer.h`: modelo estable del reporte por función.
- `src/Frontend/ProjectCostExplorer.cpp`: recorrido estructural de MIR, totales y serialización
  text/JSON.
- `tests/unit/NativeSharp3P08P10ExplainTests.cpp`: ocho comprobaciones aisladas del contrato,
  costos, causalidad, JSON y routing CLI.
- `docs/3.0/TANDA-OPERATIVA-28-P08-P10-X01-X05-EXPLAIN-COSTS.md`: expediente de esta tanda.

## Validación dirigida ejecutada

1. `nsc` y `nsc_ns3_p08_p10_explain_tests` construidos en Debug/O0.
2. Prueba única: `PASS: 8 checks`.
3. `nsc explain optimization` emitió los cuatro pipelines `default<O0>`–`default<O3>` versión 1.
4. `nsc explain costs tests/cc1/c68_ns3_integer_range/c68_ns3_integer_range.nsproj
   --output-format=json` produjo `NSCOST 1.0` con cuatro funciones.
5. Un probe inicial sobre un ejemplo heredado incompatible con edición 3 falló causalmente; se
   reemplazó por c68 y se eliminó únicamente su `error7.log` residual.
6. `error.log`–`error6.log` se conservaron porque no pertenecen a ese probe; `error6.log` registra
   una compilación externa concurrente con el compilador 2.0.
7. `NSC_BUILD_TESTS` quedó restaurado a `OFF` y `nsc` se reconstruyó.

No se ejecutó baseline, CTest general, O1/O2/O3, libraries, instalación, deploy, debugger ni
operaciones remotas.

## Mejora concreta

Cambiar la composición de un pipeline ya no puede reutilizar silenciosamente un objeto de caché
con identidad vieja. Al mismo tiempo, autores y tooling pueden localizar costos por función desde
la representación semántica que realmente consume el backend, sin buscar texto fuente ni asumir
que toda llamada o construcción asigna memoria.

## Deuda explícita

- certificar equivalencia observable y ABI O0–O3 en el gate final autorizado;
- agregar tamaños finales de frame/assembly y comparar MIR contra código manual;
- convertir candidatos overload/traits/effects en cadenas causales completas;
- entregar el compiler server persistente aislable/cancelable de X01;
- completar `test`, `pack` y `publish` del workflow de paquetes X03;
- implementar el runtime debug allocator de X04;
- conectar el JSON de costos a LSP/VS Code fuera de este repositorio;
- medir link/PDB/NSMETA/package y determinismo de rutas bajo P10.

La tanda 28 cierra el vertical de observabilidad dentro del compilador; no declara completados
X01, X03, X04 ni la certificación cuantitativa de P08–P10.
