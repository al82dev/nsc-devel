# Tanda 9 - MIR de funciones libres

## Objetivo

Transportar las funciones libres y sus llamadas desde HIR hasta MIR sin convertirlas en miembros de una clase.

## Cambios realizados

- ProjectOopMirFunction conserva el ordinal de funcion de namespace.
- ProjectOopMirNode conserva ese ordinal en las llamadas.
- ProjectOopMirModel mantiene un indice separado para consultar funciones libres.
- La conversion HIR a MIR copia firma, ABI Win64 x64, ownership, locales, cuerpo y nombre fisico.
- StaticCall acepta como destino un miembro estatico o una funcion libre y produce DirectStaticCall.
- El esquema MIR interno subio de 1.11 a 1.12 por el nuevo campo estructural.

## Archivos modificados

- include/nativesharp/Frontend/ProjectOopMirModel.h
- src/Frontend/ProjectOopMirModel.cpp
- CMakeLists.txt

## Archivos nuevos

- tests/unit/NamespaceFunctionMirTests.cpp
  - Se creo para probar exclusivamente la frontera HIR a MIR.
  - Verifica funciones, indices, cuerpos, nombre fisico, destino de llamada, argumentos y ausencia de miembro o receptor sintetico.

## Mejora obtenida

Las funciones libres ya llegan a la representacion que consume el backend. MIR no necesita consultar tipos propietarios inexistentes y cada llamada directa contiene la identidad exacta que LLVM debera declarar o definir.

## Validacion enfocada

- Construccion exclusiva de nsc: correcta.
- Construccion exclusiva de nsc_namespace_function_mir_tests: correcta.
- Ejecucion exclusiva de nsc_namespace_function_mir_tests: PASS: 12 checks.
- No se ejecuto baseline, CTest completo, matrices ni fixtures generales.