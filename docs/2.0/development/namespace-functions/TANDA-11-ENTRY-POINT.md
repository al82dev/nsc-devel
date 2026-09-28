# Tanda 11 - Main como funcion libre

## Objetivo

Permitir que una funcion libre int Main() sin parametros sea el punto de entrada de un ejecutable NativeSharp.

## Cambios realizados

- ProjectOopMirModel centraliza la seleccion del punto de entrada.
- La regla acepta:
  - una funcion libre int Main() sin parametros y con linkage NativeSharp;
  - el metodo estatico int Main() tradicional.
- Dos candidatas, incluso una libre y una de clase, producen ambiguedad explicita.
- Una funcion Main con firma incorrecta no se selecciona.
- El driver usa exclusivamente la decision centralizada de MIR y enlaza su nombre fisico congelado.

## Archivos modificados

- include/nativesharp/Frontend/ProjectOopMirModel.h
- src/Frontend/ProjectOopMirModel.cpp
- src/Driver/Driver.cpp
- CMakeLists.txt

## Archivos nuevos

- tests/unit/NamespaceFunctionEntryPointTests.cpp
  - Se creo para validar la seleccion sin depender de una instalacion externa del runtime.
  - Cubre Main libre valido, simbolo fisico, firma invalida y ambiguedad con Main de clase.

## Mejora obtenida

Los proyectos ya no necesitan una clase Program artificial para arrancar. La seleccion tiene una sola implementacion compartida por el modelo y el driver, evitando que ambos diverjan.

## Validacion enfocada

- Construccion exclusiva de nsc: correcta.
- Construccion exclusiva de nsc_namespace_function_entry_tests: correcta.
- Ejecucion exclusiva de nsc_namespace_function_entry_tests: PASS: 6 checks.
- El intento inicial de enlace detecto que la configuracion local apunta a una biblioteca runtime instalada fuera de A: que no existe; la prueba final elimina esa dependencia externa.
- No se ejecuto baseline, CTest completo, matrices ni fixtures generales.