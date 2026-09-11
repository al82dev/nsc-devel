# Runtime declarado sin importación física / Declared runtime without a physical import

## Resumen / Summary

Un `LibraryReference` puede incluir `Runtime` para describir dónde se encuentra la DLL de un producto sin que cada consumidor termine importándola. Si el código consumido sólo usa tipos de metadata y ningún símbolo físico de esa DLL, el enlazador no crea una entrada PE de importación. Esa ausencia no es un error: es el cierre mínimo correcto de dependencias.

A `LibraryReference` may include `Runtime` to describe a product DLL location without forcing every consumer to import it. If consumed code uses only metadata types and no physical symbol from that DLL, the linker creates no PE import entry. That absence is not an error; it is the correct minimal dependency closure.

## Archivo modificado / Modified file

- `tests/unit/Stage46NativeLibrariesTests.cpp`
  - Actualiza `testCompileOnlyMetadataReferenceDoesNotPromiseRuntime`.
  - Replaces the obsolete rejection in `testCompileOnlyMetadataReferenceDoesNotPromiseRuntime`.

## Implementación / Implementation

El caso conserva deliberadamente un atributo `Runtime` para `MetadataTypes.dll`, compila el consumidor y verifica tres propiedades:

1. La compilación tiene éxito.
2. El PE importa `MetadataOnlyApi.dll`, cuyos símbolos sí utiliza.
3. El PE no importa `MetadataTypes.dll`, usada únicamente como contrato nominal.

En Windows el ejecutable también debe conservar el resultado observable 42.

The case deliberately retains a `Runtime` attribute for `MetadataTypes.dll`, compiles the consumer, and verifies three properties: compilation succeeds, the PE imports the DLL that contributes physical symbols, and it does not import the metadata-only DLL. On Windows, the executable must also retain observable result 42.

## Motivo / Rationale

La tabla de importaciones del PE es la autoridad sobre dependencias físicas. Rechazar un runtime declarado pero no importado obligaría dependencias artificiales y contradiría el soporte agregado para referencias de metadata que sólo aportan tipos.

The PE import table is authoritative for physical dependencies. Rejecting a declared but unused runtime would force artificial dependencies and contradict metadata-only type references.

## Validación / Validation

No se ejecutaron pruebas ni baseline durante esta corrección. Se validará acumulativamente al cerrar el conjunto pendiente.

No tests or baseline were run during this correction. It will be validated cumulatively after the pending set is closed.

## Commit de referencia / Reference commit

### Título / Title

```text
Acepta runtimes declarados sin importación física / Accept declared runtimes without physical imports
```

### Descripción / Description

```text
ES:
- alinea Stage 46 con el cierre físico real de dependencias PE
- acepta Runtime cuando la biblioteca sólo aporta tipos de metadata
- verifica que no se inventen importaciones de DLL

EN:
- align Stage 46 with the actual PE physical dependency closure
- accept Runtime when a library contributes metadata types only
- verify that no DLL imports are invented
```
