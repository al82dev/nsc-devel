# NS3-T08-F04 — Generic lambdas

## Estado

EN CURSO. Este documento acompaña al primer corte ejecutable del ítem; el tracking no debe
marcar F04 como terminado hasta cerrar constraints, packs, recursión y captures.

## Cambio implementado en este corte

- El parser reconoce lambdas con cabeza genérica explícita: `<T>(T value) => value`.
- La misma cabeza acepta parámetros `const` y packs mediante la gramática genérica compartida.
- `auto` sólo dentro de parámetros de una lambda crea un parámetro de tipo inferido independiente.
- Las cláusulas `where` se parsean y validan con el mismo modelo de constraints que métodos y
  tipos genéricos; una instancia inválida se corta antes de analizar o bajar su cuerpo.
- El modelo genérico registra plantillas lambda por alcance léxico.
- Una llamada sin argumentos de tipo infiere contra los tipos reales de sus argumentos.
- Una llamada con `<...>` sustituye argumentos type/const explícitos y expande un pack final.
- Cada llamada se clona como lambda cerrada antes del name lookup y del análisis de cuerpos.
- El callable concreto conserva el lowering ordinario HIR/MIR/LLVM; no existe thunk con borrado
  de tipos ni dispatch erased.

## Archivos

- `src/Parser/Parser.cpp`: reconocimiento y gramática.
- `include/nativesharp/Frontend/ProjectGenericModel.h`: plantilla lambda léxica.
- `src/Frontend/ProjectGenericModel.cpp`: inferencia, sustitución y materialización.
- `tests/unit/NativeSharp3T08GenericLambdaTests.cpp`: ejecución O0/NSLink y diagnóstico.
- `CMakeLists.txt`: blanco focalizado `nsc_ns3_t08_generic_lambda_tests`.

## Evidencia puntual

`nsc_ns3_t08_generic_lambda_tests.exe`: 5/5 checks.

- `identity(10)` infiere `T=int`.
- `identity<long>(20)` materializa otra instancia nativa.
- `(auto value) => ...` infiere su parámetro.
- `<const int N>` sustituye el valor en el cuerpo.
- `<pack const int Ns>` expande y pliega sus argumentos explícitos.
- `where T : struct, copy` acepta `int`; `where T : class` lo rechaza en la instancia.
- una instancia cerrada recursiva obtiene su retorno por punto fijo y usa self-binding directo.
- un parámetro no inferible produce un diagnóstico que nombra a `T`.

No se ejecutó baseline, minibaseline ni suite general.

## Pendiente antes del cierre

- Capturas cuyo environment se crea una sola vez al declarar la lambda y es compartido por todas
  sus especializaciones.
- Dumps/identidad de cache que publiquen las instancias genéricas.
