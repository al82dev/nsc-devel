# Tanda 23: mensajes humanos de consola

## Objetivo

Extender `--language es` más allá del banner para que la ayuda descriptiva, los errores de opciones, el progreso y las advertencias propias del ejecutable se presenten en español.

## Diagnóstico

La tanda anterior resolvió el idioma antes del despacho, pero varios mensajes de arranque y validación seguían escritos directamente en inglés. La traza de optimización incluso contenía frases en español cuando el idioma predeterminado debía ser inglés.

## Cambios

- Se localizaron las advertencias de presupuesto O2 y supervisor de trazas LLVM.
- La traza de optimización ahora está completamente en inglés por defecto y en español sólo cuando se solicita.
- Se localizaron los errores principales de perfiles, trabajos, núcleos reservados, caché, referencias, tiempo límite, LTO, punto flotante, subsistema y nivel LLVM.
- La opción desconocida y la línea de comandos inválida respetan el idioma seleccionado.
- La ayuda en español conserva sin cambios los nombres de opciones y comandos, pero explica en español su comportamiento.
- Identificadores técnicos, rutas, valores permitidos, nombres de fase, JSON, códigos de salida y artefactos permanecen estables.

## Archivos modificados

- `src/nsc/main.cpp`: mensajes humanos del proceso anfitrión, registro y supervisor LLVM.
- `src/Driver/Driver.cpp`: validación de opciones, ayuda, progreso y advertencias del driver.
- `tests/unit/DriverTests.cpp`: errores españoles de opción desconocida y validación numérica.

## Archivo nuevo

- `docs/2.0/development/namespace-functions/TANDA-23-MENSAJES-HUMANOS-DE-CONSOLA.md`: alcance, decisiones y verificación de esta tanda.

No se agregaron catálogos externos ni archivos de recursos en tiempo de ejecución. Los dos idiomas se compilan dentro de `nsc`, por lo que la instalación continúa siendo un ejecutable autocontenido.

## Mejora obtenida

El idioma predeterminado deja de mezclar frases españolas y la selección española cubre la superficie de consola que el usuario usa para configurar y observar una compilación. La información destinada a herramientas conserva su esquema y vocabulario técnico.

## Verificación

- Construcción incremental exclusiva del target `nsc`.
- Ejecución individual de `nsc_driver_tests`.
- Invocaciones directas para ayuda, opción desconocida y validación `-j0` en español.
- No se ejecuta baseline ni CTest general.
