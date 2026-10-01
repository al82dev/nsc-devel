# Tanda correctiva G00 — ejecutable NSC autónomo

Fecha: 2026-10-01
Rama: `development`
Alcance: compilador C++ anfitrión `nsc.exe` para Windows x64
Estado: implementación terminada; pendiente del baseline integral de Alberto

## Objetivo

Eliminar las dependencias redistribuibles del ejecutable del compilador. El `nsc.exe` entregable
debe usar LLVM 23 mediante bibliotecas de componentes estáticas y debe incorporar estáticamente el
runtime C/C++ de Microsoft. El contrato prohíbe las importaciones `LLVM-C.dll`, `MSVCP*.dll`,
`VCRUNTIME*.dll`, `ucrtbase.dll`, `ucrtbased.dll` y `api-ms-win-crt-*.dll`.

Las DLL propias del sistema operativo —por ejemplo `KERNEL32.dll`, `ADVAPI32.dll`, `ntdll.dll`,
`ole32.dll` y `SHELL32.dll`— siguen siendo dependencias legítimas de Windows y no son runtimes
redistribuibles del compilador.

## Cambios realizados

### Toolchain y runtime

- CMake habilita la política `CMP0091` y fija `CMAKE_MSVC_RUNTIME_LIBRARY=MultiThreaded`.
- Debug mantiene `_ITERATOR_DEBUG_LEVEL=0`, compatible con los archivos estáticos oficiales de
  LLVM 23, que fueron construidos con el runtime release `/MT`.
- El proyecto habilita C además de C++, porque zlib y zstd se compilan desde sus fuentes C.
- `nscbuilder.py` fija `clang-cl.exe` como compilador tanto de C como de C++, y `lld-link.exe` como
  enlazador. No se usa `cl.exe`.
- La configuración de `nscbuilder.py` exige `LLVMConfig.cmake`, `LLVMCore.lib`, `LLVMSupport.lib` y
  `llvm-readobj.exe`; dejó de aceptar `LLVM-C.lib`.

### LLVM estático

- Windows exige el paquete CMake de LLVM 23 y mapea archivos estáticos por componente.
- La ruta monolítica `LLVM-C.dll`/`LLVM-C.lib` queda prohibida en Windows.
- Se enlazan los componentes X86 y AArch64 porque la API existente de `LlvmTargetMachine` publica
  ambos. El ejecutable anfitrión y los artefactos comprobados siguen siendo Windows x64.
- La ubicación del DIA SDK se descubre con `vswhere` para satisfacer el soporte PDB estático de
  LLVM sin introducir una DLL de LLVM.

### Dependencias estáticas fijadas

Los archivos estáticos oficiales de LLVM referencian símbolos de zlib y zstd, pero el paquete
Windows de LLVM no incluye sus archivos `.lib`. Se incorporaron únicamente fuentes de biblioteca y
avisos de licencia:

| Dependencia | Versión | SHA-256 del archivo oficial |
| --- | --- | --- |
| zlib | 1.3.2 | `bb329a0a2cd0274d05519d61c667c062e06990d72e125ee2dfa8de64f0119d16` |
| zstd | 1.5.7 | `eb33e51f49a15e023950cd7825ca74a4a2b43db8354825ac24fc1b7ee09e6fa3` |

Ambas bibliotecas se construyen dentro del mismo grafo con `clang-cl` y `/MT`. No se generan DLL,
programas, pruebas ni instalaciones independientes de terceros.

### Verificación fail-closed

Después de cada enlace de `nsc.exe`, CMake ejecuta `llvm-readobj --coff-imports`. La construcción
falla si aparece cualquiera de las DLL prohibidas. Esto convierte la autonomía del ejecutable en
un contrato del build y evita que una actualización de LLVM o CMake reintroduzca silenciosamente
el runtime dinámico.

## Archivos modificados

- `CMakeLists.txt`: política `/MT`, lenguaje C, descubrimiento DIA, componentes LLVM estáticos y
  verificación posbuild.
- `nscbuilder.py`: toolchain C/C++ uniforme y requisitos LLVM estáticos.
- `tests/selfhost/test_bootstrap_infrastructure.py`: regresión del comando CMake emitido por el
  builder.

## Archivos nuevos

- `cmake/NativeSharpLlvmStaticDependencies.cmake`: define zlib/zstd como targets estáticos del
  compilador.
- `cmake/VerifyNativeSharpStaticHostRuntime.cmake`: inspecciona y bloquea imports dinámicos.
- `third_party/README.md`: procedencia, versiones, hashes y licencias.
- `third_party/zlib/*`: fuentes de biblioteca zlib 1.3.2 y licencia.
- `third_party/zstd/lib/*`, `third_party/zstd/LICENSE`, `third_party/zstd/COPYING`: fuentes de
  biblioteca zstd 1.5.7 y licencias.

Los fuentes de terceros son nuevos porque el LLVM 23 instalado sólo proporciona sus componentes
estáticos, no los archivos estáticos de compresión que `LLVMSupport.lib` referencia. Vendorizarlos
con versiones y hashes fijos evita depender de vcpkg, MSYS2, una descarga durante el build o DLLs
externas en la máquina del usuario.

## Incidencias encontradas y resueltas

- La primera regeneración mostró que sólo C++ estaba fijado a `clang-cl`; CMake seleccionó
  `clang.exe` para C y rechazó mezclar frontends. Se corrigió el builder para fijar `clang-cl` en
  ambos lenguajes.
- El primer enlace estático expuso referencias reales a zlib/zstd y a inicializadores AArch64. Se
  añadieron las bibliotecas estáticas fijadas y el componente LLVM AArch64 en vez de ocultar
  símbolos sin resolver.
- Un fixture con `namespace` fue rechazado correctamente por el modo histórico de archivo único;
  la prueba de humo se sustituyó por el fixture procedural mínimo que usa `DriverTests`.

## Validación dirigida

- Configuración CMake Debug/O0 con `clang-cl` 23.1.0 para C/C++ y `lld-link` 23.1.0: correcta.
- Construcción exclusiva del target `nsc`: correcta.
- Verificador posbuild de imports prohibidos: correcto.
- Inspección independiente de imports: sólo DLLs de sistema Windows.
- Directivas COFF de objetos zlib, zstd y `nsc_driver`: `libcmt.lib`; el objeto C++ declara
  además `RuntimeLibrary=MT_StaticRelease` y `_ITERATOR_DEBUG_LEVEL=0`.
- `nsc.exe --version` y `nsc.exe --help`: correctos.
- Tamaño del ejecutable autónomo Debug: 73,651,712 bytes.
- `nsc_llvm_codegen_tests`: 25/25 comprobaciones.
- Prueba unitaria específica de `nscbuilder.py`: 1/1.
- Compilación de humo `--emit-obj -O0`: objeto COFF x86-64 correcto, 5 secciones.
- Temporales de descarga, extracción y compilación de humo: eliminados.
- Baseline integral: no ejecutado; queda reservado para Alberto.

## Mejora obtenida

El usuario final ya no necesita copiar `LLVM-C.dll` ni instalar el Microsoft Visual C++
Redistributable para iniciar el compilador. La contrapartida esperada es un `nsc.exe` mayor, porque
contiene el código LLVM y los runtimes que realmente utiliza; a cambio, el paquete es más simple,
determinista y resistente a mezclas de versiones de DLL.
