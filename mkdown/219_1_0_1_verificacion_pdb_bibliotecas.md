# 219 — Verificación PDB de bibliotecas / Library PDB verification

## Resultado / Outcome

**ES.** `--verify-library-artifacts` puede recibir ahora el conjunto
`NSMETA + LIB + DLL + PDB` y certificar que los cuatro archivos pertenecen al mismo producto. La
verificación ya no acepta por existencia un archivo llamado `.pdb`: analiza MSF 7.00, recupera la
identidad del stream PDB, recupera el registro RSDS del PE y compara GUID, age y basename. También
exige evidencia de módulos, símbolos públicos, fuentes y checksums CodeView.

**EN.** `--verify-library-artifacts` can now receive the
`NSMETA + LIB + DLL + PDB` set and certify that all four files belong to the same product. A file
named `.pdb` is no longer accepted by existence alone: verification parses MSF 7.00, recovers the
PDB stream identity, recovers the PE RSDS record, and compares GUID, age, and basename. It also
requires evidence of modules, public symbols, sources, and CodeView checksums.

## Uso / Usage

```text
nsc --verify-library-artifacts Product.nsmeta Product.lib Product.dll Product.pdb
```

Las formas anteriores continúan vigentes para productos sin PDB:

```text
nsc --verify-library-artifacts Product.nsmeta Product.lib
nsc --verify-library-artifacts Product.nsmeta Product.lib Product.dll
```

## Implementación / Implementation

### Lector MSF/PDB acotado

Se agregó `PdbVerifier`, un lector de validación, no un depurador ni un escritor PDB. Comprueba:

1. firma `Microsoft C/C++ MSF 7.00`;
2. tamaño, potencia de dos y rango del bloque MSF;
3. mapa de bloques del directorio;
4. tamaños, índices y listas de bloques de streams;
5. stream 1 de información PDB con versión, signature, age y GUID;
6. stream 3 DBI con firma válida;
7. substream de módulos;
8. stream de símbolos públicos no vacío;
9. información de archivos fuente;
10. al menos una subsección CodeView `DEBUG_S_FILECHKSMS` bien delimitada.

`PdbVerifier` deliberately is a bounded validation reader, not a debugger or PDB writer. It checks
the MSF 7.00 signature, superblock, directory map, stream topology, PDB information identity, DBI
signature, module substream, public-symbol stream, source-file information, and at least one
well-bounded CodeView `DEBUG_S_FILECHKSMS` subsection.

### Registro RSDS del PE

`PeExecutableVerifier` conserva ahora el directorio debug y recorre entradas
`IMAGE_DEBUG_DIRECTORY`. Para una entrada CodeView RSDS valida tamaño y límites y publica:

- GUID de 16 bytes;
- `age`;
- nombre PDB terminado en cero;
- presencia inequívoca de una sola identidad RSDS.

`PeExecutableVerifier` now retains the debug directory and walks `IMAGE_DEBUG_DIRECTORY` entries.
For a CodeView RSDS entry it validates bounds and exposes the 16-byte GUID, age, null-terminated PDB
name, and the unambiguous presence of exactly one RSDS identity.

### Concordancia del conjunto

El driver rechaza:

- PDB sin DLL;
- DLL sin RSDS cuando se solicitó verificar PDB;
- PDB corrupto o truncado;
- GUID o `age` distintos;
- nombre RSDS con ruta absoluta o basename diferente;
- PDB sin módulos, públicos, fuentes o checksums.

The driver rejects a PDB without its DLL, a DLL without RSDS when PDB verification was requested,
truncated/corrupt PDB data, different GUID or age, an absolute or mismatched RSDS name, and a PDB
without module, public, source, or checksum evidence.

## Archivos y funciones / Files and functions

