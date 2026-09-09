# 1.0.1 - Constructores genericos exportados y NSMETA / Exported generic constructors and NSMETA

## Resumen / Summary

**ES.** Una DLL NativeSharp podia compilar y enlazar correctamente, pero
`--verify-library-artifacts` rechazaba despues uno de sus propios simbolos: el
constructor publico de una especializacion generica concreta de struct. La tabla
de exportaciones PE y `.nsmeta` eran correctas por separado; el defecto estaba en
la funcion que derivaba desde metadata el conjunto de simbolos fisicos esperados.

**EN.** A NativeSharp DLL could compile and link successfully, yet
`--verify-library-artifacts` subsequently rejected one of its own symbols: the
public constructor of a concrete generic struct specialization. The PE export
table and `.nsmeta` were independently correct; the defect was in the function
that derived the expected physical symbol set from metadata.

## Reproductor / Reproducer

**ES.** El caso externo que expuso el defecto esta en
`A:\native-sharp-bindings-devel\bindings\_bugs\precompiled_dll_generic_exports`.
Declara `Box<T>`, materializa `Box<int>` desde `Boxes.Make()` y produce una DLL
con tres exportaciones. Antes de este cambio, el verificador reportaba el simbolo
`__nsc_ctor_*Box*int*` como inesperado.

**EN.** The external case that exposed the defect lives at
`A:\native-sharp-bindings-devel\bindings\_bugs\precompiled_dll_generic_exports`.
It declares `Box<T>`, materializes `Box<int>` from `Boxes.Make()`, and produces a
DLL with three exports. Before this change, the verifier reported the
`__nsc_ctor_*Box*int*` symbol as unexpected.

## Causa / Root cause

**ES.** `publishedNativeSymbols()` usaba `canMaterializeConstructor()` para
decidir si un constructor debia existir fisicamente. Esa funcion responde una
pregunta distinta: si el importador puede reconstruir el constructor como una
declaracion utilizable por `new`. Por diseno esa ruta solo admite clases con un
descriptor PE seguro, por lo que excluia constructores de structs. El productor,
sin embargo, exporta todo constructor publico de un tipo publico del proyecto
raiz, incluidos los structs genericos concretos.

**EN.** `publishedNativeSymbols()` used `canMaterializeConstructor()` to decide
whether a constructor had to exist physically. That function answers a different
question: whether the importer can reconstruct the constructor as a declaration
usable by `new`. By design, that path only accepts classes with a PE-safe class
descriptor, so it excluded struct constructors. The producer, however, exports
every public constructor of a public root-project type, including concrete generic
structs.

## Implementacion / Implementation

### `src/Driver/Driver.cpp`

- **ES:** se agrego `isPublishedNativeConstructor()`. La funcion replica el
  contrato fisico del productor: miembro constructor publico, nombre mangled no
  vacio y tipo declarador publico presente en metadata.
- **EN:** added `isPublishedNativeConstructor()`. It mirrors the producer's
  physical contract: public constructor member, non-empty mangled name, and a
  public declaring type present in metadata.
- **ES:** `publishedNativeSymbols()` usa ahora esa regla fisica para constructores.
  `canMaterializeConstructor()` permanece intacta y continua gobernando solo la
  reconstruccion de API precompilada.
- **EN:** `publishedNativeSymbols()` now uses that physical rule for constructors.
  `canMaterializeConstructor()` remains unchanged and continues to govern only
  precompiled API reconstruction.

### `tests/unit/Stage48_2LibraryMetadataTests.cpp`

- **ES:** `testAutomaticProducts()` crea una DLL minima con `Box<T>`, su
  especializacion `Box<int>` y un constructor publico.
- **EN:** `testAutomaticProducts()` creates a minimal DLL with `Box<T>`, its
  `Box<int>` specialization, and a public constructor.
- **ES:** la prueba demuestra que `.nsmeta` serializa el constructor y que la
  certificacion conjunta de DLL, import library y metadata acepta exactamente las
  tres exportaciones del producto.
- **EN:** the test proves that `.nsmeta` serializes the constructor and that joint
  DLL, import-library, and metadata certification accepts exactly the product's
  three exports.

## Decisiones / Decisions

- **ES:** no se oculto el constructor de la tabla PE y no se debilito la
  comparacion de artefactos.
- **EN:** the constructor was not hidden from the PE table and artifact comparison
  was not weakened.
- **ES:** no se habilito `new` sobre structs precompilados. Exportacion fisica y
  materializacion de fuente permanecen contratos separados.
- **EN:** `new` over precompiled structs was not enabled. Physical publication and
  source materialization remain separate contracts.
- **ES:** no cambia el formato NSMETA ni su ABI; el miembro ya estaba serializado.
- **EN:** neither the NSMETA format nor its ABI changes; the member was already
  serialized.

## Evidencia / Evidence

- Regresion roja previa / Prior red regression: `1 of 96 checks` failed in Stage
  48.2 at DLL/NSMETA artifact verification.
- `nsc_stage46_native_libraries_tests`: `PASS: 145 checks`.
- `nsc_stage48_2_library_metadata_tests`: `PASS: 98 checks`.
- Reproductor externo con el compilador corregido / External reproducer with the
  fixed compiler: `BUILD=0 VERIFY=0`, `runtime-exports: 3`, `required-symbols: 3`.
- **ES:** no se ejecuto el baseline completo; este cierre usa pruebas dirigidas
  para no consumir CPU durante la iteracion PDB activa.
- **EN:** the full baseline was not run; this closure uses targeted tests to avoid
  consuming CPU during the active PDB iteration.

## Mensaje de commit / Commit message

### Titulo ES / Spanish title

`Sincroniza constructores genericos con NSMETA`

### Title EN / English title

`Synchronize generic constructors with NSMETA`

### Descripcion ES / Spanish description

`Separa la validacion de simbolos fisicos de la materializacion de API precompilada, incluye constructores publicos de structs genericos en el contrato de exportaciones y agrega una regresion DLL+LIB+NSMETA.`

### Description EN / English description

`Separate physical symbol validation from precompiled API materialization, include public generic struct constructors in the export contract, and add a DLL+LIB+NSMETA regression.`
