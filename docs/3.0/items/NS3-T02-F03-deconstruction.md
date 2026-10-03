# NS3-T02-F03 — Deconstruction

## Estado

TERMINADA.

## Qué se implementó

- Declaraciones `var (x, y) = source` y asignaciones `(x, y) = source` para tuplas de aridad
  completa, incluidas `()` y `(T,)`.
- Patrones anidados en declaración y asignación, con descartes `_` en cualquier hoja.
- Protocolo nominal estático mediante un único `Deconstruct()` de instancia, accesible,
  paramétrico-cero, no virtual/no abstracto y con retorno tupla por valor.
- Binding semántico persistente de la fuente y del miembro nominal seleccionado; HIR no repite
  lookup ni resolución.
- Fuente evaluada una vez, snapshot completo antes de destinos y stores hoja por hoja en orden
  izquierda→derecha.
- Rechazo causal de formas/aridades incompatibles, hojas no direccionables, conversiones fallidas,
  firmas nominales inválidas, retornos `ref` y copias ocultas de temporales move-only.

## Archivos nuevos

- `tests/unit/NativeSharp3T02DeconstructionTests.cpp`: seis checks dirigidos que compilan O0,
  enlazan con NSLink, ejecutan el PE y verifican negativas semánticas.
- `docs/3.0/language/deconstruction.md`: contrato bilingüe del lenguaje.
- Este documento: evidencia del ítem y de su validación.

## Archivos modificados

- `include/nativesharp/Parser/Parser.h`, `src/Parser/Parser.cpp`: parser recursivo de patrones.
- `include/nativesharp/Frontend/ProjectMethodBodyModel.h`,
  `src/Frontend/ProjectMethodBodyModel.cpp`: binding nominal, validación recursiva y locals hoja.
- `include/nativesharp/Frontend/ProjectOopHirModel.h`,
  `src/Frontend/ProjectOopHirModel.cpp`: llamada nominal y offsets absolutos de hojas anidadas.
- `CMakeLists.txt`: target dirigido `nsc_ns3_t02_deconstruction_tests`.
- Documentos históricos T02: se retira la frontera ya cerrada de `Deconstruct` nominal.

## Mejora

La deconstrucción dejó de ser una ruta plana exclusiva de tuplas. Ahora es un contrato completo de
frontend a LLVM: forma recursiva, protocolo nominal sin reflexión, evaluación única, aliasing
seguro, orden observable y frontera explícita de ownership sin copias ocultas.

## Validación dirigida

- `nsc` compila correctamente.
- `nsc_ns3_t02_deconstruction_tests`: PASS 6/6 con productos NativeSharp O0 y NSLink local.
- Regresiones vecinas: aggregates 13/13, native tuples 24/24, tuple literals 4/4 y method-body
  model 58/58.
- No se ejecutó baseline, CTest agregado, optimizaciones O1/O2/O3, instalación ni despliegue.
