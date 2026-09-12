# 218 — PDB para productos de biblioteca / PDB support for library products

## Resultado / Outcome

**ES.** Los perfiles `-g0`, `-gline-tables-only` y `-g` ya no terminan en la frontera de objeto o
ejecutable. También gobiernan productos `StaticLibrary` y `SharedLibrary`. Una biblioteca estática
con depuración conserva CodeView dentro de sus miembros COFF para que el enlazador del consumidor
pueda incorporarlo a su PDB final. Una DLL con depuración produce `<producto>.pdb` y registra en el
PE la identidad RSDS decidida por el enlazador con nombre de archivo portable.

**EN.** The `-g0`, `-gline-tables-only`, and `-g` profiles no longer stop at object or executable
boundaries. They now govern `StaticLibrary` and `SharedLibrary` products as well. A debug static
library retains CodeView in its COFF members so the consumer linker can merge it into the final
PDB. A debug DLL produces `<product>.pdb` and records the linker-owned RSDS identity in the PE with
a portable file name.

## Problema cerrado / Closed problem

La ruta de bibliotecas llamaba el overload mínimo de `ProjectOopLlvmLowerer::lower`, que no recibe
`SourceManager`, modelo de miembros, perfil de depuración ni remapeos de rutas. Aun cuando el usuario
escribiera `-g`, el driver rechazaba la opción antes de compilar. La ruta DLL, además, invocaba
`lld-link` sin `/debug:full`, `/pdb:` ni `/pdbaltpath:`.

The library path called the minimal `ProjectOopLlvmLowerer::lower` overload, which receives no
`SourceManager`, member model, debug profile, or path remapping. Even when the user supplied `-g`,
the driver rejected the option before compilation. The DLL path also invoked `lld-link` without
`/debug:full`, `/pdb:`, or `/pdbaltpath:`.

## Superficie implementada / Implemented surface

### Driver

- `Driver::run` clasifica `--emit-project-oop-lib` y `--emit-project-oop-dll` como acciones capaces
  de producir depuración.
- Los dos comandos aceptan los tres perfiles y `-fdebug-prefix-map` con la misma semántica que
  objetos y ejecutables.
- `runProjectOopStaticLibraryGeneration` y `runProjectOopDynamicLibraryGeneration` reciben y
  transportan el perfil y los remapeos.
- Ambos caminos usan el lowering completo con `ClassMemberModel` y `SourceManager`; no existe una
  segunda interpretación de ubicaciones o nombres depurables.
- La salida textual declara el perfil y el artefacto real: CodeView en miembros del archive para
  `.lib`, o ruta PDB para `.dll`.

- `Driver::run` classifies `--emit-project-oop-lib` and `--emit-project-oop-dll` as debug-capable
  actions.
- Both commands accept all three profiles and `-fdebug-prefix-map` with the same semantics as
  objects and executables.
- `runProjectOopStaticLibraryGeneration` and `runProjectOopDynamicLibraryGeneration` carry the
  profile and remaps through the pipeline.
- Both paths use the full lowering entry point with `ClassMemberModel` and `SourceManager`; no
  second interpretation of debug locations or names was introduced.
- Command output identifies the actual product: CodeView-bearing archive members for `.lib`, or
  the PDB path for `.dll`.

### Biblioteca estática / Static library

No se inventa un PDB autónomo para un archive, porque un `.lib` no representa una imagen enlazada
y no posee todavía direcciones finales, contribuciones seleccionadas ni una identidad RSDS. El
contrato correcto es conservar `.debug$S`/`.debug$T` CodeView en los objetos archivados. Cuando un
EXE o DLL consume el archive, el enlazador selecciona miembros y construye un solo PDB para la
imagen final.

No standalone PDB is fabricated for an archive because a `.lib` is not a linked image and does not
yet own final addresses, selected contributions, or an RSDS identity. The correct contract is to
retain `.debug$S`/`.debug$T` CodeView in archived objects. When an EXE or DLL consumes the archive,
the linker selects members and builds one PDB for the final image.

### DLL

`WindowsLibraryBuilder::buildDynamic` deriva `<producto>.pdb` únicamente cuando el perfil no es
`none`. `LldLinker::linkDynamicLibrary` recibe esa ruta explícita y añade:

```text
/debug:full
/pdb:<producto>.pdb
/pdbaltpath:%_PDB%
```

`/pdbaltpath:%_PDB%` evita incrustar una ruta absoluta de la máquina de build. Después de un éxito,
la ausencia del PDB se considera fallo del producto. Todos los caminos de error eliminan DLL,
import library y PDB parcial en conjunto.

