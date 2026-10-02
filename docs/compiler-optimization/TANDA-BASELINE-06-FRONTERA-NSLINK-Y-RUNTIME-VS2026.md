# Tanda baseline 06 — Frontera NSLink y runtime VS2026 x64 fijado en A:

## Objetivo

Separar sin ambigüedad las dos mitades del toolchain:

- el host nativo `nsc.exe` se construye con `clang-cl`, LLVM estático y `lld-link`;
- todo producto generado desde fuentes `.ns` durante las pruebas se enlaza con NSLink;
- LLD queda disponible sólo como modo de desarrollo explícito, no como repetición automática ni
  como criterio ordinario de certificación;
- ninguna prueba descubre, carga o ejecuta Visual Studio;
- el único uso de Visual Studio queda dentro del enlace del host `nsc.exe`, mediante una copia
  local y fijada del runtime estático VS2026 x64 en `A:\vc2026-runtime`.

## Resultado funcional

El baseline normal de NSCBuilder pasa a ser una matriz NSLink O0–O3. La opción profunda todavía
puede pedir ambos linkers de forma explícita, pero el modo predeterminado no repite con LLD los
fallos de NSLink y nunca los oculta.

Las pruebas self-hosted SH03–SH48 dejaron de producir o esperar imágenes `lld-*`. Sus contratos,
verificadores y consumidores posteriores comparten una sola nomenclatura `*-nslink.exe`.

La construcción nativa de `nsc.exe` sigue usando `lld-link`, porque ésa es la frontera del host y
no un producto NativeSharp. Las pruebas C++ también pueden ser enlazadas por el toolchain CMake;
la restricción NSLink comienza cuando `nsc.exe` compila o emite un programa `.ns`.

## Runtime de Visual C++ fijado

Se creó fuera del repositorio el caché local `A:\vc2026-runtime`, exclusivamente para x64:

- `A:\vc2026-runtime\lib\x64\release`
  - `libcmt.lib`, `libcpmt.lib`, `libvcruntime.lib`;
  - `oldnames.lib`, `legacy_stdio_definitions.lib`;
  - `iso_stdio_wide_specifiers.lib`, `delayimp.lib`, `aligned_new.lib`.
- `A:\vc2026-runtime\lib\x64\debug`
  - `libcmtd.lib`, `libcpmtd.lib`, `libvcruntimed.lib`;
  - `oldnames.lib`, `legacy_stdio_definitions.lib`;
  - `iso_stdio_wide_specifiers.lib`, `delayimp.lib`, `aligned_new.lib`.

No se copiaron árboles x86, ARM, ARM64, Spectre ni rutas completas de Visual Studio. El
manifiesto `A:\vc2026-runtime\manifest.sha256` fija las 16 bibliotecas y
`A:\vc2026-runtime\README.md` registra procedencia, toolset y política de uso.

`nsc.exe` usa siempre el conjunto Release `/MT`, incluso en una configuración de desarrollo,
porque las bibliotecas estáticas calificadas de LLVM se construyeron con ese mismo CRT. El juego
Debug se conserva para herramientas x64 que puedan construirse completamente con `/MTd`; no se
mezcla con LLVM Release.

## Cambios de CMake y empaquetado

### `CMakeLists.txt`

- fija `NSC_VS2026_RUNTIME_ROOT` en `A:/vc2026-runtime`;
- valida de forma fail-closed los ocho archivos Release y los ocho Debug;
- añade únicamente `lib/x64/release` al enlace de `nsc.exe`;
- limita el descubrimiento de DIA a Visual Studio 2026 (`18.x`) y exclusivamente a la
  construcción del compilador;
- elimina la prueba consumidora de DIA/Visual Studio;
- deja de registrar un `lld-link` especial para T36, T37, ABI de assembly y ABI vararg;
- entrega NSLink a esas cuatro pruebas;
- instala el nuevo cierre de imports `sdk/win64/nativesharp-runtime.nsimports`.

### Verificadores CMake

Los siguientes verificadores dejaron de enlazar imágenes NativeSharp mediante LLD o de consumir
artefactos self-hosted con sufijo LLD:

