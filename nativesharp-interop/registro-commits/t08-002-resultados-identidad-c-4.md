# T08-002: resultados del cuarto conjunto (H7, H8 y evidencia de H1)

Ejecución dirigida en la rama `interop-t08-claude-identidad` con el build aislado
`A:\nsc-devel\build-claude-t08\build-ON`:

- Clang `A:\al82dev-clang` 23.1.0.
- `nslink.exe` existente de `A:\nsc-devel\build-clang\bin`, solo lectura.
- `nsc_cpp_c_identity_tests` reconstruido de forma incremental tras cada commit
  de código. Los tres estados compilaron.
- `nsc` reconstruido de forma incremental dos veces por la medición temporal de
  H7:
  - con el parche: hash `c756fa1bb11fb393`;
  - revertido, con la fuente igual a HEAD: hash `368fe14bbf0d356a`, que es con
    el que se ejecutó el validador final.

Comando final: `pruebas-tanda-08-identidad-c\validar.cmd` sin argumentos y sin
`NSC_T08_PROPUESTAS`. Código de salida: 0. Hubo 67 casos de compilación, 20
restauraciones O0–O3 y ningún `FAIL`.

No se ejecutó la batería de la tanda 08 ni otras suites: no es validación
integral.

## H7, resultados ACTUAL (validador final)

```text
Case CConstNone compiled and ran.
Case CConstSurfaceOnly compiled and ran.
Case CConstNamedPlain rejected as state: metadata contains an incomplete or duplicate namespace state
Case CConstNamedTemplate rejected as state: metadata contains an incomplete or duplicate namespace state
Case CConstNamedSurfaceValue rejected as state: metadata contains an incomplete or duplicate namespace state
Case CConstNamedSurfaceTemplate rejected as state: metadata contains an incomplete or duplicate namespace state
Case CConstAnonPlain rejected as state: metadata contains an incomplete or duplicate namespace state
Case CConstAnonTemplate rejected as state: metadata contains an incomplete or duplicate namespace state
PUBLISHED A:\nsc-devel\build-claude-t08\out\identidad-c\constantes-enum\PConstNone.nsmeta: states=0 catalogue-records=0 catalogue-enums=0
CATALOGUE enum PConstSurfaceOnly::CConstMode: CConstModeA=10 CConstModeB=11
PUBLISHED A:\nsc-devel\build-claude-t08\out\identidad-c\constantes-enum\PConstSurfaceOnly.nsmeta: states=0 catalogue-records=0 catalogue-enums=1
Inspection PConstNamedPlain skipped: the producer did not build in this run.
Inspection PConstNamedTemplate skipped: the producer did not build in this run.
Inspection PConstNamedSurface skipped: the producer did not build in this run.
Inspection PConstAnonPlain skipped: the producer did not build in this run.
Inspection PConstAnonTemplate skipped: the producer did not build in this run.
```

Primer diagnóstico del productor (`out\identidad-c\constantes-enum\PConstNamedPlain.log`):

```text
nsc: error: Stage 48.2 static-library metadata generation failed: metadata producer read-back verification failed: metadata contains an incomplete or duplicate namespace state '.CConstModeB'
```

## H7, resultados con el parche A aplicado temporalmente

`validar.cmd constantes` con `NSC_T08_PROPUESTAS=1` y `nsc` con hash
`c756fa1bb11fb393`. Después se revirtió el parche.

```text
Case CConstNone compiled and ran.
Case CConstSurfaceOnly compiled and ran.
Case CConstNamedPlain compiled and ran.
Case CConstNamedTemplate rejected as ident: unknown identifier
Case CConstNamedSurfaceValue compiled and ran.
Case CConstNamedSurfaceTemplate compiled and ran.
Case CConstAnonPlain compiled and ran.
Case CConstAnonTemplate rejected as ident: unknown identifier
PUBLISHED ...\PConstNone.nsmeta: states=0 catalogue-records=0 catalogue-enums=0
CATALOGUE enum PConstSurfaceOnly::CConstMode: CConstModeA=10 CConstModeB=11
PUBLISHED ...\PConstSurfaceOnly.nsmeta: states=0 catalogue-records=0 catalogue-enums=1
PUBLISHED ...\PConstNamedPlain.nsmeta: states=0 catalogue-records=0 catalogue-enums=0
PUBLISHED ...\PConstNamedTemplate.nsmeta: states=0 catalogue-records=0 catalogue-enums=0
CATALOGUE enum PConstNamedSurface::CConstMode: CConstModeA=10 CConstModeB=11
PUBLISHED ...\PConstNamedSurface.nsmeta: states=0 catalogue-records=0 catalogue-enums=1
PUBLISHED ...\PConstAnonPlain.nsmeta: states=0 catalogue-records=0 catalogue-enums=0
PUBLISHED ...\PConstAnonTemplate.nsmeta: states=0 catalogue-records=0 catalogue-enums=0
T08-002 C enum constant checks passed.
PConstNamedTemplate.nsmeta.public-templates.ns:5:52: error: unknown identifier 'CConstModeB' in callable/initializer context
PConstAnonTemplate.nsmeta.public-templates.ns:5:48: error: unknown identifier 'CConstLoose' in callable/initializer context
```

