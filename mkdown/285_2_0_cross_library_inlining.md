# 285 — Inlining entre bibliotecas precompiladas / Cross-library inlining

## Resultado / Result

NSMETA 1.13 conserva la política de inlining de los callables publicados y el cuerpo fuente de los
métodos ordinarios `inline` y `forceinline`. Un consumidor que sólo recibe `LibraryReference`
materializa esos cuerpos dentro de la identidad nominal del productor, los analiza con el frontend
normal y puede expandirlos en O0. `noinline` cruza como atributo de la declaración importada sin
copiar la implementación.

NSMETA 1.13 preserves published callable inlining policies and the source body of ordinary
`inline` and `forceinline` methods. A consumer receiving only a `LibraryReference` materializes
those bodies under the producer's nominal identity, analyzes them through the normal frontend, and
can expand them at O0. `noinline` crosses as an imported-declaration attribute without copying the
implementation.

## Archivos y funciones / Files and functions

- `include/nativesharp/Interop/LibraryMetadata.h`
  - Eleva el formato de 1.12 a 1.13.
  - Publica el rango de bits de `LibraryMetadataMember::flags` reservado a `InliningPolicy`.
  - Agrega `inlineSourceText` al registro del miembro.
- `src/Interop/LibraryMetadata.cpp`
  - `memberFlags()` codifica la política de cuatro estados.
  - `buildMetadataModel()` conserva la declaración fuente sólo para métodos ordinarios preferidos
    u obligatorios; las plantillas genéricas mantienen su representación existente.
  - El codec escribe, lee, valida y elimina correctamente el payload al producir minors antiguos.
- `src/Interop/LibraryMetadataMerger.cpp`
  - Incluye el cuerpo inline en la igualdad y detección de conflictos entre fragmentos.
- `src/Driver/Driver.cpp`
  - `buildPrecompiledPublicSource()` inserta el cuerpo transportado dentro del tipo reconstruido y
    vuelve a declarar `noinline` sobre imports opacos.
- `src/Frontend/ProjectMemberModel.cpp`
  - Distingue un cuerpo inline transportado de las declaraciones vacías del mismo unit precompilado.
- `src/Frontend/ProjectOopHirModel.cpp`
  - Propaga la política de un callable NativeSharp importado al HIR.
- `src/LLVM/ProjectOopLlvmLowerer.cpp`
  - Emite copias `weak_odr` con COMDAT por símbolo para cuerpos distribuibles.
  - Adjunta `noinline` también a declaraciones NativeSharp precompiladas.
- `tests/unit/CrossLibraryInliningTests.cpp`
  - Produce una `.lib + .nsmeta`, consume únicamente esos artefactos, inspecciona LLVM y ejecuta un
    EXE O0. Cubre métodos estáticos, receptor struct, las tres políticas explícitas, identidad física
    compartida y ausencia de símbolos fuertes duplicados.
- Documentación 1.0.1
  - Actualiza estado, changelog, versión, roadmap, especificación, contrato del compilador, libro y
    mapas de conformidad.

## Decisiones de diseño / Design decisions

El cuerpo vive en el mismo registro que su firma porque ambos describen un único callable físico.
No se creó una tabla paralela que pudiera perder sincronía. Los métodos genéricos no duplican texto:
su `LibraryMetadataGenericTemplate` ya es la fuente canónica de especialización entre módulos.

The body lives in the same record as its signature because both describe one physical callable. No
parallel table was introduced that could drift out of sync. Generic methods do not duplicate text:
their `LibraryMetadataGenericTemplate` remains the canonical cross-module specialization source.

Una definición transportada no puede continuar siendo fuerte: productor y consumidor pueden
aportar el mismo símbolo al enlace. `weak_odr` más un COMDAT cuyo líder es el propio símbolo expresa
exactamente esa identidad compartida y evita el antiguo defecto de grupos COMDAT parciales. La
política `noinline` no necesita ni debe transportar cuerpo; basta conservarla sobre la declaración.

A transported definition cannot remain strong because producer and consumer may both contribute
the same symbol to the final link. `weak_odr` plus a COMDAT led by that symbol expresses the shared
identity and avoids partial COMDAT groups. `noinline` neither needs nor should transport a body; its
declaration policy is sufficient.

## Evidencia enfocada / Focused evidence

```text
ctest -R ^nsc.language.cross-library-inlining$
1/1 passed
14 checks
O0 producer + NSMETA 1.13 + LibraryReference consumer + executed EXE
```

No se ejecutó baseline en esta entrega. / No baseline was run for this delivery.

## Mensaje de commit de referencia / Reference commit message

**Título / Title**

`Transporta inlining entre bibliotecas / Transport cross-library inlining`

**Descripción / Description**

`Eleva NSMETA a 1.13 para conservar políticas y cuerpos inline ordinarios, materializa definiciones ODR en consumidores LibraryReference y preserva noinline sobre imports nativos. / Raise NSMETA to 1.13 to preserve ordinary inline policies and bodies, materialize ODR definitions in LibraryReference consumers, and preserve noinline on native imports.`
