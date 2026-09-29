# Tanda 3: modelo multiparchivo de símbolos

## Objetivo

Registrar funciones libres de namespaces nombrados durante la predeclaración paralela del proyecto, antes del análisis de cuerpos.

## Cambios

- Se añadió NamespaceFunctionDeclaration con módulo, namespace, nombre, accesibilidad, origen y presencia de cuerpo.
- ProjectNameLookup conserva un índice determinista de funciones de todos los archivos.
- NamespaceSymbol conserva ordinales de funciones además de ordinales de tipos.
- Las funciones permanecen separadas de ClassMemberSymbol; no se crea tipo propietario ni clase sintética.
- La accesibilidad predeterminada se materializa como internal.

## Archivos modificados

- include/nativesharp/Frontend/NamespaceSymbol.h
- src/Frontend/NamespaceSymbol.cpp
- include/nativesharp/Frontend/ProjectNameLookup.h
- src/Frontend/ProjectNameLookup.cpp
- CMakeLists.txt

## Archivos nuevos

- tests/unit/NamespaceFunctionModelTests.cpp: prueba aislada del índice multiparchivo.
- Este documento.

## Mejora

El proyecto dispone por primera vez de una tabla completa y orden-independiente de funciones auténticas de namespace. Las fases siguientes podrán construir firmas y resolver llamadas sin inventar miembros de clase.

## Verificación específica

- Construcción incremental de nsc.
- Construcción y ejecución exclusiva de nsc_namespace_function_model_tests.
- Resultado: nsc_namespace_function_model_tests — PASS, 7 checks.
- Sin baseline, CTest general, fixtures ni matriz de optimización.
