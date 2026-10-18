# Tanda NSC305 GATE-F02B — golden histórico auténtico NSMETA 1.39

## SCOPE LOCK

- objetivo autorizado: reconstruir con el productor histórico auténtico y versionar únicamente
  `historical-1.39.nsmeta` para que Stage 48.2 compruebe lectura y compatibilidad reales;
- naturaleza: fixture histórico y evidencia de procedencia, sin cambio funcional del compilador;
- revisión productora: `5aa5c0db66afb92a378ec4d49f2d2f2b226c8175`;
- archivos versionables autorizados: el golden 1.39 y este expediente;
- archivos y contratos protegidos: codec, header, versión, payload físico, ABI, NSMETA 1.0–1.38,
  fuentes de producto, expectativas y el trabajo ajeno del árbol;
- pruebas autorizadas: construcción aislada del productor histórico y de los targets focales,
  verificadores histórico/actual, Stage 48.2 y validadores estáticos de infraestructura;
- acciones prohibidas: worktrees administrados, otras unidades, MSYS, baseline, suite completa,
  deploy, instalación o reescritura de goldens;
- terminación: procedencia reproducible, encabezado 1.39 y SHA verificados, productor histórico y
  HEAD capaces de leer el artefacto, hashes 1.0–1.38 invariantes y Stage 48.2 con 398/398 PASS.

## Alcance

Esta hoja incorpora exclusivamente `historical-1.39.nsmeta`, producido por el compilador del
commit que publicó NSMETA 1.39. No modifica codec, formato, versión, proyección ABI, fixtures
anteriores ni expectativas de Stage 48.2.

## Procedencia inmutable

- commit productor: `5aa5c0db66afb92a378ec4d49f2d2f2b226c8175`;
- árbol Git: `fd8749a479290f01a1f6c55022b5e6880916d0ab`;
- archive: `A:\temp\gate-f02b-5aa5c0db-source.zip`;
- SHA-256 del archive:
  `b198dd55c232cb2d682281249208a7f99293f6b2143149d230c67818ef74b8e4`;
- fuente extraída: `A:\temp\gate-f02b-5aa5c0db-source`;
- build aislado: `A:\temp\gate-f02b-5aa5c0db-build`.

El árbol se obtuvo con `git archive`, no contiene `.git` en ningún nivel y `git rev-parse`
confirma que no es repositorio ni worktree administrado. El header exportado declara
`kMetadataFormatMinor = 39`. El build fijó la identidad reproducible
`3.0.0+build.19700101T000000Z.5aa5c0db66afb92a` y construyó el target `nsc` en 289 pasos.

## Fixture canónico

Se materializó literalmente el fixture que
`Stage48_2LibraryMetadataTests.cpp::testIntegrityAndCompatibility` usaba en ese commit:

- proyecto `Integrity`;
- `OutputType` igual a `StaticLibrary`;
- `Api.ns` idéntico al valor de `richSource()`;
- proyecto idéntico al XML producido por `writeProject`;
- acción `--emit-project-library-metadata`.

La fuente materializada tiene 2,096 bytes y SHA-256
`9d6bde7d7b1b6b3fd4f9e55cb396a5ccb5cda8c7408eb3f64356bf6d0fe6bfb1`. El proyecto tiene
162 bytes y SHA-256 `ddad8b210200d74a3d55236fa7f32c8c337a19b1c835e076640adcb5df4cd064`.
No se usó `NSC_P2_REFRESH_GOLDENS`: el productor histórico se invocó directamente sobre su
fixture canónico para generar el formato que en ese commit todavía era el actual.

## Construcción hermética

Fuentes, build, fixture, salida, temporales y logs permanecieron en `A:`. CMake, Ninja,
`clang-cl`, `lld-link`, `llvm-config` y NSLink se resolvieron desde el PATH nativo de Windows;
NSLink resolvió `A:\nslink-devel\bin\release\nslink.exe`. La configuración usó Ninja, Release,
LLVM 23.1.0, tests desactivados y construyó únicamente el target `nsc` histórico.

## Identidad del artefacto

- archivo: `tests/fixtures/nsmeta/historical-1.39.nsmeta`;
- tamaño: 9,853 bytes;
- magic: `NSMETA1\0` (`4e534d4554413100`);
- major: 1;
- minor: 39;
- endian sentinel: `0x01020304`;
- módulo: `Integrity`;
- target: `x86_64-pc-windows-msvc`;
- tipos: 10;
- miembros: 27;
- exportaciones: 3;
- SHA-256: `04d1f346c99dd16893c8a11b2216cb67229496e69edbd2586f0e57a9d98c5af2`;
- ABI fingerprint:
  `2b9f64fef76e1aa5826fa3cce4b4a55a07ced34a3fbd4aa7d9edbcb17a382e97`.

El productor histórico leyó y verificó el archivo con exit 0. El `nsc` reconstruido desde HEAD,
build `ad0208df4f0fa186`, también lo leyó y aceptó mediante `--verify-library-metadata`, que
ejecuta la verificación de compatibilidad del compilador actual.

## Inmutabilidad previa

Antes y después de incorporar 1.39 se calcularon los SHA-256 de todos los goldens 1.0–1.38. La
comparación normalizada cubrió 39 archivos y produjo cero diferencias. No se reescribió ni tocó
ningún snapshot anterior.

## Gate focal

Se construyó únicamente `nsc_stage48_2_library_metadata_tests` y se ejecutó exclusivamente
`nsc.stage48_2.library-metadata` con el runtime generado en A:, PATH Machine+User y NSLink por
nombre. El agregado completó `PASS: 398 checks`, CTest 1/1 y exit 0. También pasó el validador
estático de registro de fuentes. No se ejecutaron baseline, suite completa ni deploy.

## Evidencia persistente

- `A:\temp\gate-f02b-configure.log`;
- `A:\temp\gate-f02b-build-historical-nsc.log`;
- `A:\temp\gate-f02b-historical-version.log`;
- `A:\temp\gate-f02b-generate-1.39.log`;
- `A:\temp\gate-f02b-binary-identity.log`;
- `A:\temp\gate-f02b-historical-verify.log`;
- `A:\temp\gate-f02b-build-head-nsc.log`;
- `A:\temp\gate-f02b-head-version.log`;
- `A:\temp\gate-f02b-head-verify.log`;
- `A:\temp\gate-f02b-goldens-before.log`;
- `A:\temp\gate-f02b-goldens-after.log`;
- `A:\temp\gate-f02b-where-nslink.log`;
- `A:\temp\gate-f02b-build-stage48.log`;
- `A:\temp\gate-f02b-ctest-stage48.log`;
- `A:\temp\gate-f02b-ctest-stage48-verbose.log`;
- `A:\temp\gate-f02b-source-registration.log`.
