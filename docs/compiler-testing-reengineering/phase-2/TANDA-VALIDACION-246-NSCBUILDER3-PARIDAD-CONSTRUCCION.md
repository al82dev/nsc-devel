# Validación 246 — Paridad completa de construcción de NSC en Builder 3

Fecha: 2026-10-10

## SCOPE LOCK

### Objetivo exacto

Auditar literalmente la configuración y construcción de NSC en `nscbuilder.py`,
`nscbuilder2.py` y `nscbuilder3.py`; restaurar en Builder 3 la procedencia funcional de la raíz
LLVM usada por el builder autoritativo y retirar la adaptación posterior de CMake.

### Presupuesto

| Dimensión | Autorizado |
|---|---|
| Repositorio | `A:\nsc-devel` |
| Código | `nscbuilder3.py`, sólo construcción de NSC |
| Producto | `CMakeLists.txt`, exclusivamente el hunk introducido por `6c4c5a516` |
| Documentación | Este documento y `phase-2/TRACKING.md` |
| Validación | Auditoría estática y diferencial |

Todo lo demás permanece protegido. Está prohibido configurar, compilar o ejecutar.

## Evidencia causal

Las corridas `20261010-205612-3820d100` y `20261010-211432-87c4102f` usaron:

```text
CMAKE_C_COMPILER=A:/apps/msys64/mingw64/bin/clang-cl.exe
LLVM_DIR=A:/apps/msys64/mingw64/lib/cmake/llvm
```

La segunda corrida ya contenía la corrección `d6e090ed2` y falló igual. Por tanto,
`CMAKE_MAKE_PROGRAM` y `--config` eran diferencias reales, pero no explicaban el cambio de paquete.

La causa confirmada estaba antes del ensamblado del comando:

- `nscbuilder.py` modela una **raíz LLVM**. Su precedencia es override `--llvm-root`, entorno
  `NSC_LLVM_ROOT`, archivo de rutas y finalmente `C:\clang-llvm`. Todas las herramientas y
  `LLVM_DIR` se derivan de esa única raíz.
- `nscbuilder2.py` no configura ni construye NSC; consume un compilador ya seleccionado y no define
  un contrato alternativo.
- Builder 3 modelaba y persistía `_nscbuilder3/config.json:llvm_dir`; priorizaba ese directorio de
  paquete y también derivaba de sus ancestros Clang, LLD, MT y RC. Esa fuente no existe en el
  contrato de Builder 1 y permitió que construir NSC cambiara de toolchain y paquete.

El commit `6c4c5a516` relajó después `CMakeLists.txt` para admitir el paquete distinto. Esa adaptación
modificó el producto en vez de restaurar la procedencia del builder y queda retirada mediante un
commit nuevo, sin reescribir el historial.

## Tabla literal de paridad — configuración

| Posición/argumento | Builder 1 autoritativo | Builder 2 | Builder 3 restaurado | Estado |
|---|---|---|---|---|
| ejecutable CMake | `cmake` resuelto por `PATH` | no construye NSC | `cmake` resuelto por `PATH` | paridad efectiva |
| `-S` | raíz del repositorio del plan | no aplica | `SOURCE_ROOT` | paridad |
| `-B` | `compiler_build_dir` | no aplica | `compiler_build_root`; sufijo `-debug` sólo para Debug | paridad Release; extensión Debug autorizada |
| `-G` | `Ninja` | no aplica | `Ninja` | paridad |
| `CMAKE_MAKE_PROGRAM` | ausente | no aplica | ausente desde `d6e090ed2` | paridad |
| `CMAKE_BUILD_TYPE` | `Release` | no aplica | modo seleccionado; predeterminado `Release` | paridad predeterminada; extensión Debug autorizada |
| `CMAKE_INSTALL_PREFIX` | prefijo explícito del plan | no aplica | prefijo explícito persistido por la GUI | paridad |
| `CMAKE_C_COMPILER` | `<llvm-root>/bin/clang-cl.exe` | no aplica | `<compiler-llvm-root>/bin/clang-cl.exe` | paridad |
| `CMAKE_CXX_COMPILER` | misma raíz y `clang-cl.exe` | no aplica | misma raíz y `clang-cl.exe` | paridad |
| `CMAKE_LINKER` | `<llvm-root>/bin/lld-link.exe` | no aplica | `<compiler-llvm-root>/bin/lld-link.exe` | paridad |
| `CMAKE_MT` | `<llvm-root>/bin/llvm-mt.exe` | no aplica | `<compiler-llvm-root>/bin/llvm-mt.exe` | paridad |
| `CMAKE_RC_COMPILER` | `<llvm-root>/bin/llvm-rc.exe` | no aplica | `<compiler-llvm-root>/bin/llvm-rc.exe` | paridad |
| `CMAKE_EXE_LINKER_FLAGS` | `-fuse-ld=lld` | no aplica | `-fuse-ld=lld` | paridad |
| `CMAKE_DISABLE_FIND_PACKAGE_LLVM` | `FALSE` | no aplica | `FALSE` | paridad |
| `-U NSC_LLVM_MONOLITHIC_LIBRARY` | presente | no aplica | presente | paridad de caché |
| `LLVM_DIR` | `<llvm-root>/lib/cmake/llvm` | no aplica | `<compiler-llvm-root>/lib/cmake/llvm` | paridad restaurada |
| `NSC_BUILD_TESTS` | `OFF` al construir sólo NSC | no aplica | `OFF` | paridad |
| `NSC_WARNINGS_AS_ERRORS` | `ON` | no aplica | `ON` | paridad |
| `NSC_HOST_LINKAGE` | selección del plan | no aplica | selección de la GUI | paridad |
| `NSC_HOST_OPTIMIZATION` | selección del plan | no aplica | selección de la GUI | paridad |
| `NSC_NSLINK_EXECUTABLE` | ruta explícita seleccionada | no aplica | ruta explícita seleccionada | paridad |
| `NSC_BUILD_ID_OVERRIDE` | valor explícito; vacío en build ordinario | no aplica | vacío en build ordinario | paridad |

