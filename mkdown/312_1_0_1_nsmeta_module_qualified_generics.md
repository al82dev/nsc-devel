# 1.0.1-dev — Calificador de módulo en genéricos importados desde NSMETA

## Resultado / Result

La reconstrucción de una API pública precompilada conserva ahora la identidad completa
`Modulo::Namespace.Tipo` para tipos externos y especializaciones genéricas del store.
Un consumidor puede combinar un `LibraryReference` con el `ProjectReference` que
proporciona el store sin que la misma plantilla visible desde dos módulos se vuelva
ambigua.

Precompiled public API reconstruction now preserves the complete
`Module::Namespace.Type` identity for external types and store-owned generic
specializations. A consumer can combine a `LibraryReference` with the store
`ProjectReference` without making the same template visible through two modules
ambiguous.

## Defecto / Defect

NSMETA ya guardaba correctamente claves como:

```text
type:NativeSharpCore::NativeSharp.Core.ReadOnlySpan__g1__byte__...
NativeSharpCore::NativeSharp.Core.ReadOnlySpan`1
```

Sin embargo, `sourceTypeForMetadataKey()` y
`sourceTypeForGenericSpecialization()` reconstruían:

```text
NativeSharp.Core.ReadOnlySpan<byte>
```

La pérdida de `NativeSharpCore::` ocurría antes de la fase genérica. Al coexistir la
plantilla original del store con una clausura materializada desde metadata,
`resolveTypeTemplate()` encontraba más de una candidata y emitía:

```text
does not name a unique visible generic class/struct template
```

The metadata was correct; only its synthesized public-source representation lost
the owning module before generic binding.

## Archivos y funciones / Files and functions

- `src/Driver/Driver.cpp`
  - `sourceTypeForMetadataKey(metadata, ...)` conserva el módulo de tipos nominales
    externos.
  - `sourceTypeForGenericSpecialization()` conserva el módulo de la plantilla
    externa antes de reconstruir `<T>`.
  - Los tipos pertenecientes al propio módulo de metadata siguen escribiéndose sin
    prefijo, evitando alterar referencias internas del contrato reconstruido.
- `src/Parser/Parser.cpp`
  - `parseQualifiedName()` reconoce la forma canónica
    `Modulo::Namespace.Tipo`.
  - Los detectores de declaraciones, casts y tipos entre paréntesis consumen el
    prefijo de módulo como una unidad.
- `src/Frontend/ProjectGenericModel.cpp`
  - `resolveTypeTemplate()` selecciona primero el módulo exacto y después la
    identidad calificada y la aridad. No cae a búsqueda por nombre cuando NSMETA ya
    proporcionó identidad nominal completa.
- `src/Frontend/ProjectNameLookup.cpp`
  - `resolveTypeCandidates()` resuelve nombres con `::` directamente en el
    espacio del módulo solicitado y conserva las reglas públicas para tipos
    externos.
- `tests/unit/Stage48_2LibraryMetadataTests.cpp`
  - Añade un store con `Span<T>` y `ReadOnlySpan<T>`.
  - Añade un productor O0 que publica ambas especializaciones con `byte`.
  - Añade un consumidor que usa simultáneamente `ProjectReference` al store y
    `LibraryReference` al productor.
  - Comprueba la identidad canónica guardada en NSMETA y la materialización hasta
    OOP HIR.

## Decisiones de diseño / Design decisions

1. El módulo se conserva sólo cuando el tipo pertenece a otro módulo. Las
   declaraciones locales del contrato continúan usando su escritura NativeSharp
   ordinaria.
2. `::` representa selección nominal de módulo, no un nuevo namespace ni un
   `using`; por ello evita sombreado y no depende del orden de referencias.
3. La resolución calificada no utiliza heurísticas por nombre simple. Un módulo o
   tipo inexistente sigue produciendo el diagnóstico normal de tipo no resuelto.
4. La corrección cubre tipos nominales, genéricos cerrados, argumentos genéricos
   anidados y modificadores posteriores como punteros o arreglos; no contiene casos
   especiales para `Span` o `ReadOnlySpan`.

The fix treats `Module::` as nominal identity rather than a namespace import. It
therefore applies to every external generic stored in NSMETA, not only the two
types that exposed the defect.

## Evidencia focalizada / Focused evidence

```text
nsc_stage48_2_library_metadata_tests
PASS: 247 checks
```

La prueba nueva emite el productor con `-O0`. No se ejecutó baseline, runtime
externo, bindings ni tutoriales.

The new producer is emitted with `-O0`. No baseline, external runtime, bindings,
or tutorials were run.

## Commit de referencia / Reference commit

### Español

**Título:** Conserva el módulo de genéricos importados desde NSMETA

**Descripción:** Mantiene la identidad `Modulo::Tipo` al reconstruir tipos externos
y especializaciones genéricas desde metadata, enseña al parser y a las fases
genérica y nominal a resolverla exactamente, y añade una regresión O0
store/productor/consumidor para `Span<T>` y `ReadOnlySpan<T>`.

### English

**Title:** Preserve imported NSMETA generic module identity

**Description:** Keep `Module::Type` identity while rebuilding external types and
generic specializations from metadata, teach the parser plus generic and nominal
binding to resolve it exactly, and add an O0 store/producer/consumer regression for
`Span<T>` and `ReadOnlySpan<T>`.