`WindowsLibraryBuilder::buildDynamic` derives `<product>.pdb` only when the profile is not `none`.
`LldLinker::linkDynamicLibrary` receives that path explicitly and adds the switches above.
`/pdbaltpath:%_PDB%` keeps an absolute build-machine path out of the image. After linker success, a
missing PDB is a product failure. Every error path removes the DLL, import library, and partial PDB
as one artifact set.

## Archivos y funciones / Files and functions

| Archivo / File | Cambio / Change |
|---|---|
| `include/nativesharp/Driver/Driver.h` | Amplía las firmas de emisión de bibliotecas con perfil y remapeos de depuración. |
| `src/Driver/Driver.cpp` | Acepta opciones, usa lowering completo, transporta identidad PDB y reporta los artefactos. |
| `include/nativesharp/Linker/WindowsLibraryBuilder.h` | Declara el perfil requerido por `buildDynamic`. |
| `src/Linker/WindowsLibraryBuilder.cpp` | Deriva el PDB hermano de la DLL y lo entrega al linker. |
| `include/nativesharp/Linker/LldLinker.h` | Hace explícita la ruta PDB en el contrato de enlace DLL. |
| `src/Linker/LldLinker.cpp` | Solicita PDB/RSDS y aplica validación y limpieza transaccional. |
| `tests/unit/Stage46NativeLibrariesTests.cpp` | Agrega casos `-g` para archive CodeView y DLL/PDB. |

## Decisiones de diseño / Design decisions

1. **Un perfil, todas las salidas.** `-g` conserva exactamente el significado fijado por el contrato
   CodeView; no existe un modo especial para bibliotecas.
2. **El enlazador posee GUID/age.** NSC solicita el PDB y verifica su existencia, pero no fabrica ni
   duplica la identidad que pertenece al enlazador.
3. **Un archive transporta CodeView, no una falsa imagen.** Esto conserva la selección COMDAT y de
   miembros que sólo puede resolverse en el consumidor.
4. **Conjunto transaccional.** Una DLL depurable incompleta no se publica como éxito.
5. **Sin formato privado.** Todo el handoff continúa siendo COFF CodeView, PDB/MSF y RSDS estándar.

1. **One profile across outputs.** `-g` keeps the exact meaning established by the CodeView
   contract; libraries get no private debug mode.
2. **The linker owns GUID/age.** NSC requests and checks the PDB but does not manufacture or
   duplicate linker-owned identity.
3. **An archive transports CodeView, not a fake image.** This preserves COMDAT and member selection
   that can only be resolved by the consumer.
4. **Transactional artifact set.** An incomplete debug DLL is never reported as successful.
5. **No private format.** The handoff remains standard COFF CodeView, PDB/MSF, and RSDS.

## Verificación de esta entrega / Verification for this delivery

- `git diff --check` limpio para todos los archivos de la entrega.
- Se agregaron regresiones que ejercen la aceptación de `-g`, un archive COFF válido con handoff
  CodeView, creación no vacía de PDB para DLL y reporte de la ruta hermana.
- De acuerdo con el ciclo de desarrollo acordado, las pruebas no se ejecutan en este commit. El gate
  inmediato posterior al commit es una reconstrucción limpia de todo el compilador. El baseline se
  reserva para el cierre acumulativo después de la entrega 234.

- `git diff --check` is clean for every delivery file.
- Regression coverage exercises `-g` acceptance, a valid COFF archive with CodeView handoff,
  non-empty DLL PDB creation, and sibling-path reporting.
- Under the agreed development cycle, tests are not run for this commit. The immediate post-commit
  gate is a clean full compiler rebuild. The baseline is reserved for the cumulative closure after
  delivery 234.

## Límites conscientes / Deliberate limits

- La comprobación semántica GUID/age, módulos, fuentes y checksums pertenece a la entrega 219.
- El corpus amplio de variables, stacks, interfaces, closures y EH pertenece a la entrega 220.
- La aceptación interactiva en el debugger sigue siendo un gate humano y no autoriza modificar su
  repositorio desde NSC.

- Semantic GUID/age, module, source, and checksum verification belongs to delivery 219.
- The broad variables, stacks, interfaces, closures, and EH corpus belongs to delivery 220.
- Interactive debugger acceptance remains a human gate and does not authorize changes to its
  repository from NSC.

## Commit de referencia / Reference commit

**Título ES:** Completa PDB en productos de biblioteca

**Title EN:** Complete PDB support for library products

**Descripción ES:** Propaga los perfiles CodeView y los remapeos de fuente a bibliotecas estáticas
y DLL, conserva información depurable en miembros COFF, genera un PDB hermano con RSDS portable
para DLL y trata DLL/import library/PDB como un conjunto transaccional.

**Description EN:** Propagate CodeView profiles and source remaps into static libraries and DLLs,
retain debug information in COFF archive members, generate a sibling PDB with portable RSDS for
DLLs, and treat the DLL/import-library/PDB files as one transactional artifact set.
