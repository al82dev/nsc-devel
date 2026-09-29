# Tanda 8 - HIR de funciones libres

## Objetivo

Convertir cada funcion declarada directamente en un namespace en una funcion HIR autentica y bajar sus llamadas sin inventar una clase, un receptor this ni un miembro sintetico.

## Cambios realizados

- ProjectOopHirModel recibe el modelo consolidado de funciones de namespace.
- Cada funcion libre obtiene un ProjectOopHirFunction con:
  - ordinal propio de funcion de namespace;
  - nombre fisico congelado por el modelo semantico;
  - firma, ownership, parametros, locales y cuerpo;
  - forma estatica sin tipo propietario real.
- Las llamadas resueltas se convierten en StaticCall y conservan:
  - el ordinal exacto de la funcion seleccionada;
  - conversiones implicitas;
  - orden fuente y orden ABI de argumentos nombrados;
  - argumentos opcionales;
  - ownership temporal y retornos por referencia.
- El lowering de return acepta tanto miembros de tipo como funciones libres.
- Las declaraciones extern C sin cuerpo quedan representadas como importaciones HIR.
- ProjectCompilation entrega el modelo de funciones libres a la fase HIR.

## Archivos modificados

- include/nativesharp/Frontend/ProjectOopHirModel.h
- src/Frontend/ProjectOopHirModel.cpp
- src/Frontend/ProjectCompilation.cpp
- CMakeLists.txt

## Archivos nuevos

- tests/unit/NamespaceFunctionHirTests.cpp
  - Se creo porque la frontera HIR necesita una prueba pequena e independiente del baseline.
  - Verifica identidad fisica, cuerpos, ordinales, llamada directa, ausencia de receptor falso y argumentos.

## Mejora obtenida

Antes de esta tanda las funciones libres terminaban en el analisis de cuerpos. Ahora ya existen como unidades ejecutables en la representacion de alto nivel y sus llamadas tienen un destino directo e inequivoco. Esto prepara MIR y LLVM sin contaminar el modelo orientado a objetos.

## Validacion enfocada

- Construccion exclusiva de nsc: correcta.
- Construccion exclusiva de nsc_namespace_function_hir_tests: correcta.
- Ejecucion exclusiva de nsc_namespace_function_hir_tests: PASS: 12 checks.
- No se ejecuto baseline, CTest completo, matrices ni fixtures generales.