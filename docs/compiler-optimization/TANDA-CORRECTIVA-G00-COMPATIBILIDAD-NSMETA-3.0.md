# Tanda correctiva G00: compatibilidad NSMETA de NativeSharp 3.0

## Problema

La implementacion activa ya escribia NSMETA 1.25, Runtime ABI 1.1 y MIR 1.21, pero la ventana
de productores y varias pruebas continuaban congeladas en el cierre de NativeSharp 2.0:

- el codec aceptaba `1.0.1` como N-1 y rechazaba bibliotecas `2.0.0`;
- la matriz declaraba compilador 2.0/1.0.1, NSMETA 1.22/1.20 y Runtime ABI 1.0/1.0;
- faltaban los snapshots inmutables NSMETA 1.22, 1.23 y 1.24;
- varias expectativas conservaban Runtime ABI 1.0, la huella ABI anterior y el diagnostico
  previo a la resolucion de cierre binario.

## Cambios

- `include/nativesharp/Basic/Version.h`: agrega una fuente canonica para el productor N-1,
  `kPreviousCompilerReleaseVersion = "2.0.0"`.
- `src/Interop/LibraryMetadata.cpp`: acepta exclusivamente productores 3.0 y 2.0; 1.0.1 sigue
  siendo legible como bytes historicos, pero ya es N-2 y no es compatible.
- `src/Driver/CompatibilityPolicy.cpp`: publica la matriz real 3.0/2.0, NSMETA 1.25/1.21,
  Native ABI 1.0/1.0 y Runtime ABI 1.1/1.0. Los valores actuales se derivan de constantes.
- `tests/unit/Item393CompatibilityMatrixTests.cpp` y
  `tests/unit/Item405FinalCompatibilityCertificationTests.cpp`: validan la ventana 3.0.
- `tests/unit/Stage48_2LibraryMetadataTests.cpp`: alinea ABI, huella dorada, diagnostico de
  dependencia binaria, ventana N-2, salida del driver y compatibilidad historica.
- `tests/fixtures/nsmeta/historical-1.22.nsmeta`, `historical-1.23.nsmeta` y
  `historical-1.24.nsmeta`: snapshots binarios faltantes, creados mediante el mecanismo oficial
  `NSC_P2_REFRESH_GOLDENS` sin reescribir los snapshots anteriores.

No se modifico ninguna instalacion en C: ni se ejecuto el baseline.

## Mejora

Un compilador 3.0 puede consumir metadata de su predecesor real 2.0, rechaza de forma explicita
productores N-2 y conserva evidencia binaria de lectura para cada minor anterior a NSMETA 1.25.
La matriz publica coincide con lo que el codec aplica y deja visible el avance de Runtime ABI.

## Validacion especifica

- Target `nsc` construido en `A:\nsc-devel\build-clang`.
- `--print-compatibility-matrix` publica 3.0/2.0, 1.25/1.21, 1.0/1.0 y 1.1/1.0.
- Matriz N/N-1: 17/17.
- Certificacion de matriz: 16/16.
- Metadata Stage 48.2: 321/321.
- Los tres snapshots nuevos fueron leidos y verificados por la misma ejecucion.
- El directorio temporal dirigido en A: fue eliminado al terminar.
- No se ejecuto CTest general ni el baseline.
