# Identidad y documentación versionada 1.0.1 / Versioned 1.0.1 identity and documentation

## Resumen / Summary

La rama `development` deja de anunciar el compilador histórico `0.9.0` y el lenguaje `0.1`. La
familia compatible pasa a NativeSharp 1.0.1, mientras cada binario no certificado conserva de forma
visible el canal de desarrollo:

```text
nsc 1.0.1-dev+build.<UTC>.<id> (NativeSharp 1.0.1, stage 67)
```

The `development` branch no longer advertises the historical `0.9.0` compiler and `0.1` language.
The compatible family moves to NativeSharp 1.0.1, while every uncertified binary visibly retains
the development channel shown above.

La documentación actual ya no se mezcla con los expedientes históricos. Se crea una edición
autocontenida bajo `docs/1.0.1`, se incluye en el paquete instalado y el verificador exige sus puntos
de entrada principales. No se ejecuta ni se atribuye un baseline nuevo.

Current documentation is no longer mixed with historical engineering records. A self-contained
edition is created under `docs/1.0.1`, included in the installed package, and its main entry points
are required by installation verification. No new baseline is run or claimed.

## Archivos de identidad modificados / Modified identity files

- `CMakeLists.txt`
  - Cambia `PROJECT_VERSION` de `0.9.0` a `1.0.1`.
  - Usa `1.0.1-dev` como base de la identidad generada para builds de desarrollo.
  - Actualiza el regex de `nsc.version` y los mensajes de configuración LLVM.
  - Empaqueta `docs/1.0.1` y agrega cada archivo al inventario instalado.
  - Changes `PROJECT_VERSION` from `0.9.0` to `1.0.1`.
  - Uses `1.0.1-dev` as the generated identity base for development builds.
  - Updates the `nsc.version` regex and LLVM configuration messages.
  - Packages `docs/1.0.1` and adds every file to the installed inventory.
- `include/nativesharp/Basic/Version.h`
  - Fija lenguaje y release compatible en `1.0.1`; conserva Stage 67.
  - Sets language and compatible release to `1.0.1`; preserves Stage 67.
- `src/Driver/CompatibilityPolicy.cpp`
  - Publica directamente `language_release=1.0.1` sin agregar el antiguo `.0` sintético.
  - Publishes `language_release=1.0.1` directly without appending the old synthetic `.0`.
- `src/Interop/LibraryMetadata.cpp`
  - Separa tanto prerelease (`-dev`) como metadata (`+build`) al comparar la familia productora de
    NSMETA.
  - Strips both prerelease (`-dev`) and build metadata (`+build`) when comparing an NSMETA
    producer's compatible family.

## Mensajes y contratos actualizados / Updated messages and contracts

- `src/Sema/SemanticAnalyzer.cpp`
  - Los diagnósticos que nombran la versión del lenguaje dicen NativeSharp 1.0.1.
  - Diagnostics that name the language version now say NativeSharp 1.0.1.
- `src/Interop/CHeaderGenerator.cpp`
  - Las guardas de plataforma de headers C identifican el contrato host 1.0.1.
  - C-header platform guards identify the 1.0.1 host contract.
- `src/Interop/LibraryMetadata.cpp`
  - Los errores de target y comentarios del formato se alinean con 1.0.1.
  - Target errors and format comments are aligned with 1.0.1.
- `src/LLVM/LlvmVersion.cpp`
  - El rechazo de una LLVM incompatible identifica la línea 1.0.1.
  - Incompatible LLVM rejection identifies the 1.0.1 line.
- `src/Driver/Driver.cpp`
  - La ayuda remite a la documentación instalada `share/nativesharp/docs/1.0.1` y declara el canal
    de desarrollo actual.
  - Help points to installed documentation under `share/nativesharp/docs/1.0.1` and declares the
    current development line.
- `include/nativesharp/Compilation/Project.h` y `src/LLVM/ProjectOopLlvmLowerer.cpp`
  - Actualizan las referencias vigentes al contrato de bibliotecas componibles.
  - Update current references to the composable-library contract.

## Documentación creada / Created documentation

### Entrada y estado / Entry and status

- `docs/1.0.1/README.md`
  - Índice de lectura, autoridad documental, ubicación instalada y reglas para programadores que
    llegan desde C#.
  - Reading index, documentation authority, installed location, and rules for programmers coming
    from C#.
- `docs/README.md`
  - Índice estable de ediciones para que futuras versiones agreguen una carpeta sin sustituir las
    anteriores.
  - Stable edition index so future versions can add a directory without replacing previous ones.
- `docs/1.0.1/VERSION.md`
  - Distingue familia 1.0.1, canal `-dev`, build id, lenguaje, Stage, LLVM, NSMETA y ABIs.
  - Distinguishes the 1.0.1 family, `-dev` channel, build id, language, Stage, LLVM, NSMETA, and ABIs.
- `docs/1.0.1/CURRENT_STATE.md`
  - Resume el modelo nativo, lenguaje, ownership, interop, bibliotecas precompiladas, PDB, Core y
    diferencias deliberadas respecto de C#.
  - Summarizes the native model, language, ownership, interop, precompiled libraries, PDB, Core, and
    deliberate differences from C#.
- `docs/1.0.1/CHANGELOG.md`
  - Expone el resultado acumulado desde 0.9 sin copiar el diario histórico de commits.
  - Exposes the accumulated result since 0.9 without copying the historical commit diary.

### Contratos actuales / Current contracts

