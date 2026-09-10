# 176 — Identidad reproducible de fuentes PDB / Reproducible PDB source identity

## Resultado / Outcome

**ES.** NativeSharp ahora publica en CodeView/PDB una identidad verificable para cada fuente:
ruta UTF-8 normalizada, remapeo explícito de prefijos y checksum SHA-256 de los bytes exactos que
leyó el compilador. Dos checkouts físicos distintos producen el mismo objeto cuando usan el mismo
destino de remapeo, mientras que cambiar sólo los bytes de una fuente cambia la identidad del
artefacto.

**EN.** NativeSharp now publishes a verifiable identity for every source in CodeView/PDB: a
normalized UTF-8 path, explicit prefix remapping, and a SHA-256 checksum of the exact bytes loaded
by the compiler. Two physically different checkouts produce the same object when mapped to the
same published root, while changing source bytes alone changes the artifact identity.

Este slice cierra la parte del productor de `PDB-P1` y `NSC-DEBUG-005`. La política con la que el
debugger decide aceptar, localizar o rechazar una fuente sigue perteneciendo al consumidor.

This slice closes the producer side of `PDB-P1` and `NSC-DEBUG-005`. The policy used by a debugger
to accept, locate, or reject a source remains a consumer responsibility.

## Contrato de línea de comandos / Command-line contract

```text
-fdebug-prefix-map=<absolute-source-prefix>=<replacement-prefix>
```

- el origen debe ser absoluto y el reemplazo no puede estar vacío;
- la opción sólo modifica la identidad publicada, nunca el archivo que se abre;
- el matching respeta límites de componentes y el casing ASCII de Windows;
- gana el prefijo coincidente más largo; en empate gana la última opción;
- se aceptan varias opciones;
- sin coincidencia se publica la ruta absoluta normalizada;
- las rutas usan UTF-8, separadores `/` y letra de unidad Windows en mayúscula.

- the source must be absolute and the replacement cannot be empty;
- the option changes only the published identity, never the file being opened;
- matching respects component boundaries and Windows ASCII casing;
- the longest matching prefix wins; the last option wins ties;
- multiple options are accepted;
- without a match, the normalized absolute path is published;
- paths use UTF-8, `/` separators, and an uppercase Windows drive letter.

Ejemplo / Example:

```powershell
nsc --emit-project-oop-exe App.nsproj -g -O2 `
  -fdebug-prefix-map=A:/work/private/App=/_/src/App -o App.exe