- `VerifyNativeSharpArraysMemory.cmake`, `VerifyNativeSharpBackendGeneral.cmake`,
  `VerifyNativeSharpBootstrapSurface.cmake`, `VerifyNativeSharpCallableGraph.cmake`,
  `VerifyNativeSharpDataLayout.cmake`, `VerifyNativeSharpDriverProducts.cmake`,
  `VerifyNativeSharpEhRegions.cmake`, `VerifyNativeSharpGenericsAot.cmake`,
  `VerifyNativeSharpInteropBase.cmake`, `VerifyNativeSharpNativeText.cmake`,
  `VerifyNativeSharpObjectModel.cmake` y `VerifyNativeSharpOwnershipCleanup.cmake`;
- `VerifyNativeSharpFirstGeneration.cmake`, `VerifyNativeSharpFixedPoint.cmake`,
  `VerifyNativeSharpFunctionalEquivalence.cmake`, `VerifyNativeSharpProjectDriver.cmake`,
  `VerifyNativeSharpSelfHostedGeneration.cmake`,
  `VerifyNativeSharpRepeatedFixedPoint.cmake` y
  `VerifyNativeSharpCrossMachineDeterminism.cmake`;
- `VerifyNativeSharpLowLevelParity.cmake`, `VerifyNativeSharpInteropParity.cmake`,
  `VerifyNativeSharpSpecializedCode.cmake` y `VerifyNativeSharpRuntimeLibrary.cmake`;
- `VerifyNativeSharpSourceDerivedBackend.cmake`, `VerifyNativeSharpSourceFunctions.cmake`,
  `VerifyNativeSharpSourceControlFlow.cmake`, `VerifyNativeSharpSourceLoopControl.cmake`,
  `VerifyNativeSharpSourceBoolVoid.cmake`, `VerifyNativeSharpSourceExpressions.cmake`,
  `VerifyNativeSharpSourceIntegerWidths.cmake`, `VerifyNativeSharpSourcePointers.cmake` y
  `VerifyNativeSharpSourceFloatingScalars.cmake`;
- `VerifyNativeSharpLlvmBackend.cmake`, `VerifyNativeSharpLlvmCBridge.cmake` y
  `VerifyNativeSharpT44MetadataClosure.cmake`.

Los verificadores finales `AdvancedToolchain`, `Performance`, `DeveloperExperience`,
`RobustnessFinal`, `DistributionFinal` y `FinalCertification` fueron alineados con el nuevo conteo
y la nueva cadena de evidencias NSLink.

T36, T37, `AssemblyAbiO0` y `Win64VarargAbi` reciben `NSLINK_EXECUTABLE` directamente. T37 ya no
busca bibliotecas en instalaciones de Windows SDK o Visual Studio: usa el archivo explícito de
imports Win64 y el runtime generado por el árbol actual.

## Cambios self-hosted

### Verificadores Python

Se actualizaron los verificadores de first generation, fixed point, functional equivalence,
project driver, generación nsc1→nsc2→nsc3, fixed point repetido, determinismo, low-level,
interop, specialized code y runtime. También se alinearon los verificadores SH16–SH39 que aún
buscaban resultados `lld-*` después de que sus productores ya generaban sólo NSLink.

Se retiraron ramas muertas que todavía sabían construir comandos LLD dentro de:

- `verify_self_hosted_generation.py`;
- `verify_functional_equivalence.py`;
- `verify_cross_machine_determinism.py`.

`verify_llvm_c_contract.py` pasó a contrato versión 2: prohíbe imports dinámicos de `LLVM-C.dll`,
conserva el inventario auditado de 219 símbolos y valida las 127 declaraciones NativeSharp contra
esa superficie congelada. Ya no confunde “LLVM enlazado estáticamente” con “faltan todos los
imports LLVM-C”.

### Contratos JSON

Todos los contratos ejecutables de self-host declaran una sola ruta o linker: NSLink. Esto cubre
first generation, fixed point, source-derived backend, source features, backend general, layout,
object model, texto, memoria, interop, genéricos, SH41–SH48 y LLVM backend.

`llvm-c-api.contract.json` sube a versión 2 y declara:

- `linkage = static-llvm-components`;
- `forbidden_dynamic_library = LLVM-C.dll`;
- `audited_binding_symbols` como inventario histórico congelado.

### Backend NativeSharp

`selfhost/nsc/CodeGen/LlvmBackend.ns` implementa el lowering del operador unario `~` mediante
`LLVMBuildNot`. Esto corrige el fallo observado en
`NativeSharp.Compiler.Source.ComptimeExpression.ParseUnary` durante SH41.

