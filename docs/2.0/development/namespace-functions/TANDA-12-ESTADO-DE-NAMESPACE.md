# Tanda 12: estado de namespace

## Objetivo

Incorporar constantes y variables de namespace al modelo multiproyecto sin representarlas como campos de una clase sintética.

## Cambios

- ProjectNameLookup registra variables globales declaradas dentro de namespaces nombrados.
- El registro conserva proyecto, archivo, namespace, accesibilidad, nombre y atributo const.
- Las declaraciones se ordenan de forma determinista, independiente del paralelismo y del orden de archivos.
- ProjectNamespaceFunctionModel expone símbolos de estado con identidad lógica y nombre físico estable.
- Las constantes enteras usan el evaluador integral tipado existente.
- Las variables sin inicializador quedan clasificadas para inicialización cero.
- Se aceptan inicializadores literales compatibles para enteros, flotantes, booleanos, texto, null y default.
- La inicialización dinámica se rechaza explícitamente con GlobalRequiresConstant o ConstRequiresConstant.
- Las declaraciones duplicadas producen DuplicateSymbol y PreviousDeclaration.

## Archivos modificados

- include/nativesharp/Frontend/ProjectNameLookup.h
- src/Frontend/ProjectNameLookup.cpp
- include/nativesharp/Frontend/ProjectNamespaceFunctionModel.h
- src/Frontend/ProjectNamespaceFunctionModel.cpp
- CMakeLists.txt

## Archivo nuevo

- tests/unit/NamespaceStateModelTests.cpp: prueba aislada del modelo, plegado, cero, identidad, duplicados y rechazo dinámico.

## Mejora obtenida

El estado de namespace deja de ser sintaxis procedural ignorada por la compilación de proyectos. Ahora posee una identidad estable y validación determinista, sin introducir un tipo contenedor falso ni un constructor global implícito.

## Verificación

- Construcción incremental exclusiva del objetivo nsc: correcta.
- Construcción exclusiva de nsc_namespace_state_model_tests: correcta.
- Ejecución exclusiva de nsc_namespace_state_model_tests: PASS, 11 verificaciones.
- No se ejecutó baseline, CTest general, fixtures generales ni matriz de optimizaciones.