- `docs/1.0.1/language/NativeSharp_Language_Specification_1_0_1.md`
- `docs/1.0.1/language/NativeSharp_Grammar_1_0_1.ebnf`
- `docs/1.0.1/language/NativeSharp_1_0_1_conformance.map`
- `docs/1.0.1/compiler/NativeSharp_Compiler_Contract_1_0_1.md`
- `docs/1.0.1/compiler/NativeSharp_Compiler_1_0_1_conformance.map`
- `docs/1.0.1/debug/NativeSharp_Debug_Artifact_Contract_1_0_1.md`

Estos archivos derivan de los contratos canónicos existentes, pero sus encabezados, enlaces y
estado se actualizan para la edición 1.0.1. Los originales de `spec/` no se reescriben, por lo que la
historia 1.0 candidata permanece disponible.

These files derive from the existing canonical contracts, but their headings, links, and status are
updated for the 1.0.1 edition. Originals under `spec/` are not rewritten, preserving the candidate
1.0 history.

### Referencias extensas / Extended references

- `docs/1.0.1/book/NativeSharp_El_Lenguaje_de_Programacion_ES.md`
- `docs/1.0.1/book/Deitel_en_NativeSharp_ES.md`
- `docs/1.0.1/book/Drozdek_en_NativeSharp_ES.md`

La cabecera del libro principal se adapta a 1.0.1-dev y elimina la afirmación de que el contenido ya
fue recertificado con un build nuevo. Las referencias anteriores se preservan bajo `book/`.

The main book header is adapted to 1.0.1-dev and removes any claim that its contents have already
been recertified with a new build. Previous references remain preserved under `book/`.

## Empaquetado y verificación / Packaging and verification

`CMakeLists.txt` enumera recursivamente `docs` y lo instala en `share/nativesharp/docs`, conservando
el índice de versiones y la edición `1.0.1`. Sus archivos entran en `NativeSharpInstallation.files`, por lo que el
manifiesto criptográfico de la instalación puede detectar ausencia o modificación.

`CMakeLists.txt` recursively enumerates `docs` and installs it under `share/nativesharp/docs`,
preserving both the version index and the `1.0.1` edition. Its files enter
`NativeSharpInstallation.files`, allowing the
cryptographic installation manifest to detect missing or modified documentation.

`src/Driver/InstallationVerifier.cpp` exige el índice de versiones y seis anclas de 1.0.1: índice, contrato de versión,
especificación del lenguaje, contrato del compilador, contrato PDB y libro principal.
`tests/unit/Stage63InstallationVerifierTests.cpp` construye ese layout mínimo y verifica un error
preciso cuando falta el índice.

`src/Driver/InstallationVerifier.cpp` requires the version index and six 1.0.1 anchors: index, version contract, language
specification, compiler contract, PDB contract, and main book.
`tests/unit/Stage63InstallationVerifierTests.cpp` builds that minimum layout and verifies a precise
error when the index is missing.

`nscbuilder.py` incorpora `spec` y `docs` en la identidad de fuentes del próximo baseline. La
documentación instalada puede cambiar el producto distribuido y, por ello, ya no queda fuera de la
huella que se certificará posteriormente.

`nscbuilder.py` includes `spec` and `docs` in the next baseline's source identity. Installed
documentation can change the distributed product and therefore no longer remains outside the
fingerprint to be certified later.

## Pruebas actualizadas / Updated tests

- `tests/unit/Cc1BuildIdentityTests.cpp`: familia, canal visible y lenguaje 1.0.1.
- `tests/unit/Stage48_1CHeaderGeneratorTests.cpp`: mensaje del contrato host.
- `tests/unit/Stage48_2LibraryMetadataTests.cpp`: persistencia del productor y compatibilidad
  explícita de `1.0.1-dev+build`.
- `tests/unit/Stage62ToolchainContractTests.cpp`: `compiler_release` y `language`.
- `tests/unit/Stage63InstallationVerifierTests.cpp`: layout documental instalado.
- `tests/unit/Stage66E9E10ReleaseContractTests.cpp`: política y decisiones de compatibilidad 1.0.1.
- `tests/unit/LexerTests.cpp`, `ParserTests.cpp`, `SemanticTests.cpp`,
  `Stage46NativeLibrariesTests.cpp` y
  `Stage48_6CppCliHostClosureTests.cpp`: nombres vigentes en evidencia y mensajes.

## Validación / Validation

No se ejecutó baseline, no se compiló el compilador y no se instaló en `C:\native-sharp`. La entrega
se valida mediante revisión de coherencia textual, enlaces internos, sintaxis CMake de las secciones
modificadas y ausencia de identidades antiguas fuera de menciones históricas deliberadas. La
compilación y la certificación quedan como acciones posteriores explícitas.

No baseline was run, the compiler was not built, and nothing was installed into
`C:\native-sharp`. This delivery is validated through textual consistency, internal links, review of
the modified CMake sections, and absence of old identities outside deliberate historical mentions.
Build and certification remain separate, explicit later actions.

## Commit de referencia / Reference commit

### Título / Title

```text
Versiona NativeSharp como 1.0.1-dev y publica su documentación / Version NativeSharp as 1.0.1-dev and publish its documentation
```

### Descripción / Description

```text
ES:
- alinea compilador, lenguaje, diagnósticos y contratos con la línea 1.0.1
- conserva -dev y un build id único hasta la certificación por baseline
- crea una edición documental autocontenida bajo docs/1.0.1
- instala, inventaría y verifica la documentación vigente con el producto
- preserva intactas las ediciones históricas de spec y book

EN:
- align compiler, language, diagnostics, and contracts with the 1.0.1 line
- retain -dev and a unique build id until baseline-backed certification
- create a self-contained documentation edition under docs/1.0.1
- install, inventory, and verify current documentation with the product
- preserve historical spec and book editions unchanged
```
