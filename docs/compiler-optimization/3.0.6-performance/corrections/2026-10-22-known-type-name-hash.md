# Corrección del hash transparente de nombres de tipo conocidos

## SCOPE LOCK

- Objetivo: corregir la ambigüedad de `KnownTypeNameHash` al consultar `KnownTypeNameSet` con literales.
- Naturaleza: corrección no funcional de una regresión introducida por la optimización T3.3.
- Revisión fallida: `6c636fcf88b49cb43198d1304fd81df1b1a06d89`.
- Productor responsable: `KnownTypeNameHash`.
- Archivos de producto autorizados: `KnownTypeNameSet.h` y `ParserContext.cpp`.
- Protegidos: pruebas, fixtures, CMake, NSC Builder, sintaxis, semántica, diagnósticos, CLI, ABI, NSMETA,
  HIR, MIR, LLVM IR y artefactos emitidos.
- Validación autorizada: limpieza, compilación y ejecución de la batería unitaria completa.
- Prohibido: pruebas externas, minibaseline, baseline, instalación y deploy.
- Terminación: la batería unitaria completa pasa sin modificar sus fuentes ni expectativas.

## Evidencia

La compilación de `nsc_component_tests` se detuvo en 99 de 784 objetos. MSVC 14.51 diagnosticó como ambigua
la llamada a `KnownTypeNameHash` para `char[1]`, `char[2]` y `char[9]`. Las instanciaciones proceden de los
literales usados por `ParserLookaheadModule.cpp` y `ParserCursorRecoveryModule.cpp`.

El commit `ac66ebbc8` introdujo dos overloads transparentes: uno para `std::string_view` y otro para
`const std::string&`. Un literal puede convertirse a ambos tipos mediante una conversión definida por el usuario,
por lo que ninguno resulta mejor durante la resolución.

## Corrección

Se conserva cada overload existente y se añade el overload estrecho `const char*`. Para un literal, la conversión
de arreglo a puntero es una conversión estándar y tiene precedencia. El cuerpo delega en el overload de
`std::string_view`, por lo que no crea un `std::string` ni introduce un algoritmo de hash diferente.

## Contratos conservados

- No cambian las entradas aceptadas por el parser ni su semántica.
- No cambian los elementos almacenados, la igualdad ni el valor de hash del contenido textual.
- No cambian diagnósticos, formatos, metadata, ABI del lenguaje ni artefactos emitidos.
- No se modifica ninguna prueba ni consumidor.

## Validación

Estado actual: `CORRECCIÓN COMPILADA, EJECUCIÓN UNITARIA COMPLETA PENDIENTE`.

Se limpió `build-compiler-suite-v3/debug`: Ninja eliminó 107 archivos y terminó con código 0. Después se inició
desde limpio la construcción del target `nsc_component_tests`. Los dos consumidores que reproducían el defecto
compilaron: `ParserCursorRecoveryModule.cpp` en el paso 98 y `ParserLookaheadModule.cpp` en el paso 99. La
ambigüedad de `KnownTypeNameHash` no volvió a aparecer.

La construcción completa todavía no terminó porque después aparecieron dos causas independientes: inicializadores
históricos de `LibraryReference` incompatibles con un campo agregado nuevo y el cambio de firma de
`DriverExecutor`. Por ello las pruebas aún no se ejecutaron y T3.3 permanece pendiente de la batería unitaria
completa. Esas causas no pertenecen a esta hipótesis ni a este commit.
