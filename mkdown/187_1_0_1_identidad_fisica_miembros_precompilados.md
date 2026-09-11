# 1.0.1 — Identidad física de miembros NativeSharp precompilados

## Resultado

El consumidor de una biblioteca precompilada usa ahora el `mangledName` exacto
publicado por el productor en NSMETA. La firma reconstruida desde la API
sintética continúa resolviéndose y validándose semánticamente, pero dejó de ser
una segunda fuente de identidad física.

Esto corrige el caso en que el productor publica:

```text
List__g1__ProducerValue__hd9cc17e0d29e6fd9
```

y el consumidor reconstruye el mismo tipo canónico con una ortografía más
calificada. Antes, HIR volvía a ejecutar `mangleCallable` y pedía un símbolo con
texto distinto aunque NSMETA ya contenía el nombre correcto.

## Causa raíz

`LibraryMetadataMember::mangledName` sobrevivía escritura, lectura y
verificación de NSMETA, pero `ClassMemberSymbol` no tenía dónde conservarlo.
`ProjectOopHirModel` trataba por ello un callable precompilado como si fuera una
declaración local y recalculaba su nombre desde el tipo y los parámetros
materializados como source.

La identidad semántica era equivalente; la identidad física no. Un símbolo
externo publicado debe producirse una vez, en el productor.

## Archivos, funciones e implementación

- `include/nativesharp/Frontend/ProjectMemberModel.h`:
  añade `ClassMemberSymbol::precompiledMangledName`, el contrato transportable
  `PrecompiledMemberIdentity` y la operación
  `bindPrecompiledPhysicalNames`.
- `include/nativesharp/Frontend/ProjectCompilation.h`:
  cada `PrecompiledSyntaxModule` transporta los miembros físicos leídos de su
  NSMETA junto con el source sintético y las especializaciones.
- `src/Driver/Driver.cpp`, `configurePrecompiledModules`:
  copia clase, tipo propietario, nombre, firma y `mangledName` de cada miembro
  NSMETA al descriptor del frontend.
- `src/Frontend/ProjectCompilation.cpp`, `compileProjectMembers`:
  construye aliases entre los nombres locales regenerados de especializaciones
  cerradas y los nombres publicados por el productor; después enlaza cada
  callable sintético con su identidad física.
- `src/Frontend/ProjectMemberModel.cpp`,
  `bindPrecompiledPhysicalNames`:
  empareja por clase de miembro, tipo propietario, nombre y firma canónica. Los
  aliases se aplican sólo durante esa comparación. Una identidad ausente o
  ambigua es un error temprano de compilación, no un fallo posterior del
  enlazador.
- `src/Frontend/ProjectOopHirModel.cpp`, `ProjectOopHirModel::build`:
  los métodos y constructores NativeSharp precompilados toman el nombre ya
  asociado. HIR rechaza explícitamente cualquier callable precompilado que no
  tenga símbolo NSMETA.
- `tests/unit/Stage48_2LibraryMetadataTests.cpp`,
  `testPrecompiledClosedGenericSurface`:
  el productor publica `CountValues(Bucket<ProducerValue>)` y el consumidor lo
  llama usando exclusivamente `LibraryReference`.

## Alcance

La regla es común a métodos, constructores, operadores y accessors porque todos
ellos llegan al modelo como `Method` o `Constructor` con una firma NSMETA. Los
imports `extern C` mantienen su identidad C independiente y los métodos
abstractos no requieren símbolo físico.

No se alteró el mangling de código local, el formato binario NSMETA ni los
bindings. La canonicalización sirve para comprobar que el miembro sintético
corresponde al contrato; nunca reemplaza el nombre externo publicado.

## Evidencia dirigida

- Compilación exclusiva de `nsc_stage48_2_library_metadata_tests`.
- Ejecución: `PASS: 185 checks`.
- El caso dirigido produce biblioteca y NSMETA, verifica artefactos, pasa HIR,
  emite el consumidor O0 y lo ejecuta con código 0.

Por instrucción expresa no se ejecutó baseline ni O2.

## Commit de referencia

### Español

**Título:** Conserva símbolos físicos de miembros precompilados

**Descripción:** Transporta el `mangledName` de NSMETA al miembro sintético,
canonicaliza únicamente la asociación semántica y hace que HIR use la identidad
física publicada. Añade una regresión con un método cuya firma contiene una
especialización genérica cerrada del productor.

### English

**Title:** Preserve physical symbols for precompiled members

**Description:** Carry the NSMETA `mangledName` into the synthetic member,
canonicalize only the semantic association, and make HIR use the published
physical identity. Add a regression for a method whose signature contains a
producer-owned closed generic specialization.

## English summary

Precompiled NativeSharp callables now retain the exact physical symbol emitted
by their producer. Synthetic source remains responsible for semantic lookup,
while NSMETA is the sole source of external symbol identity. The targeted O0
producer/consumer regression passes without source references or a baseline.