| Archivo / File | Elemento / Element | Responsabilidad / Responsibility |
|---|---|---|
| `include/nativesharp/Linker/PdbVerifier.h` | `PdbInfo`, `PdbVerifier` | Contrato público del lector acotado. |
| `src/Linker/PdbVerifier.cpp` | `PdbVerifier::verify` | Reconstruye directorio/streams MSF y clasifica la superficie debug. |
| `include/nativesharp/Linker/PeExecutableVerifier.h` | `PeExecutableInfo` | Expone directorio debug e identidad RSDS. |
| `src/Linker/PeExecutableVerifier.cpp` | `verifyImage` | Valida `IMAGE_DEBUG_DIRECTORY` y CodeView RSDS. |
| `include/nativesharp/Driver/Driver.h` | `verifyLibraryArtifacts` | Amplía el contrato con PDB opcional. |
| `src/Driver/Driver.cpp` | dispatch y verificación | Compara PE/PDB y emite diagnóstico de superficie incompleta. |
| `tests/unit/Stage46NativeLibrariesTests.cpp` | regresiones PDB | Cubre producto válido y PDB corrupto. |
| `CMakeLists.txt` | target `nsc_linker` | Incorpora el lector PDB al compilador. |

## Decisiones de diseño / Design decisions

1. **Comparar bytes de identidad, no texto formateado.** El GUID mantiene el orden exacto de RSDS y
   del stream PDB; se evita una conversión textual con reglas endian ambiguas.
2. **No ejecutar herramientas externas.** La verificación sigue funcionando desde la instalación
   de NSC sin depender de `llvm-pdbutil` en el PATH.
3. **Lectura sin mutación.** El verificador no repara, reescribe ni normaliza artefactos.
4. **Basename portable obligatorio.** Una ruta de la máquina productora dentro de RSDS convierte el
   paquete en no relocatable y se rechaza.
5. **Evidencia estructural, no promesas.** La presencia de módulos/fuentes/checksums se deriva de
   streams reales.

1. **Compare identity bytes, not formatted text.** GUID bytes retain their exact RSDS/PDB-stream
   order and avoid ambiguous textual endian rules.
2. **No external tools.** Installed NSC can verify products without `llvm-pdbutil` on PATH.
3. **Read-only validation.** Verification never repairs, rewrites, or normalizes an artifact.
4. **Portable basename required.** A producer-machine path in RSDS makes a package non-relocatable.
5. **Structural evidence, not declarations.** Module/source/checksum presence comes from real PDB
   streams.

## Cobertura agregada / Added coverage

- PDB real producido por una DLL `-g`.
- Identidad no nula y superficie completa reconocida por `PdbVerifier`.
- Verificación acumulativa de NSMETA, import library, exports DLL y PDB.
- Nombre RSDS esperado `NativeMathDebug.pdb`.
- Archivo con contenido ajeno a MSF rechazado con diagnóstico.

- A real PDB emitted by a `-g` DLL.
- Non-zero identity and complete surface recognized by `PdbVerifier`.
- Cumulative NSMETA, import-library, DLL-export, and PDB verification.
- Expected `NativeMathDebug.pdb` RSDS name.
- Non-MSF content rejected with a diagnostic.

Las regresiones quedan incorporadas pero no se ejecutan durante la iteración. El gate posterior al
commit es sólo la reconstrucción limpia del compilador; el baseline acumulativo se ejecuta después
de la entrega 234.

The regressions are included but are not run during iteration. The post-commit gate is only a clean
compiler rebuild; the cumulative baseline runs after delivery 234.

## Commit de referencia / Reference commit

**Título ES:** Verifica la identidad PDB de bibliotecas

**Title EN:** Verify library PDB identity

**Descripción ES:** Agrega un lector acotado MSF/PDB, extrae RSDS del PE y amplía la verificación de
artefactos para comparar GUID, age y basename y exigir módulos, públicos, fuentes y checksums.

**Description EN:** Add a bounded MSF/PDB reader, extract PE RSDS data, and extend artifact
verification to compare GUID, age, and basename while requiring modules, publics, sources, and
checksums.