## Tabla literal de paridad — construcción

| Posición/argumento | Builder 1 autoritativo | Builder 2 | Builder 3 restaurado | Estado |
|---|---|---|---|---|
| ejecutable | `cmake --build` | no construye NSC | `cmake --build` | paridad efectiva |
| árbol | `compiler_build_dir` | no aplica | árbol del modo seleccionado | paridad Release; extensión Debug autorizada |
| `--config` | ausente con Ninja de configuración única | no aplica | ausente desde `d6e090ed2` | paridad |
| target | selección que contiene `nsc` | no aplica | `nsc` | paridad del botón dedicado |
| paralelismo | `--parallel <jobs>` | no aplica | `--parallel <jobs>` | paridad |
| argumentos nativos | `-- -k 0` | no aplica | `-- -k 0` | paridad |

## Procedencia y persistencia restauradas

Builder 3 conserva dos campos separados porque representan contratos distintos:

| Campo | Uso | Precedencia |
|---|---|---|
| `compiler_llvm_root` | configurar y construir NSC | valor explícito persistido > `NSC_LLVM_ROOT` > `C:\clang-llvm` |
| `llvm_dir` | superficies que ya consumían un directorio de paquete | sin cambios |

Una configuración histórica de Builder 3 no contiene `compiler_llvm_root`. Al cargarla, el dataclass
usa el valor vacío y `with_discovery` lo completa mediante `NSC_LLVM_ROOT` o la misma raíz canónica
de Builder 1. El antiguo `llvm_dir` permanece legible, pero ya no participa en la configuración ni
en la construcción de NSC. Cuando el usuario guarda la configuración, la nueva raíz queda explícita
y separada.

La configuración de NSC vuelve a escribir `LLVM_DIR` y elimina de la caché
`NSC_LLVM_MONOLITHIC_LIBRARY` en cada invocación, igual que Builder 1. No se reutiliza el
`LLVM_DIR` persistido por la superficie anterior.

## Cambios exactos

1. `nscbuilder3.py` añade el campo explícito `compiler_llvm_root` y su selector visible.
2. El root se resuelve desde override persistido, `NSC_LLVM_ROOT` o `C:\clang-llvm`.
3. Clang, LLD, MT, RC, `llvm-readobj`, `LLVMCore.lib`, `LLVMSupport.lib` y
   `LLVMConfig.cmake` se validan bajo esa misma raíz.
4. `compiler_configure_commands`, `compiler_build_commands` y el recibo de la operación usan la
   raíz de NSC, no `llvm_dir`.
5. `CMakeLists.txt` recupera exactamente la compuerta LLVM 23.x anterior a `6c4c5a516`.

## Resultado

`PARIDAD ESTÁTICA COMPLETA; VALIDACIÓN DE CONFIGURACIÓN Y BUILD PENDIENTE DEL USUARIO`.

No se configuró, compiló ni ejecutó ningún artefacto. La afirmación se limita al inventario literal
de argumentos, procedencias, persistencia, caché y hunks Git.
