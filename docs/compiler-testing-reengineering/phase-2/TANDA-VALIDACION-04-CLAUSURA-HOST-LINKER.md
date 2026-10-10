# Tanda de validación 04 — Clausura real de Host.Linker

## SCOPE LOCK local

- Defecto: `WindowsLibraryBuilder.cpp` dejó `24` símbolos NativeSharp sin
  productor al enlazar `nsc_component_tests.exe`.
- Entrada exacta: casos ejecutables de `CF-LINKER-034` que ejercitan la API
  pública real de `WindowsLibraryBuilder`.
- Archivos funcionales autorizados: `tests/compiler-suite/CMakeLists.txt`.
- Producto, pruebas, oráculos, manifiestos y disposiciones: protegidos.
- Prohibido: reemplazar el producto con stubs, eliminar casos o agregar toda la
  clausura del compilador por tanteo.

## Auditoría de símbolos

El `rsp` real del enlace contiene `522` objetos. La diferencia completa entre
referencias y definiciones arrojó `26` símbolos NativeSharp ausentes; dos
pertenecían a identidad y los `24` restantes tenían como consumidor inicial
único `WindowsLibraryBuilder.cpp`.

Los productores mínimos y su clausura NativeSharp son once unidades:

- LLVM: `CompilationArtifactCache.cpp`, `LlvmModule.cpp`,
  `LlvmOptimizer.cpp`, `LlvmProcessTrace.cpp`, `LlvmTargetMachine.cpp` y
  `LlvmVersion.cpp`;
- runtime: `WindowsRuntimeBuilder.cpp`;
- linker: `LldLinker.cpp`, `CoffArchiveVerifier.cpp`,
  `PeExecutableVerifier.cpp` y `WindowsRuntimeImports.cpp`.

Las demás dependencias NativeSharp de esas unidades ya estaban presentes en el
target: SHA-256, políticas LLVM, observación, `TargetInfo` y contrato ABI del
runtime. No se incorporaron los ocho productores canónicos que no participan
en esta reproducción.

## Contrato LLVM

La suite resuelve `LLVM-C.lib` y `LLVM-C.dll` desde el `LLVM_DIR` ya seleccionado
por Builder 3. La DLL se copia junto al ejecutable de pruebas, dentro del árbol
de construcción en `A:`, para que el loader no dependa del `PATH`. No se usa
Visual Studio, no se despliega y no se escribe en `C:`.

## Estado

- Símbolos de esta causa inventariados: `24/24`.
- Productores NativeSharp incorporados: `11/11`.
- Dependencias NativeSharp transitivas sin productor conocido: `0`.
- Auditoría predictiva del enlace real más los productores nuevos:
  `181,968` definiciones, `2,334` referencias externas, `0` símbolos
  NativeSharp sin productor y `0` símbolos LLVM-C sin productor.
- Configuración, compilación, enlace y ejecución: pendientes de corrida externa.
