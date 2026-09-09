# 168 — Primera ruta ejecutable PDB/CodeView / First executable PDB/CodeView path

## Resultado / Outcome

`nsc` puede emitir información CodeView estándar desde proyectos OOP y producir un PDB enlazado
para ejecutables Windows x64. La selección de depuración es independiente de `-O`; el gate cubre
O0 y O2.

`nsc` can emit standard CodeView information from OOP projects and produce a linked PDB for
Windows x64 executables. Debug selection is independent from `-O`; the gate covers O0 and O2.

Esta entrega cierra el primer slice vertical de archivo, función y línea. No declara terminado el
perfil completo: parámetros, locales, tipos NativeSharp, checksums, path remapping y defranges
siguen como trabajo explícito posterior.

This delivery closes the first file/function/line vertical slice. It does not claim the complete
profile is finished: parameters, locals, NativeSharp types, checksums, path remapping and defranges
remain explicit follow-up work.

## Archivos creados / Files created

### `include/nativesharp/LLVM/DebugInformationKind.h`

Define `None`, `LineTablesOnly` y `Full` como selección separada de
`LlvmOptimizationLevel`, además de un nombre estable para diagnósticos y salida CLI.

Defines `None`, `LineTablesOnly` and `Full` independently from `LlvmOptimizationLevel`, plus a
stable name for diagnostics and CLI output.

### `src/LLVM/DebugInformationKind.cpp`

Implementa la representación textual estable de los tres perfiles.

Implements the stable textual representation of all three profiles.

### `tests/integration/debug_pdb/DebugPdb.nsproj`

Proyecto mínimo y sin dependencias usado exclusivamente para comprobar artefactos de depuración.

Minimal dependency-free project used exclusively to verify debug artifacts.

### `tests/integration/debug_pdb/Main.ns`

Función `DebugFixture.Program.Main` con líneas ejecutables distinguibles antes y después de
optimizar.

Provides `DebugFixture.Program.Main` with executable lines that can be distinguished before and
after optimization.

### `cmake/VerifyNativeSharpDebugArtifacts.cmake`

Compila el fixture como objeto con `-gline-tables-only` y como ejecutable con `-g` en O0/O2. El
validador exige:

- sección `.debug$S` en COFF;
- `.exe` y `.pdb` juntos;
- registro RSDS con nombre de PDB sin ruta absoluta;
- resumen PDB con debug real;
- módulo, archivo `Main.ns`, función visible y líneas ejecutables;
- soporte de precedencia de opciones mediante `-g0 -g`.

Compiles the fixture as a `-gline-tables-only` object and as a `-g` executable at O0/O2. The
validator requires:

- a `.debug$S` COFF section;
- paired `.exe` and `.pdb` files;
- an RSDS record with a basename-only PDB name;
- a PDB summary containing real debug information;
- the module, `Main.ns`, visible function and executable lines;
- option precedence through `-g0 -g`.

## Archivos modificados / Files modified

### `src/LLVM/LlvmCApi.h`

Agrega únicamente el subconjunto LLVM 23 requerido por DIBuilder: metadata, compile unit, file,
subprogram, subroutine type, locations, lexical file scopes y module flags CodeView.

Adds only the LLVM 23 DIBuilder subset required for metadata, compile units, files, subprograms,
subroutine types, locations, lexical file scopes and CodeView module flags.

### `include/nativesharp/LLVM/ProjectOopLlvmLowerer.h`

Conserva el overload histórico sin debug y agrega un overload explícito que recibe miembros,
fuentes, perfil debug y optimización.

Keeps the historical no-debug overload and adds an explicit overload receiving members, sources,
debug profile and optimization.

### `src/LLVM/ProjectOopLlvmLowerer.cpp`

- crea `DICompileUnit`/`DIFile` y los module flags `Debug Info Version` y `CodeView`;
- crea un `DISubprogram` por función de usuario con nombre visible y linkage name separados;
- traduce los rangos de sintaxis MIR a línea/columna;
- restaura ubicaciones al salir de expresiones anidadas;
- marca prólogos y código sintético con línea cero para satisfacer el verificador sin convertirlos
  en pasos de usuario;
- finaliza y libera DIBuilder antes de verificar el módulo.

- creates `DICompileUnit`/`DIFile` plus `Debug Info Version` and `CodeView` module flags;
- creates one `DISubprogram` per user function with separate display and linkage names;
- maps MIR syntax ranges to line/column locations;
- restores locations after nested expressions;
- marks prologues and synthetic code as line zero, satisfying LLVM without exposing them as user
  steps;
- finalizes and disposes DIBuilder before module verification.

### `include/nativesharp/Driver/Driver.h` y `src/Driver/Driver.cpp`

Implementan `-g0`, `-gline-tables-only` y `-g`, con última opción ganadora, salida explícita del
perfil y validación de acciones soportadas. El perfil viaja hasta el lowering y el constructor PE
sin derivarse de `-O`.

Implement `-g0`, `-gline-tables-only` and `-g`, with last-option-wins semantics, explicit profile
output and supported-action validation. The profile reaches lowering and PE construction without
being inferred from `-O`.

