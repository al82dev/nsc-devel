# Tanda operativa 27 — E036–E044: auditoría de superficie científica

Fecha: 2026-09-29

Rama: `development`

Estado: superficies auditables; algoritmos, exactitud y rendimiento no certificados

## Objetivo

Extender el contrato `NSLIBSURFACE 1.0` del compilador para que los artefactos `.nsmeta` revelen,
sin recompilar bibliotecas, la presencia o ausencia de las superficies públicas previstas para
matrices sparse, complejos, precisión arbitraria, unidades, intervalos, ODE, Fourier y
vectorización medida.

El comando sigue siendo:

```text
nsc --audit-library-capabilities <metadata-directory>
```

## Cambios funcionales

El reporte incluye ahora requisitos explícitos para:

- E036: `CooMatrix`, `CsrMatrix` y `CscMatrix`;
- E037: operaciones sparse, solver iterativo, precondicionador e informe de convergencia;
- E038: tipo de valor `Complex` y algoritmos `ComplexMath`;
- E039: `BigInteger`, `BigFloat` y límites explícitos;
- E040: dimensiones, cantidades y conversión de unidades;
- E041: `IntervalSet` e `IntervalMap`;
- E042: solver ODE, Euler controlado y Runge–Kutta;
- E043: transformadas Fourier/FFT y convolución;
- E044: evidencia pública de benchmark de vectorización y fallback escalar.

La presencia nominal continúa separada de la certificación semántica mediante
`semantics-certified=false`. En particular, una clase de algoritmos complejos no sustituye al tipo
de valor público exigido, y un benchmark no sustituye al fallback escalar.

## Archivos modificados

- `src/Interop/LibraryCapabilityAudit.cpp`: agrega las nueve reglas E036–E044 al contrato común.
- `tests/unit/NativeSharp3E012E031LibrarySurfaceTests.cpp`: conserva y comprueba las diez reglas
  originales dentro del inventario ampliado.
- `CMakeLists.txt`: registra únicamente la prueba dirigida nueva.

## Archivos nuevos y por qué

- `tests/unit/NativeSharp3E036E044ScientificSurfaceTests.cpp`: ocho comprobaciones de requisitos
  completos, parciales y fail-closed sin depender de las bibliotecas externas.
- `docs/3.0/TANDA-OPERATIVA-27-E036-E044-SCIENTIFIC-SURFACE-AUDIT.md`: expediente reproducible de
  esta tanda.

## Validación dirigida ejecutada

1. `nsc` y los dos targets afectados se construyeron en Debug/O0.
2. `nsc_ns3_e012_e031_library_surface_tests`: `PASS: 7 checks`.
3. `nsc_ns3_e036_e044_scientific_surface_tests`: `PASS: 8 checks`.
4. La CLI auditó en sólo lectura los 26 NSMETA existentes en
   `A:\nativesharp-libraries\build\O0`.
5. E038 encontró `ComplexMath`, pero permanece missing porque no existe `Complex` público.
6. E036, E037 y E039–E044 permanecen missing según sus contratos públicos.
7. `NSC_BUILD_TESTS` se restauró a `OFF` y `nsc` se reconstruyó.

No se ejecutó baseline, CTest general, O1/O2/O3, compilación de libraries, instalación, deploy,
debugger ni operaciones remotas.

## Mejora concreta

El compilador puede contrastar el bloque científico del roadmap contra artefactos consumibles y
producir una lista determinista de faltantes. Esto evita confundir archivos fuente, comentarios,
tipos auxiliares o carriers SIMD con una API científica terminada.

## Deuda explícita

- implementar cada superficie faltante en los repositorios de bibliotecas correspondientes;
- ampliar el gate desde nombres hacia métodos, layouts, ownership, errores y compatibilidad;
- validar E036–E043 con oráculos externos independientes y corpus numéricos hostiles;
- exigir para E044 benchmarks reproducibles, ganancia neta, equivalencia escalar/SIMD y fallback;
- ejecutar ABI, consumo cross-module, fuzzing y matrices numéricas antes de cambiar estados a
  `TARGETED-GREEN`.

La tanda 27 cierra el mecanismo de auditoría del compilador. No declara implementadas E036–E044.
