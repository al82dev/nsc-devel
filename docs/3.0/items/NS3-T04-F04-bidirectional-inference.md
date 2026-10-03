# NS3-T04-F04 — Bidirectional inference

## Estado

`TERMINADA`. La inferencia genérica productiva combina evidencia local y contexto exacto, propaga
tipos hacia expresiones anidadas y conserva un algoritmo acotado sin búsqueda de conversiones.

## Qué se hizo

- `inferExpressionType` construye tipos tuple estructurales y la clonación conserva su marca,
  nombres de elementos, calificadores y origen.
- El parser vuelve a distinguir `Box<(int,int)>` de un argumento const parentizado mediante el
  escáner de tipos existente; expresiones como `(2 + 2)` conservan su ruta const.
- Las llamadas internas se intentan primero sin diagnóstico. Después, una plantilla única puede
  enviar un parámetro ya cerrado hacia la expresión interna y reanudar la unificación.
- El tipo esperado se propaga por inicialización, retorno y asignación, y dentro de condicionales,
  tuples y `move`.
- Una lambda con parámetros explícitos unifica esos tipos contra la firma de un delegate genérico;
  su cuerpo de expresión también aporta evidencia cuando tiene tipo local determinable.
- Lambdas sin tipos y method groups sólo se contextualizan cuando el resto de la firma ya produce
  un destino único; la ambigüedad no se resuelve por orden de declaración.
- Un overload no genérico con retorno exacto incompatible se considera irrelevante. Los candidatos
  potencialmente aplicables permanecen a cargo de la resolución ordinaria.
- La recursión contextual comparte el límite 64 y los fallos aconsejan el escape explícito
  `<...>`.

## Archivos modificados

- `include/nativesharp/Frontend/ProjectGenericModel.h` y
  `src/Frontend/ProjectGenericModel.cpp`: propagación contextual, evidencia lambda/tuple, orden y
  límite del solver.
- `src/Parser/Parser.cpp`: desambiguación de tuple types dentro de listas genéricas.
- `CMakeLists.txt`: target dirigido exclusivo de F04.
- Documentos consolidados de T04: retiro de deuda F04 ya cerrada.

## Archivos nuevos y por qué

- `tests/unit/NativeSharp3T04BidirectionalInferenceTests.cpp`: seis cierres dirigidos con PE O0 y
  NSLink local, estructuras contextuales, orden de overloads y rechazos causales.
- `docs/3.0/language/bidirectional-inference.md`: contrato bilingüe y guía de desambiguación.
- Este documento: evidencia atómica del commit F04.

## Mejora obtenida

El usuario puede omitir argumentos genéricos cuando el programa ya contiene una solución exacta,
incluso a través de una llamada anidada, tuple, lambda o tipo nominal/const cerrado. El resultado
no depende de conversiones especulativas ni del orden de candidatos y siempre conserva una salida
explícita.

## Validación dirigida

- `nsc_ns3_t04_bidirectional_inference_tests`: 6/6; programas productivos O0/NSLink terminan en 42.
- `nsc_generic_inference_tests`: regresión del solver local histórico.
- Regresiones dirigidas T04 F01–F03, parser y fixture `c60_ns3_generic_solver`.
- Regresiones específicas T02 de tuple nativa (24/24), literal (4/4) y deconstruction (6/6).

No se ejecutó baseline, CTest general, O1/O2/O3, instalación ni deploy.
