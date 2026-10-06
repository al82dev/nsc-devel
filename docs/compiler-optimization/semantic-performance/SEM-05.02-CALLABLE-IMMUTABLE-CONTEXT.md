# SEM-05.02 — Contexto inmutable por callable

## Propósito

Hacer explícitas las entradas que un callable puede leer y separar su identidad estable del
estado mutable acumulado por `ProjectMethodBodyModel`.

## Contrato

`ProjectCallableAnalysisContext` conserva:

- ordinal estable del callable;
- ordinal estable de la oleada;
- catálogos de nombres, miembros, funciones libres, herencia e interfaces;
- modelos opcionales de genéricos y constraints;
- edición del lenguaje.

Todas las dependencias se publican como lectura. El contexto no contiene vectores de salida ni
scratch y no ofrece operaciones de mutación.

## Integración

El coordinador crea un contexto por elemento de la oleada. La admisión de especializaciones y
la unidad `analyzeCallable` consumen ese contexto en vez de reconstruir identidad y catálogos
desde variables implícitas del bucle.

## Archivos

- `ProjectCallableAnalysisContext.h/.cpp`;
- `ProjectMethodBodyModel.h/.cpp`;
- `CMakeLists.txt`;
- este documento.

## Validación dirigida

Build y ejecución individual de `nsc_project_method_body_model_tests` en O0. No baseline,
minibaseline, CTest general ni O1/O2/O3.
