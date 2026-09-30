# Tanda operativa 24 — T11/E008–E011: manifiesto productivo de comptime

Fecha: 2026-09-29

Rama: `development`

Alcance: observabilidad reproducible de `#run` en la compilación real de proyectos

Estado de T11: `IMPLEMENTING`

Estado de E008–E011: `PARTIAL` / `DISCOVERED`

## Objetivo

Conservar y publicar la evidencia que el motor productivo de `#run` ya calcula por cada fuente. Antes
de esta tanda, el compilador evaluaba expresiones, usaba un cache compartido y producía digests, pero
descartaba pasos, dependencias y estadísticas antes de terminar la fase de syntax.

El comando nuevo es:

```text
nsc --dump-project-comptime <file.nsproj>
```

## Diagnóstico previo

- `ComptimeEngine` ya reemplaza `#run(integer-expression)` antes del lexer dentro de
  `ProjectCompilation`; no era sólo un helper aislado.
- El motor ya impone límites de fuente, directivas, pasos y nesting, prohíbe acceso host y produce
  un digest SHA-256 reproducible.
- La cache de expresiones ya se comparte entre las fuentes procesadas en una compilación.
- `ProjectCompilation` movía el source transformado al `SourceManager` y perdía el resto del
  `ComptimeResult`.
- Los helpers de introspection, attributes, derive e higiene de T11 siguen aislados; macros y
  source mapping productivos no existen todavía.
- En libraries existen `BinaryReader`, `BinaryWriter` y serializers específicos, pero no se encontró
  un paquete `NativeSharp.Serialization` ni el contrato completo E008–E011. Esta tanda no modifica
  ni compila bibliotecas.

## Manifiesto `NSCOMPTIME 1.0`

El reporte incluye totales y una entrada determinista por fuente con:

- ordinal de proyecto y fuente;
- módulo y ruta relativa al proyecto, nunca la raíz absoluta del checkout;
- número de evaluaciones y pasos ejecutados;
- cache hits y misses;
- digest SHA-256 de reproducibilidad;
- tipo, identidad por offset y digest de cada dependencia `source-expression`.

La compilación mantiene un registro incluso para archivos sin `#run`. Esto permite demostrar que el
inventario cubre todo el source set, en lugar de mostrar sólo resultados positivos.

## Archivos modificados

- `include/nativesharp/Frontend/ProjectCompilation.h`: modelo y accessor de registros comptime.
- `src/Frontend/ProjectCompilation.cpp`: conserva evidencia de cada `ComptimeResult` y normaliza la
  ruta relativa de fuente.
- `include/nativesharp/Frontend/ProjectCompilationDumper.h`: superficie del manifiesto.
- `src/Frontend/ProjectCompilationDumper.cpp`: serialización textual determinista `NSCOMPTIME 1.0`.
- `src/Driver/Driver.cpp`: comando público y ayuda.
- `CMakeLists.txt`: target de prueba aislado y ruta de fixture obligatoriamente dentro de A:.
- `docs/3.0/TANDA-11-METAPROGRAMACION-ESTATICA.md`: integración real y deuda restante.

## Archivos nuevos y por qué

- `tests/unit/NativeSharp3T11ComptimeIntegrationTests.cpp`: ocho comprobaciones sobre el pipeline
  real, rutas reproducibles, cache, dependencias y manifiesto.
- `tests/fixtures/ns3_t11_comptime/Comptime.nsproj`: proyecto mínimo para validar la CLI pública.
- `tests/fixtures/ns3_t11_comptime/Main.ns`: dos expresiones idénticas que hacen observable un miss
  y un hit sin entradas externas.
- `docs/3.0/TANDA-OPERATIVA-24-T11-COMPTIME-MANIFEST.md`: expediente de cambios, pruebas y deuda.

## Validación dirigida ejecutada

1. `nsc` Debug/O0 construido correctamente.
2. Target histórico `nsc_ns3_t11_metaprogramming_tests`: `PASS: 8 checks`.
3. Target nuevo `nsc_ns3_t11_comptime_integration_tests`: `PASS: 8 checks`.
4. La integración productiva comprobó dos registros de fuente, rutas relativas y digest de 64
   dígitos hexadecimales.
5. Dos expresiones idénticas produjeron un cache miss, un cache hit y dos dependencias con el mismo
   digest de contenido.
6. La CLI publicó `NSCOMPTIME 1.0`, dos evaluaciones, 22 pasos, un hit, un miss y `path=Main.ns`.
7. La carpeta temporal de ejecución CLI fue eliminada de forma dirigida.
8. `NSC_BUILD_TESTS` quedó restaurado a `OFF` y `nsc` se reconstruyó en ese estado.

No se ejecutó baseline, CTest general, O1/O2/O3, libraries, instalación, deploy, debugger ni remotas.

## Mejora concreta

Los costos y dependencias de `#run` ya no son invisibles. Un proyecto puede auditar qué se evaluó,
cuánto trabajo consumió, qué se reutilizó y qué digest debe participar en incrementalidad, sin
introducir reflection runtime ni rutas dependientes de la máquina.

## Deuda explícita

- conectar introspección tipada, atributos y derive al frontend/NSMETA reales;
- implementar macros higiénicas con spans y backtrace de expansión;
- definir capabilities explícitas para archivos/env y registrar sus contenidos en el digest;
- publicar diagnósticos de usuario con códigos y spans sin colisionar con `NS3xxx`;
- aplicar budgets configurables de memoria, recursión y cancelación en toda evaluación;
- integrar el manifiesto con cache incremental persistente y comparar builds en raíces distintas;
- E008: completar `SpanReader`, codecs y endian explícito en libraries;
- E009–E011: schemas/serializers versionados, tipos compuestos/referencias, corpus hostil, fuzzing,
  límites, benchmarks y paquete `NativeSharp.Serialization`.

Por estas deudas T11 sigue `IMPLEMENTING`. E008 es parcial por piezas existentes de libraries y
E009–E011 permanecen abiertas. La tanda 24 cierra la observabilidad productiva de `#run`, no declara
terminada la metaprogramación ni la serialización.