## H8 (validador final)

```text
FIXTURE legacy-enum-only.hex bytes=240 first=04 00 00 00 00 00 00 00 current: envelope=yes decoded=no 'invalid C record catalogue size'; proposed 1.42: decoded=yes enums=1
PRODUCER PCatEnumOnly: NSMETA 1.43 envelope=1 records=0 enums=1
VARIANT PCatEnumOnly 1.42: first=04 00 00 00 00 00 00 00 sniffed-envelope=yes current=fails 'metadata before 1.43 cannot represent C source provenance' proposed=decodes records=0 enums=1
VARIANT PCatEnumOnly 1.43: first=01 00 00 00 7e 01 00 00 sniffed-envelope=yes current=reads proposed=decodes records=0 enums=1
VARIANT PCatEnumOnly 1.44: first=01 00 00 00 7e 01 00 00 sniffed-envelope=yes current=reads proposed=decodes records=0 enums=1
VARIANT PCatEnumOnly 1.45: first=01 00 00 00 7e 01 00 00 sniffed-envelope=yes current=reads proposed=decodes records=0 enums=1
PRODUCER PCatRecord: NSMETA 1.43 envelope=1 records=1 enums=0
VARIANT PCatRecord 1.42: first=e9 00 00 00 00 00 00 00 sniffed-envelope=no current=reads proposed=decodes records=1 enums=0
VARIANT PCatRecord 1.43: first=01 00 00 00 f5 00 00 00 sniffed-envelope=yes current=reads proposed=decodes records=1 enums=0
VARIANT PCatRecord 1.44: first=01 00 00 00 f5 00 00 00 sniffed-envelope=yes current=reads proposed=decodes records=1 enums=0
VARIANT PCatRecord 1.45: first=01 00 00 00 f5 00 00 00 sniffed-envelope=yes current=reads proposed=decodes records=1 enums=0
PRODUCER PCatRecordEnum: NSMETA 1.43 envelope=1 records=1 enums=1
VARIANT PCatRecordEnum 1.42: first=f1 00 00 00 00 00 00 00 sniffed-envelope=no current=reads proposed=decodes records=1 enums=1
VARIANT PCatRecordEnum 1.43: first=01 00 00 00 6d 02 00 00 sniffed-envelope=yes current=reads proposed=decodes records=1 enums=1
VARIANT PCatRecordEnum 1.44: first=01 00 00 00 6d 02 00 00 sniffed-envelope=yes current=reads proposed=decodes records=1 enums=1
VARIANT PCatRecordEnum 1.45: first=01 00 00 00 6d 02 00 00 sniffed-envelope=yes current=reads proposed=decodes records=1 enums=1
PRODUCER PCatFloat: NSMETA 1.44 envelope=2 records=1 enums=0
VARIANT PCatFloat 1.44: first=02 00 00 00 0b 01 00 00 sniffed-envelope=yes current=reads proposed=decodes records=1 enums=0
VARIANT PCatFloat 1.45: first=02 00 00 00 0b 01 00 00 sniffed-envelope=yes current=reads proposed=decodes records=1 enums=0
PRODUCER PCatOpaque: NSMETA 1.45 envelope=3 records=1 enums=0
VARIANT PCatOpaque 1.45: first=03 00 00 00 86 00 00 00 sniffed-envelope=yes current=reads proposed=decodes records=1 enums=0
PRODUCER PCatFloatOpaque: NSMETA 1.45 envelope=4 records=2 enums=0
VARIANT PCatFloatOpaque 1.45: first=04 00 00 00 97 01 00 00 sniffed-envelope=yes current=reads proposed=decodes records=2 enums=0
C catalogue version controls passed
```

## Evidencia nueva para H1 (validador final)

```text
TYPEDEF FORM formas/plain.h + formas/at@sign.h
  first  c:plain.h@T@CTdForm 'int' record=- enum=-
  second c:at@sign.h@T@CTdForm 'int' record=- enum=-
  current=reject proposed=reject
TYPEDEF FORM formas/plain.h + formas/with space.h
  first  c:plain.h@T@CTdForm 'int' record=- enum=-
  second c:with space.h@T@CTdForm 'int' record=- enum=-
  current=reject proposed=accept
TYPEDEF FORM formas/macro-x.h + formas/macro-y.h
  first  c:macro-x.h@T@CTdMacro 'int' record=- enum=-
  second c:macro-y.h@T@CTdMacro 'long long' record=- enum=-
  current=reject proposed=reject
Typedef identity matrix passed
```

## Corrección

`T08-002-H8-CATALOGO.md` decía "catorce variantes"; son dieciséis (4 + 4 + 4 + 2 +
1 + 1). Se corrige en este commit.
