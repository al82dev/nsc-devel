# Tanda 10 - LLVM, cache, particionado y depuracion

## Objetivo

Emitir funciones libres y sus llamadas como simbolos LLVM reales, y hacer que los consumidores tardios entiendan que no poseen una clase declaradora.

## Cambios realizados

- LLVM omite la inicializacion de tipo estatico para funciones de namespace.
- Las declaraciones, definiciones y llamadas usan el mismo nombre fisico congelado.
- CodeView crea subprogramas para funciones libres usando su rango fuente y nombre Namespace.Funcion.
- HIR y MIR conservan un nombre de depuracion separado del simbolo fisico.
- La huella de cache incorpora ordinales de funciones libres, llamadas y nombre de depuracion.
- El particionador no agrega funciones libres al indice de miembros de clase; sus dependencias se resuelven por simbolo.
- Los dumpers HIR/MIR muestran el ordinal de funcion de namespace.
- El esquema MIR interno subio de 1.12 a 1.13.

## Archivos modificados

- include/nativesharp/Frontend/ProjectOopHirModel.h
- include/nativesharp/Frontend/ProjectOopMirModel.h
- src/Frontend/ProjectOopHirModel.cpp
- src/Frontend/ProjectOopMirModel.cpp
- src/Frontend/ProjectOopHirModelDumper.cpp
- src/Frontend/ProjectOopMirModelDumper.cpp
- src/LLVM/ProjectOopLlvmLowerer.cpp
- src/LLVM/ProjectOopCodePartition.cpp
- src/LLVM/ProjectOopMirCacheIdentity.cpp
- CMakeLists.txt

## Archivos nuevos

- tests/unit/NamespaceFunctionLlvmTests.cpp
  - Se creo para validar solo la frontera backend de esta feature.
  - Verifica LLVM, verifier, llamada directa, CodeView completo en O0, particionado y cache.

## Mejora obtenida

Una funcion libre ya puede atravesar el backend sin provocar acceso a un tipo propietario fuera de rango. La compilacion incremental distingue cambios en estas funciones, los modulos particionados conservan sus dependencias y el depurador recibe nombres de fuente legibles.

## Validacion enfocada

- Construccion exclusiva de nsc: correcta.
- Construccion exclusiva de nsc_namespace_function_llvm_tests: correcta.
- Ejecucion exclusiva de nsc_namespace_function_llvm_tests: PASS: 10 checks.
- No se ejecuto baseline, CTest completo, matrices ni fixtures generales.