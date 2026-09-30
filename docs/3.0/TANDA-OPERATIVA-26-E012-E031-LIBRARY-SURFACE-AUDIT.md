# Tanda operativa 26 — E012–E016/E027–E031: auditor de superficie NSMETA

Fecha: 2026-09-29

Rama: `development`

Estado: superficies auditables; semántica no certificada

## Objetivo

Permitir que el compilador inventarie sobre artefactos `.nsmeta` reales si existen las superficies
nominales exigidas por bounded containers, texto, hashing, formatting y Unicode, sin recompilar
libraries y sin convertir la presencia de un tipo en certificación funcional.

Comando nuevo:

```text
nsc --audit-library-capabilities <metadata-directory>
```

## Contrato `NSLIBSURFACE 1.0`

- enumera recursivamente `.nsmeta` en orden determinista;
- exige que cada archivo sea estructuralmente legible por `LibraryMetadataCodec`;
- agrega tipos públicos por nombre desde todos los módulos;
- reporta `surface=present|missing` y cada evidencia/faltante para E012–E016/E027–E031;
- declara siempre `semantics-certified=false`.

El auditor no reemplaza `--verify-library-metadata`: puede inventariar un productor histórico aunque
la ventana de consumo del compilador actual lo rechace. Compatibilidad de consumo y presencia de API
son preguntas diferentes.

## Archivos modificados

- `CMakeLists.txt`: integra el auditor y su única prueba dirigida.
- `src/Driver/Driver.cpp`: comando CLI y ayuda.

## Archivos nuevos y por qué

- `include/nativesharp/Interop/LibraryCapabilityAudit.h`: modelo público de evidencia/faltantes.
- `src/Interop/LibraryCapabilityAudit.cpp`: reglas, agregación NSMETA, reporte y lectura de directorio.
- `tests/unit/NativeSharp3E012E031LibrarySurfaceTests.cpp`: siete comprobaciones fail-closed.
- `docs/3.0/TANDA-OPERATIVA-26-E012-E031-LIBRARY-SURFACE-AUDIT.md`: expediente de la tanda.

## Validación dirigida ejecutada

1. `nsc` Debug/O0 construido correctamente.
2. Target único `nsc_ns3_e012_e031_library_surface_tests`: `PASS: 7 checks`.
3. La CLI auditó en sólo lectura 26 NSMETA existentes bajo
   `A:\nativesharp-libraries\build\O0` sin recompilar libraries.
4. Resultado real: sólo E027 tiene completa su superficie nominal (`FormatWriter`,
   `IntegerFormatter`, `FloatFormatter`).
5. E031 conserva `RegexMatcher` como evidencia pero sigue missing por `UnicodeRegexData`.
6. E012–E016 y E028–E030 permanecen missing según sus tipos obligatorios.
7. Un directorio sin NSMETA falla causalmente, en vez de producir un reporte vacío verde.
8. Las carpetas temporales se eliminaron de forma dirigida.
9. `NSC_BUILD_TESTS` quedó restaurado a `OFF` y `nsc` se reconstruyó.

No se ejecutó baseline, CTest general, O1/O2/O3, build de libraries, instalación, deploy, debugger ni
remotas.

## Mejora concreta

El roadmap ya puede contrastarse automáticamente con artefactos, no con nombres encontrados en
source o afirmaciones históricas. Un tipo parcial aporta evidencia sin elevar todo su E### a verde.

## Deuda explícita

- implementar las superficies missing en sus repositorios de libraries;
- agregar métodos/constantes/ownership/capacity/allocator al gate, no sólo tipos;
- certificar E027 con golden vectors, errores, límites y benchmarks pese a su superficie presente;
- versionar tablas Unicode y probar normalización/grafemas/anchura/regex Unicode;
- ejecutar funcionalidad, corpus hostil, fuzzing, ABI y consumo externo antes de cualquier
  reclasificación `TARGETED-GREEN`.

La tanda 26 cierra el auditor del compilador. No declara implementados E012–E016 ni E027–E031.