```

El PDB registra `/_/src/App/...` y conserva el SHA-256 del archivo real cargado desde
`A:/work/private/App/...`.

The PDB records `/_/src/App/...` while preserving the SHA-256 of the real file loaded from
`A:/work/private/App/...`.

## Implementación / Implementation

### LLVM y lowering / LLVM and lowering

- `src/LLVM/LlvmCApi.h` declara la superficie LLVM 23 para
  `LLVMDIBuilderCreateFileWithChecksum` y `CSK_SHA256`.
- `src/LLVM/ProjectOopLlvmLowerer.cpp` normaliza y remapea la ruta, calcula SHA-256 sobre
  `SourceFile::buffer()` y crea cada `DIFile` con checksum.
- `include/nativesharp/LLVM/DebugPrefixMap.h` define el dato que cruza del driver al backend.
- El checksum se calcula sobre el buffer ya leído, no reabriendo la ruta; así no existe una
  ventana en la que el contenido depurado difiera del contenido compilado.

- `src/LLVM/LlvmCApi.h` declares the LLVM 23 surface for
  `LLVMDIBuilderCreateFileWithChecksum` and `CSK_SHA256`.
- `src/LLVM/ProjectOopLlvmLowerer.cpp` normalizes and remaps the path, computes SHA-256 over
  `SourceFile::buffer()`, and creates every `DIFile` with a checksum.
- `include/nativesharp/LLVM/DebugPrefixMap.h` defines the value transported from driver to backend.
- The checksum is computed from the already-loaded buffer instead of reopening the path, avoiding
  a window where debug identity could describe bytes other than those compiled.

### Autoridad SHA-256 / SHA-256 authority

La implementación ya probada de SHA-256 se movió de `LibraryMetadata.cpp` a
`src/Basic/Sha256.cpp`. `nsc_frontend` la contiene ahora para que metadatos, empaquetado y lowering
PDB usen una sola autoridad sin crear una dependencia circular del backend hacia el driver.

The proven SHA-256 implementation moved from `LibraryMetadata.cpp` to
`src/Basic/Sha256.cpp`. `nsc_frontend` now owns it so metadata, packaging, and PDB lowering share a
single authority without introducing a backend-to-driver dependency cycle.

### Driver y documentación normativa / Driver and normative documentation

`src/Driver/Driver.cpp` valida, conserva y propaga todos los mapas. La ayuda pública y
`spec/NativeSharp_Debug_Artifact_Contract_1_0.md` fijan sintaxis, precedencia, normalización y el
hecho de que el remapeo no cambia la entrada física.

`src/Driver/Driver.cpp` validates, preserves, and propagates every map. Public help and
`spec/NativeSharp_Debug_Artifact_Contract_1_0.md` define syntax, precedence, normalization, and the
fact that remapping does not alter physical input selection.

## Evidencia / Evidence

El fixture PDB ahora es multiarchivo y contiene dos archivos llamados `Shared.ns` en directorios
distintos, con contenido y checksum diferentes. El gate `nsc.debug.codeview-pdb` comprueba:

- EXE/PDB O0 y O2, ambos ejecutados con código de salida `42`;
- rutas publicadas bajo `/_/nativesharp/debug-pdb`, sin filtrar `NSC-DEVEL-BRANCHES`;
- SHA-256 exacto de `Main.ns`, `left/Shared.ns` y `right/Shared.ns`;
- coexistencia correcta de dos basenames iguales;
- igualdad byte a byte de objetos creados desde dos raíces físicas distintas con el mismo remapeo;
- cambio del objeto CodeView cuando se mutan sólo los bytes de una fuente;
- conservación de símbolos, tipos, scopes, rangos y líneas existentes.

The PDB fixture is now multi-file and contains two files named `Shared.ns` in separate directories,
with distinct content and checksums. The `nsc.debug.codeview-pdb` gate verifies:

- O0 and O2 EXE/PDB pairs, both executed with exit code `42`;
- published paths under `/_/nativesharp/debug-pdb`, without leaking `NSC-DEVEL-BRANCHES`;
- exact SHA-256 for `Main.ns`, `left/Shared.ns`, and `right/Shared.ns`;
- correct coexistence of two equal basenames;
- byte-identical objects built from two physical roots under the same remapping;
- a changed CodeView object when only source bytes are mutated;
- preservation of existing symbols, types, scopes, ranges, and lines.

La configuración y todos los artefactos de verificación viven en
`A:/NSC-DEVEL-BRANCHES/nsc-support-pdb/_pdb_local`. No se instaló ni se modificó ningún paquete en
`C:`. El test de endurecimiento también se hizo robusto a mapas de conformidad materializados con
LF o CRLF; el fallo era exclusivo de este checkout nuevo y no del compilador.

The configuration and all verification artifacts live under
`A:/NSC-DEVEL-BRANCHES/nsc-support-pdb/_pdb_local`. No package was installed or modified on `C:`.
The hardening test was also made robust to conformance maps materialized with either LF or CRLF;
the failure was specific to this new checkout and not to the compiler.

Evidencia final / Final evidence:

- build Release de `nsc` con warnings como errores: correcto / successful;
- build del target `pruebas`: 135 ejecutables / executables;
- suites dirigidas de lowering, SHA-256/NSMETA, manifiesto, driver y PDB: 5/5;
- CTest local completo: 307/307, 234.02 s;
- identidad del compilador verificado: `47d481fa7588fb0f`;
- instalación en `C:`: no ejecutada / not run.

## Límites / Limits

- La aceptación interactiva de PDB-P0 en NativeSharp Debugger, Visual Studio y WinDbg continúa
  siendo una verificación humana y del consumidor.
- La identidad del PDB que decide el linker se compara semánticamente mediante RSDS; el gate de
  reproducibilidad byte a byte se aplica al objeto COFF controlado por el productor.
- El remapeo conserva bytes Unicode, pero el folding sin distinción de mayúsculas se limita al
  rango ASCII usado por raíces y unidades Windows.
- NSLink continúa fuera de este carril.

- Interactive PDB-P0 acceptance in NativeSharp Debugger, Visual Studio, and WinDbg remains a human
  and consumer-side verification.
- Linker-owned PDB identity is compared semantically through RSDS; byte-for-byte reproducibility is
  asserted for the producer-controlled COFF object.
- Remapping preserves Unicode bytes, while case-insensitive folding is limited to the ASCII range
  used by Windows roots and drive letters.
- NSLink remains outside this lane.

## Commit / Commit message

**Título ES:** Agrega identidad reproducible de fuentes PDB

**Title EN:** Add reproducible PDB source identity

**Descripción ES:** Emite checksums SHA-256 sobre los bytes compilados, agrega remapeo normalizado
de prefijos de fuente y amplía el gate con rutas privadas, multiarchivo, basenames duplicados,
fuente modificada y reproducibilidad entre checkouts.

**Description EN:** Emit SHA-256 checksums over compiled bytes, add normalized source-prefix
remapping, and extend the gate with private-path, multi-file, duplicate-basename, modified-source,
and cross-checkout reproducibility coverage.
