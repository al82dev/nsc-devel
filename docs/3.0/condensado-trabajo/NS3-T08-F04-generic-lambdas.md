# NS3-T08-F04 — Generic lambdas

## Estado

TERMINADO. El contrato ejecutable cubre constraints, packs, recursión y un único environment
léxico compartido por todas las instancias capturadas.

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
- La recursión usa un self-binding directo de la instancia cerrada y converge por punto fijo para
  inferir el retorno.
- Una lambda con captures construye un solo carrier al declararse. Las firmas adicionales reciben
  el receiver de ese carrier y llaman directamente su función monomorfizada; no reconstruyen ni
  copian el environment.
- La identidad de cada instancia incluye plantilla, argumentos type/const/pack y ordinal estable,
  por lo que los catálogos y la cache distinguen cada monomorfización.
- El callable concreto conserva el lowering ordinario HIR/MIR/LLVM; no existe thunk con borrado
  de tipos ni dispatch erased.

## Archivos

- `src/Parser/Parser.cpp`: reconocimiento y gramática.
- `include/nativesharp/Frontend/ProjectGenericModel.h`: plantilla lambda léxica.
- `src/Frontend/ProjectGenericModel.cpp`: inferencia, sustitución y materialización.
- `tests/unit/NativeSharp3T08GenericLambdaTests.cpp`: ejecución O0/NSLink y diagnóstico.
- `CMakeLists.txt`: blanco focalizado `nsc_ns3_t08_generic_lambda_tests`.

## Evidencia puntual

`nsc_ns3_t08_generic_lambda_tests.exe`: 6/6 checks O0/NSLink.

- `identity(10)` infiere `T=int`.
- `identity<long>(20)` materializa otra instancia nativa.
- `(auto value) => ...` infiere su parámetro.
- `<const int N>` sustituye el valor en el cuerpo.
- `<pack const int Ns>` expande y pliega sus argumentos explícitos.
- `where T : struct, copy` acepta `int`; `where T : class` lo rechaza en la instancia.
- una instancia cerrada recursiva obtiene su retorno por punto fijo y usa self-binding directo.
- instancias capturadas `int` y `long` comparten el snapshot `copy` creado una vez, aun después de
  cambiar el local de origen.
- un parámetro no inferible produce un diagnóstico que nombra a `T`.

No se ejecutó baseline, minibaseline ni suite general.

## Cierre

No quedan subcontratos diferidos dentro de F04. Las referencias a métodos y captura de `this`
pertenecen a F05; los atributos de tail call pertenecen a F06.
