# Tanda NSC305 GATE-F02A — golden histórico auténtico NSMETA 1.38

## SCOPE LOCK

- objetivo autorizado: reconstruir con el productor histórico auténtico y versionar únicamente
  `historical-1.38.nsmeta` para que Stage 48.2 compruebe lectura y compatibilidad reales;
- naturaleza: fixture histórico y evidencia de procedencia, sin cambio funcional del compilador;
- revisión productora: `2bcaa47aa614d0e5b1c0957ec462ae745202af31`;
- archivos versionables autorizados: el golden 1.38 y este expediente;
- archivos y contratos protegidos: codec, header, versión, payload físico, ABI, NSMETA 1.0–1.37,
  golden 1.39, fuentes de producto, expectativas y el trabajo ajeno del árbol;
- pruebas autorizadas: construcción aislada del productor histórico y de los targets focales,
  verificadores histórico/actual, Stage 48.2 y validadores estáticos de infraestructura;
- acciones prohibidas: worktrees administrados, otras unidades, MSYS, baseline, suite completa,
  deploy, instalación, reescritura de goldens o corrección de F02B;
- terminación: procedencia reproducible, encabezado 1.38 y SHA verificados, productor histórico y
  HEAD capaces de leer el artefacto, hashes 1.0–1.37 invariantes y Stage 48.2 sin fallos F02A.

## Alcance

Esta hoja incorpora exclusivamente `historical-1.38.nsmeta`, producido por el compilador del
commit que publicó NSMETA 1.38. No modifica codec, formato, versión, proyección ABI, fixtures
anteriores ni expectativas de Stage 48.2.

## Procedencia inmutable

- commit productor: `2bcaa47aa614d0e5b1c0957ec462ae745202af31`;
- árbol Git: `af2c3a97147ce1f6946d1e458e53dceea95d3abc`;
- archive: `A:\temp\gate-f02a-2bcaa47aa-source.zip`;
- SHA-256 del archive:
  `b357348683a3af1c80186966bb4d340a6819d4a8c4e3b3ac4040fad50c20f7dd`;
- fuente extraída: `A:\temp\gate-f02a-2bcaa47aa-source`;
- build aislado: `A:\temp\gate-f02a-2bcaa47aa-build`.

El árbol se obtuvo con `git archive`, no contiene `.git` en ningún nivel y `git rev-parse`
confirma que no es repositorio ni worktree administrado. El header exportado declara
`kMetadataFormatMinor = 38`. El build fijó la identidad reproducible
`3.0.0+build.19700101T000000Z.2bcaa47aa614d0e5`.

## Fixture canónico

Se materializó literalmente el fixture que
`Stage48_2LibraryMetadataTests.cpp::testIntegrityAndCompatibility` usaba en ese commit:

- proyecto `Integrity`;
- `OutputType` igual a `StaticLibrary`;
- `Api.ns` idéntico al valor de `richSource()`;
- proyecto idéntico al XML producido por `writeProject`;
- acción `--emit-project-library-metadata`.

Las comparaciones normalizadas dieron igualdad exacta: 2,021 caracteres de fuente y el XML
completo del proyecto. No se usó `NSC_P2_REFRESH_GOLDENS`: en el productor 1.38 ese mecanismo
sólo archivaba minors estrictamente menores que el formato actual, por lo que no podía autenticar
1.38. El productor histórico se invocó directamente sobre su fixture canónico.

## Construcción hermética

Fuentes, build, fixture, salida, temporales y logs permanecieron en `A:`. CMake, Ninja,
`clang-cl`, `lld-link`, `llvm-config` y NSLink se resolvieron desde el PATH nativo de Windows;
NSLink resolvió `A:\nslink-devel\bin\release\nslink.exe`. La configuración usó Ninja, Release,
LLVM 23.1.0, tests desactivados y construyó únicamente el target `nsc`. El compilador medido y el
runtime generado también permanecieron en el build aislado de `A:\temp`.

## Identidad del artefacto

- archivo: `tests/fixtures/nsmeta/historical-1.38.nsmeta`;
- tamaño: 9,849 bytes;
- magic: `NSMETA1\0`;
- major: 1;
- minor: 38;
- módulo: `Integrity`;
- target: `x86_64-pc-windows-msvc`;
- SHA-256: `3ff1c15d76ada8e460e0020ad17aae9e601691cdee96d941a5cadac963788076`;
- ABI fingerprint:
  `2b9f64fef76e1aa5826fa3cce4b4a55a07ced34a3fbd4aa7d9edbcb17a382e97`.

El productor histórico leyó y verificó el archivo con exit 0. El `nsc` reconstruido desde HEAD
también lo leyó y aceptó mediante `--verify-library-metadata`, que ejecuta la verificación de
compatibilidad del compilador actual.

## Inmutabilidad previa

Antes y después de incorporar 1.38 se calcularon los SHA-256 de todos los goldens 1.0–1.37. La
comparación produjo cero diferencias. No se reescribió ni tocó ningún snapshot anterior.

## Gate focal

Se construyó únicamente `nsc_stage48_2_library_metadata_tests` y se ejecutó exclusivamente
`nsc.stage48_2.library-metadata` con el runtime generado en A:, PATH Machine+User y NSLink por
nombre. El agregado completó 398 checks: 396 PASS y dos FAIL. Los dos fallos restantes son
exactamente la lectura y compatibilidad del ausente `historical-1.39.nsmeta`, ambos propiedad de
GATE-F02B. El exit no es un fallo de F02A: CTest permanece rojo mientras exista esa hoja vecina.
No se ejecutaron baseline, suite completa ni deploy.

## Evidencia persistente

- `A:\temp\gate-f02a-configure.log`;
- `A:\temp\gate-f02a-build-historical-nsc.log`;
- `A:\temp\gate-f02a-historical-version.log`;
- `A:\temp\gate-f02a-generate-1.38.log`;
- `A:\temp\gate-f02a-binary-identity.log`;
- `A:\temp\gate-f02a-historical-verify.log`;
- `A:\temp\gate-f02a-build-head-nsc.log`;
- `A:\temp\gate-f02a-head-version.log`;
- `A:\temp\gate-f02a-head-verify.log`;
- `A:\temp\gate-f02a-goldens-before.log`;
- `A:\temp\gate-f02a-goldens-after.log`;
- `A:\temp\gate-f02a-where-nslink.log`;
- `A:\temp\gate-f02a-build-stage48.log`;
- `A:\temp\gate-f02a-ctest-stage48.log`.
