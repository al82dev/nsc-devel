# NS3-T06-F05 — Cleanup determinista / Deterministic cleanup

## Resultado

NativeSharp 3.0 incorpora `defer` con capturas explícitas `ref`/`move` y lo baja al mismo grafo de
cleanup que ya gobierna `finally` y la destrucción RAII. El orden es LIFO y cubre salida normal,
`return`, `break`, `continue` y unwind sin crear un runtime paralelo.

## Cambios principales

- El parser reconoce `defer [captures] { body }` exclusivamente en edición 3.
- El bloque se transforma en una continuación `try/finally`; defers sucesivos quedan anidados.
- `move` toma el valor al registrar; para owners transfiere ownership y para valores Copy conserva
  un snapshot. `ref` observa el storage al ejecutar el cleanup.
- El análisis semántico impide leer locales, parámetros o el receiver exterior sin captura.
- Los owners capturados se destruyen exactamente una vez después del cuerpo diferido.
- Los exits estructurados conservan sus razones de cleanup existentes.

## Archivos nuevos

- `tests/unit/NativeSharp3T06DeterministicCleanupTests.cpp`: prueba dirigida O0/NSLink de orden,
  exits, captura move-only, diagnósticos y edición.
- `docs/3.0/language/deterministic-cleanup-and-defer.md`: contrato normativo bilingüe.
- Este documento: mapa y evidencia del commit.

## Archivos modificados

- Parser: `include/nativesharp/Parser/Parser.h` y `src/Parser/Parser.cpp`.
- Frontend: `include/nativesharp/Frontend/ProjectMethodBodyModel.h` y
  `src/Frontend/ProjectMethodBodyModel.cpp`.
- Registro de prueba: `CMakeLists.txt`.
- Roadmap, tanda 06 y tracking maestro.

## Mejora observable

El usuario puede ligar cleanup directamente al alcance sin escribir manualmente `try/finally`,
con semántica visible y estable para referencias y ownership. El lowering reutiliza infraestructura
existente, no añade heap, objeto runtime, layout ni cambio de calling convention.

## Validación dirigida

- `nsc.ns3.t06.deterministic-cleanup`: 7/7 checks en O0 con NSLink.
- Ejecución LIFO por fallthrough, `return`, `continue` y `break`.
- Captura move-only y destrucción exactamente una vez.
- Dump de cuerpos con región `finally` y razones `return`, `break` y `continue`.
- Rechazos de captura omitida, receiver implícito, duplicado, uso posterior de owner moved y
  edición 2.
- Baseline, suites y matrices: no ejecutados por prohibición expresa del proyecto.
