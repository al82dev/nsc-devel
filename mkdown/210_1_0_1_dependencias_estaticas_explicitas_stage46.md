# Dependencias estáticas explícitas en Stage 46 / Explicit static dependencies in Stage 46

## Resumen / Summary

Las bibliotecas estáticas NativeSharp conservan sus `LibraryReference` como dependencias externas. Un archivo productor no debe incorporar silenciosamente los objetos de otra biblioteca ni vender sus símbolos como propios. Un consumidor final debe declarar el cierre estático completo que enlaza.

NativeSharp static libraries keep their `LibraryReference` entries as external dependencies. A producer archive must not silently embed another library's objects or vend its symbols as its own. A final consumer must declare the complete static link closure.

## Archivo modificado / Modified file

- `tests/unit/Stage46NativeLibrariesTests.cpp`
  - Actualiza la sección compuesta de `testPrecompiledLibraryReferences`.
  - Updates the composite section of `testPrecompiledLibraryReferences`.

## Implementación / Implementation

El productor `Composite` continúa llamando a `NativeMath`, pero su archivo `.lib` ahora se valida como propietario únicamente de `Composite.Answer`. La prueba exige que el símbolo `Math.Add` no esté dentro de `Composite.lib`.

El proyecto consumidor declara dos referencias independientes: `Composite.lib` y `NativeMath.lib`. De esta forma el ejecutable resuelve el cierre completo sin duplicar, copiar ni ocultar propiedad de objetos COFF.

The `Composite` producer still calls `NativeMath`, but its `.lib` is now validated as owning only `Composite.Answer`. The test requires `Math.Add` to be absent from `Composite.lib`. The consumer declares both archives independently, making the static closure explicit without duplicating or obscuring COFF object ownership.

## Motivo / Rationale

Vendorizar transitivamente una biblioteca estática crea colisiones de símbolos, rompe actualizaciones independientes y vuelve ambiguo qué producto posee cada implementación. La regla explícita coincide con el comportamiento ya implementado por `WindowsLibraryBuilder` y con la prueba específica `testStaticLibraryKeepsReferencesExternal`.

Transitively vending a static library creates symbol collisions, breaks independent updates, and obscures implementation ownership. The explicit rule matches the behavior already implemented by `WindowsLibraryBuilder` and the dedicated `testStaticLibraryKeepsReferencesExternal` contract.

## Validación / Validation

No se ejecutaron pruebas ni baseline durante esta corrección. Se validará acumulativamente al final del conjunto pendiente.

No tests or baseline were run during this correction. It will be validated cumulatively after the pending set is complete.

## Commit de referencia / Reference commit

### Título / Title

```text
Alinea Stage 46 con dependencias estáticas explícitas / Align Stage 46 with explicit static dependencies
```

### Descripción / Description

```text
ES:
- evita esperar símbolos vendorizados dentro del archivo productor
- exige que el consumidor declare el cierre estático completo
- conserva la propiedad individual de cada implementación COFF

EN:
- stop expecting vendored symbols inside the producer archive
- require the consumer to declare the complete static closure
- preserve individual ownership of each COFF implementation
```