### `include/nativesharp/Linker/LldLinker.h` y `src/Linker/LldLinker.cpp`

Agregan una variante compatible que acepta ruta PDB. Con debug habilitado, lld-link recibe
`/debug:full`, `/pdb:` y `/pdbaltpath:%_PDB%`; fallos limpian artefactos parciales y éxito exige que
el PDB exista.

Add a compatible overload accepting a PDB path. With debugging enabled, lld-link receives
`/debug:full`, `/pdb:` and `/pdbaltpath:%_PDB%`; failures clean partial artifacts and success
requires the PDB to exist.

### `include/nativesharp/Linker/WindowsExecutableBuilder.h` y
`src/Linker/WindowsExecutableBuilder.cpp`

Propagan el perfil y derivan `<nombre>.pdb` de `<nombre>.exe` sólo cuando debug está habilitado.

Propagate the profile and derive `<name>.pdb` from `<name>.exe` only when debugging is enabled.

### `tests/unit/DriverTests.cpp`

Comprueba que la ayuda publica los perfiles y que una variante `-g` desconocida falla como error
de línea de comandos con alternativas concretas.

Verifies that help publishes the profiles and an unknown `-g` variant fails as a command-line
error with concrete alternatives.

### `_abi/gates/G00/gate.json`

Registra la certificación reproducible de la corrida G00 aceptada, incluyendo identidad del
compilador, ambas matrices de linker y cero problemas.

Records the reproducible certification for the accepted G00 run, including compiler identity,
both linker matrices and zero problems.

### `CMakeLists.txt`

Compila el nuevo tipo, descubre `llvm-pdbutil`/`llvm-readobj` y registra
`nsc.debug.codeview-pdb` como gate automatizado. El gate fija explícitamente `lld-link`, porque
esta entrega implementa el PDB estándar del carril LLVM/PE y la compatibilidad de NSLink se
mantiene como un carril independiente.

Builds the new type, discovers `llvm-pdbutil`/`llvm-readobj`, and registers
`nsc.debug.codeview-pdb` as an automated gate. The gate explicitly selects `lld-link`, because
this delivery implements the standard PDB path for the LLVM/PE lane while NSLink compatibility
remains an independent lane.

## Por qué esta implementación / Why this implementation

El flujo usa exclusivamente APIs y formatos estándar de LLVM/Windows. Mantener la fuente en el
MIR evita un mapa externo que podría desincronizarse; conservar nombre visible y linkage name
permite UI legible sin perder resolución exacta de símbolos. La línea cero para instrucciones
sintéticas respeta simultáneamente el verificador LLVM y la experiencia de stepping.

The flow uses only standard LLVM/Windows APIs and formats. Keeping source identity in MIR avoids an
external map that could drift; separate display and linkage names give readable UI without losing
exact symbol resolution. Line zero for synthetic instructions satisfies both LLVM verification and
the stepping experience.

## Evidencia ejecutada / Executed evidence

- build rápido del target `nsc` en `build-tests`;
- fixture manual grande contra NativeSharp.Core;
- `llvm-readobj --coff-debug-directory`: CodeView, RSDS, GUID, age y `PDBFileName` basename-only;
- `llvm-pdbutil`: módulo, 23 fuentes, procedimientos, frame records y líneas;
- `ctest -R "^(nsc.driver|nsc.debug.codeview-pdb)$"`: `2/2` verdes en las configuraciones
  lld-link y NSLink;
- baseline oficial G00 `20260908-232949-c9be8a0a`: `307/307` CTest, `148/148`
  fixtures y `18/18` casos de triage, tanto con lld-link como con NSLink; `0` fallos y estado
  `LANDED` en 676.976 s.

La primera corrida completa reveló que el gate PDB nuevo heredaba NSLink mediante
`NSC_LLD_LINK`, aunque este slice pertenece a la ruta PDB estándar de lld-link. El gate se acotó
explícitamente a lld-link, se volvió a probar desde ambas configuraciones y después fue aceptado
por la corrida G00 completa indicada arriba.

The first full run exposed that the new PDB gate inherited NSLink through `NSC_LLD_LINK`, even
though this slice targets the standard lld-link PDB path. The gate was scoped explicitly to
lld-link, retested from both build configurations and then accepted by the complete G00 run above.

## Commit de referencia / Reference commit

**Título ES:** Agrega la primera ruta estándar de depuración PDB/CodeView
**Title EN:** Add the first standard PDB/CodeView debug path

**Descripción ES:** Separa perfiles `-g` de optimización, emite compile units, archivos,
subprogramas y líneas CodeView desde el MIR, enlaza un PDB con RSDS basename-only y agrega un gate
O0/O2 validado con llvm-readobj y llvm-pdbutil.

**Description EN:** Separates `-g` profiles from optimization, emits CodeView compile units, files,
subprograms and lines from MIR, links a PDB with a basename-only RSDS record, and adds an O0/O2 gate
validated by llvm-readobj and llvm-pdbutil.
