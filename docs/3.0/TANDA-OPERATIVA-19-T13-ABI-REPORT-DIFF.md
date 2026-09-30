# Tanda operativa 19 — Reporte y compatibilidad ABI desde NSMETA

Fecha: 2026-09-29

Rama: `development`

Alcance: vertical productivo NS3-T13-F01/F02/F04/F06 y observabilidad parcial de F08

Estado de T13: `IMPLEMENTING`

## Objetivo

Hacer observable y comparable el ABI que el compilador realmente publica para Windows x64, sin
depender de helpers aislados ni de inspección manual del binario. Los comandos nuevos son:

```text
nsc --dump-library-abi <file.nsmeta>
nsc --abi-diff <old.nsmeta> <new.nsmeta>
```

Ambos consumen metadata NSMETA verificada por el compilador actual. No aceptan descripciones ABI
inventadas ni reconstruyen contratos desde el código fuente.

## Diagnóstico previo

- NSMETA ya conservaba layouts, campos, bitfields, convenciones de delegate, firmas físicas,
  exports C, tablas de despacho y entradas de destrucción.
- `nsc --api-diff` separaba cambios fuente y ABI, pero no existía un reporte humano estable del
  contrato físico completo.
- La huella ABI del artefacto podía detectar que algo cambió, pero no explicar qué identidad o
  contrato físico era responsable.
- El helper histórico `AbiSemantics` validaba reglas locales; no demostraba que esas decisiones
  llegaran al artefacto de biblioteca.

## Diseño implementado

### Reporte ABI canónico

`LibraryAbiReporter` ordena por identidad estable y publica:

- módulo, target Windows x64, tamaño/alineación de puntero y versiones de cada ABI;
- huella de superficie ABI derivada y huella ABI íntegra de NSMETA;
- tipos, representación, tamaño, alineación, header, referencia, stable type id, base e interfaces;
- offsets, tamaños, alineaciones y coordenadas exactas de bitfields;
- delegates managed/Win64 C/vectorcall;
- miembros, convención de llamada, ownership físico, mangled name y parámetros `value/ref/out/in`;
- tablas de despacho, slots, tablas de interfaz y entrada de destrucción cross-module;
- exports C con nombre físico y tipos de frontera.

El orden incidental de los registros en NSMETA no altera el reporte.

### Diff exclusivamente ABI

`--abi-diff` reutiliza la comparación estructural de `LibraryApiDiffer`, pero filtra únicamente el
dominio ABI. Un cambio sólo semántico de fuente, como ownership sin cambio físico, permanece ABI
compatible. Un cambio de layout se reporta como `abi-breaking` y devuelve exit 1.

Clasificaciones:

- `identical`: no existen cambios ABI;
- `additive`: sólo se agregan identidades ABI compatibles;
- `abi-breaking`: se elimina o cambia un contrato físico existente.

Cada cambio breaking conserva identidad y explicación; CI no necesita comparar texto fuente ni
interpretar una huella opaca.

## Archivos modificados

- `CMakeLists.txt`: integra el reporter en `nsc_driver`, permite que el test T13 pruebe metadata y
  diff reales y corrige el compilador de pruebas predeterminado al `nsc` del mismo árbol de build;
  una instalación externa sólo puede elegirse mediante override explícito.
- `include/nativesharp/Driver/Driver.h`: declara las dos operaciones CLI.
- `src/Driver/Driver.cpp`: parsea, documenta y ejecuta `--dump-library-abi` y `--abi-diff`.
- `tests/unit/NativeSharp3T13AbiTests.cpp`: amplía el test dirigido de 8 a 13 comprobaciones.
- `docs/3.0/TANDA-13-LAYOUT-ABI-FFI-Y-UNSAFE.md`: corrige el estado histórico y registra la
  integración productiva y su deuda.

## Archivos nuevos y por qué

- `include/nativesharp/Interop/LibraryAbiReport.h`: contrato reutilizable del reporte ABI.
- `src/Interop/LibraryAbiReport.cpp`: serialización textual estable de la superficie física real.
- `docs/3.0/TANDA-OPERATIVA-19-T13-ABI-REPORT-DIFF.md`: expediente de alcance, evidencia, archivos
  y deuda de esta tanda.

El reporter se mantiene separado del codec NSMETA: leer/verificar metadata y presentarla son
responsabilidades distintas, y tooling futuro puede consumirlo sin duplicar el formato binario.

## Validación dirigida ejecutada

1. `nsc` Debug/O0 construido en `A:\nsc-devel\build-clang`: correcto.
2. Target único `nsc_ns3_t13_abi_tests`: `PASS: 13 checks`.
3. Biblioteca testigo real emitida a NSMETA con struct bitfield, delegate `vectorcall`, clase con
   destructor y export C.
4. `--dump-library-abi` publicó coordenadas bitfield, `vectorcall`, `win64-c`, fingerprints y la
   entrada de destrucción.
5. `--abi-diff` del artefacto contra sí mismo: `identical`, exit 0.
6. `--abi-diff` contra una revisión con distribución de bits modificada: `abi-breaking`, identidad
   explicada, exit 1.
7. El test dirigido comprobó que un cambio ownership sólo de fuente no se clasifica como ruptura
   ABI.
8. `NSC_BUILD_TESTS` quedó restaurado a `OFF` y se reconstruyó `nsc` en ese estado.
9. Una reconfiguración limpia resolvió `NSC_TEST_COMPILER` a `$<TARGET_FILE:nsc>` sin depender de
   `C:\native-sharp` ni `C:\native-sharp-3.0`.

No se ejecutó baseline, CTest general, ninguna matriz O1/O2/O3, instalación, deploy ni remotas. El
compilador probado fue exclusivamente `A:\nsc-devel\build-clang\bin\nsc.exe`.

## Mejora concreta

Una biblioteca NativeSharp puede entregar un contrato ABI auditable y un gate de compatibilidad
antes de reemplazar el artefacto instalado. Los cambios físicos dejan de ser una comparación opaca
de hashes: ahora se localizan en el tipo, campo, miembro, tabla o export responsable.

## Deuda explícita

- F01/F02: faltan hosts C/C++ de cierre y reglas exhaustivas de bitfield contra MSVC;
- F03: nullability, address spaces, provenance y aritmética de punteros no tienen todavía metadata
  y reporte integrales;
- F04: falta rechazo completo de casts incompatibles entre todas las calling conventions;
- F05: bloques/funciones `unsafe`, reason annotation y reporte de operaciones unsafe no están
  conectados al pipeline;
- F06: faltan contratos completos de nullability, length, allocator, error y callback thread en el
  header generado;
- F07: SIMD genérico, target features, alignment y fallback desde código NativeSharp continúan
  pendientes;
- F08: la entrada de destrucción se observa y el witness DLL previo valida una instancia, pero
  allocator provenance, CRT/module lifetime y protección de unload no están cerrados;
- falta gate de plugin incompatible antes de carga, hosts C/C++ amplios, sanitizers aplicables y
  matriz diferencial completa con ambos linkers.

Por estas deudas T13 permanece `IMPLEMENTING`. La tanda operativa 19 queda cerrada porque el
vertical de observabilidad y compatibilidad ABI funciona de extremo a extremo sobre NSMETA real,
sin declarar completas las áreas aún desconectadas.