## NSCBuilder

`nscbuilder.py` cambia la política ordinaria:

- NSLink O0–O3, sin diagnóstico LLD automático, es una matriz certificable completa;
- “Ambos” continúa siendo una validación profunda solicitada explícitamente;
- “LLD solamente” continúa siendo un modo diagnóstico, no una certificación normal;
- la GUI y la ayuda CLI describen esta política sin prometer fallback automático.

## Stage 46 y pruebas de depuración

`Stage46_1DllCrossLinkerMatrixTests.cpp` conserva la matriz productor/consumidor y O0/O2, pero
ambos lados usan NSLink. El fallback predeterminado apunta a
`A:\nslink-devel\build-debug\bin\nslink.exe`, nunca a `C:\native-sharp`.

Se eliminó `tests/integration/debugger_consumers/DebuggerConsumerHandoff.cpp` y su registro CMake.
Era la única prueba que descubría DIA y cargaba `msdia140.dll` desde Visual Studio. La capacidad
CodeView/PDB continúa validándose con herramientas LLVM y las pruebas T36/T37.

## Archivos nuevos

- `sdk/win64/nativesharp-runtime.nsimports`: cierre explícito de imports Windows x64 para T37 y
  el SDK instalado. Evita buscar `.lib` de Visual Studio o Windows SDK durante una prueba `.ns`.
- este documento: evidencia de alcance, decisiones, archivos, pruebas y limitaciones de la tanda.
- fuera de Git: `A:\vc2026-runtime\README.md`, `manifest.sha256` y las 16 bibliotecas x64.

## Archivo eliminado

- `tests/integration/debugger_consumers/DebuggerConsumerHandoff.cpp`: retirado porque obligaba a
  instalar Visual Studio/DIA para ejecutar una prueba. No formaba parte del compilador entregable.

## Mejora obtenida

- El baseline normal deja de duplicar el costo de enlace con LLD.
- Un éxito LLD ya no puede esconder un fallo NSLink.
- Los artefactos nsc2/nsc3 usados por pruebas posteriores son exactamente los que recibiría un
  usuario final.
- Las pruebas NativeSharp ya no dependen de Visual Studio ni de `C:\native-sharp`.
- El host `nsc.exe` no depende de `LLVM-C.dll`, `MSVCP*.dll`, `VCRUNTIME*.dll` ni UCRT dinámica.
- La procedencia del runtime nativo queda fijada, verificable por hash y limitada a x64.

## Validación específica

- Construcción incremental exclusiva de `nsc`: correcta.
- Contrato posenlace de host estático: correcto; sin LLVM-C ni runtime MSVC dinámico.
- Configuración CMake de `build-test-nslink`: correcta y reconoce
  `A:/vc2026-runtime/lib/x64/release`.
- Sintaxis Python: 33 archivos modificados correctos.
- Contratos JSON: 47 archivos analizados correctamente.
- Manifiesto del runtime: 16/16 hashes correctos; sólo rutas x64 Release/Debug.
- Unitarias dirigidas de NSCBuilder, contratos SH41/SH45/SH46/SH47 y runtime VS2026: correctas.
- Stage 46 DLL NSLink, T36, T37, ABI assembly y ABI vararg: 5/5 correctas en 6.84 s.
- SH08 LLVM backend: correcto con NSLink.
- SH03 LLVM estático: el contrato LLVM-C versión 2 pasa; el cierre completo no pudo continuar
  porque `A:\native-sharp-3.0-baseline` aún contiene el contrato runtime anterior. Se resolverá al
  reconstruir el staging del baseline; no se copió desde C: ni se recompilaron librerías.
- SH41 completo no se ejecutó de forma aislada porque requiere el paquete precompilado de
  `NativeSharp.Core.IO` que prepara el baseline.
- Baseline completo: no ejecutado.
- Instalación o modificación de `C:\native-sharp`: ninguna.
- `_abi/gates/G00/gate.json`: cambio preexistente del usuario, preservado y excluido del commit.

## Residuos

Los directorios `__pycache__` creados por comprobaciones locales fueron eliminados. No se dejaron
temporales de pruebas en el árbol fuente ni se copiaron archivos desde C: hacia A:.
