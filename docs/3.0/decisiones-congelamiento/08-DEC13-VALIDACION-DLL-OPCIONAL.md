# Congelamiento 3.0 DEC-13 / D-09 — la validación de imports contra DLL del host es opcional

Fecha: 2026-10-26. Rama: `development-release-candidate`. Revisión de partida: `4bf4a7a18`.

## Propósito y problema

`d8687f28a` hizo que `createWindowsImportLibraries` (`src/Linker/WindowsRuntimeImports.cpp`) comprobara cada
símbolo de `extern C from "<dll>"` contra la tabla de exports de la DLL encontrada en System32, el directorio de
Windows o `PATH`. Hallazgo D-09 de la auditoría: con eso el resultado de compilar depende de las DLL instaladas en
el host (versión de Windows, contenido de `PATH`), y la comprobación corría en todo enlace de ejecutable y DLL.
Decisión de Alberto Huerta Luna (DEC-13, 2026-10-26): la validación física contra DLL del sistema debe ser
opcional y estar desactivada por defecto; la compilación no puede depender del host.

## SCOPE LOCK

- **Objetivo.** Sin opción, ninguna compilación lee DLL del host; con `--validate-system-imports` un símbolo que
  la DLL encontrada no exporta es error de compilación.
- **Naturaleza.** Corrección de contrato por decisión explícita.
- **Debe quedar idéntico.** Las import libraries generadas, la identidad de bundles sin la opción, el resto de
  opciones de enlace y `validateExternalImportExports` (mismo algoritmo y mismas exclusiones).
- **Modificable.** `src/Linker/WindowsRuntimeImports.cpp` (restauración), `src/Driver/*` de opciones, sesión,
  generación de artefactos e identidad, `ExternalImportValidation.h` (comentario) y
  `tests/unit/Freeze30ContractTests.cpp`.
- **Prohibido.** Cambiar firmas públicas de `Driver` o `createWindowsImportLibraries`; tocar pruebas históricas.
- **Terminación.** La sonda de abajo.

## Cambio

- `src/Linker/WindowsRuntimeImports.cpp` vuelve byte por byte a `d8687f28a~1`: el linker ya no valida.
- Opción nueva `--validate-system-imports` (`CompilerOptionParser.cpp`, campo
  `CompilerInvocation::validateSystemImports`). `validateCompilerLinkOptions` la acepta solo al emitir un
  ejecutable (`--emit-project-oop-exe`, `--emit-exe`, `nsc <file.ns> -o <file.exe>`) o una DLL
  (`--emit-project-oop-dll`); en otra acción es error de línea de comandos (exit 2).
- `parseAndConfigureInvocation` copia el valor a `DriverExecutionScope::validateSystemImports`. Así no cambian
  las firmas de `Driver::runProjectOop*Generation` ni de `runExecutableGeneration`.
- `prepareExecutableBuilder` y `prepareDynamicLibraryBuilder` llaman a `validateSystemImportsWhenRequested` con
  la lista completa (imports propios y heredados de bibliotecas precompiladas) antes de entregarla al builder.
  El error se reporta como `nsc: error: system import validation failed: ...`.
- `addCompilationLinkIdentity` agrega el campo `validate-system-imports=true` solo cuando se pide: un bundle
  construido sin la comprobación no satisface una compilación que la pide, y las identidades por defecto no
  cambian.
- `nsc build` acepta `--validate-system-imports` (`BuildOptions`) y lo reenvía a la compilación anidada.
- Ayuda en inglés y español.
- `Freeze30ContractTests`: `P5DllCalled` ahora compila sin la opción; `P5DllCalledValidated` se rechaza con
  `is not exported by 'kernel32.dll'`; `P5DllMissingValidated` compila con la opción (DLL no encontrada no se
  juzga); `P5DllMissing` sigue igual.

Las bibliotecas estáticas no validan: sus imports se heredan y se validan al enlazar el ejecutable o la DLL que
las consume, si ese enlace lo pide.

## Validación

nsc Release recompilado (exit 0). Sonda en `A:\temp\nsc-dec\d13` con `NoSuchFunctionAnywhere` de `kernel32.dll`:

| Invocación | Exit | Resultado |
|---|---|---|
| `nsc sym.ns -o sym-default.exe` | 0 | ejecutable publicado |
| `nsc --validate-system-imports sym.ns -o sym-checked.exe` | 1 | `... is not exported by 'kernel32.dll'` |
| `nsc --validate-system-imports miss.ns -o miss-checked.exe` (`nosuchlibrary_xyz.dll`) | 0 | no se juzga |
| `nsc --validate-system-imports --emit-obj sym.ns -o sym.obj` | 2 | opción no admitida para esa acción |
| `nsc build Bp.nsproj` | 0 | ejecutable publicado |
| `nsc build Bp.nsproj --validate-system-imports` (después) | 1 | rechazo; no reutiliza el bundle sin comprobación |

`nsc --help` muestra la opción. `nsc_component_tests` Debug y Release recompilados.

## No verificado

- `Freeze30ContractTests` (unitarias): no se compilan en este árbol; quedan para el cierre final.
- Batería completa (para el cierre final).
