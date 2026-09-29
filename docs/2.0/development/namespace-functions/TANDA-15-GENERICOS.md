# Tanda 15: funciones genericas de namespace

## Objetivo

Permitir que una funcion libre generica use la monomorfizacion existente sin inventar un tipo contenedor, y que sus plantillas publicas sigan siendo reutilizables desde NSMETA.

## Cambios

- El recolector generico reconoce FunctionDeclaration ademas de MethodDeclaration.
- Una funcion libre generica conserva ownerName vacio y usa namespace + nombre + aridad como identidad canonica.
- La accesibilidad predeterminada de la funcion libre generica es internal; no hereda el valor private propio de los metodos.
- Las plantillas abiertas se excluyen del lookup ordinario de funciones de namespace.
- Cada uso concreto clona la declaracion bajo su namespace real, sustituye tipos y entra al pipeline ordinario como funcion libre cerrada.
- La cache de especializaciones se comparte con el mecanismo generico existente.
- El materializador de LibraryReference reconstruye una plantilla callable sin exigir un tipo propietario.
- NSMETA publica el cuerpo fuente reutilizable de las funciones genericas publicas.

## Archivos modificados

- src/Frontend/ProjectGenericModel.cpp
- src/Frontend/ProjectNameLookup.cpp
- src/Driver/Driver.cpp
- CMakeLists.txt

## Archivo nuevo

- tests/unit/NamespaceFunctionGenericTests.cpp: prueba aislada de registro, identidad, especializacion, lookup, MIR y publicacion NSMETA.

## Mejora obtenida

Las APIs procedurales pueden expresar algoritmos genericos directamente en un namespace. Solo las especializaciones cerradas llegan a semantica y backend, mientras la declaracion abierta viaja como plantilla reutilizable para consumidores precompilados.

## Compatibilidad

- Target unico: Windows x64.
- No cambia Native ABI, Runtime ABI ni la version NSMETA.
- GenericTemplateKind::Method conserva su valor serializado y representa callables con o sin tipo propietario.

## Verificacion

- Construccion incremental exclusiva del objetivo nsc: correcta.
- Construccion exclusiva de nsc_namespace_function_generic_tests: correcta.
- Ejecucion exclusiva de nsc_namespace_function_generic_tests: PASS, 10 verificaciones.
- No se ejecuto baseline, CTest general, fixtures generales ni matriz de optimizaciones.