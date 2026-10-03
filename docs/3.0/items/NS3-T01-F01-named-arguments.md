# NS3-T01-F01 — Argumentos nombrados

## Resultado

El recorrido productivo existente quedó auditado de parser a LLVM: el AST conserva el nombre, el
resolver forma candidatos y construye el mapa argumento→parámetro, HIR/MIR conservan ese mapa y el
backend evalúa primero en orden fuente antes de reordenar los valores para el ABI. Constructores,
métodos, funciones de namespace, overloads, defaults y genéricos usan esta misma semántica.

El cierre agrega una prueba vertical exclusiva de F01. Produce una biblioteca `O0`, lee su NSMETA,
compila un consumidor precompilado con NSLink y ejecuta el resultado. También comprueba los
rechazos de nombres duplicados y desconocidos sin ejecutar el resto de CC1.

## Cambios

- nueva prueba `nsc_ns3_t01_named_argument_tests`;
- comprobación explícita de nombres y ordinales públicos en NSMETA;
- comparación ejecutable entre llamada posicional y llamada nombrada;
- orden observable `3,1,2` para una llamada cuyo ABI recibe `1,2,3`;
- mezcla posicional/nombrada, defaults, overloads y método genérico;
- rechazos cross-module para nombre duplicado o renombrado;
- guía bilingüe de sintaxis y evolución de APIs públicas.

## Archivos

### Nuevos

- `tests/unit/NativeSharp3T01NamedArgumentTests.cpp`: prueba vertical aislada para no depender de
  una corrida completa ni repetir CC1.
- `docs/3.0/language/named-arguments.md`: contrato ES/EN para usuarios y autores de bibliotecas.
- `docs/3.0/items/NS3-T01-F01-named-arguments.md`: expediente de implementación del ítem.

### Modificados

- `CMakeLists.txt`: registra únicamente el nuevo ejecutable y su prueba dirigida.

No se modificó código productivo porque la auditoría confirmó que el pipeline real ya implementa
el contrato; el hueco era la evidencia aislada de overloads, genéricos, NSMETA y rechazos exigida
por el roadmap. Reescribir el binder habría duplicado semántica funcional y aumentado el riesgo.

## Mejora

El feature deja de depender de una fixture compartida que también cubre `params` e inicializadores.
Ahora una regresión de nombres públicos, orden de efectos, defaults, selección de overload o
reordenamiento ABI falla en una prueba F01 identificable y corta.

## Pruebas dirigidas

- `nsc_ns3_t01_named_argument_tests`: `PASS: 6 checks`.
- `nsc_ns3_t01_expression_tests`: `PASS: 27 checks`.
- optimización NativeSharp: sólo `O0`.
- linker de programas `.ns`: `A:\nsc-devel\build-test-nslink\bin\nslink.exe`.
- baseline y CTest completo: no ejecutados.
