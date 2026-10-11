# Validación 249: lifetime del analyzer con símbolos y scopes

## SCOPE LOCK

- Objetivo: materializar únicamente `TS-SEMA-094`.
- Naturaleza: prueba de lifecycle con oráculo sanitizer.
- Producto protegido: compilador, semántica, ABI y construcción de NSC.
- Pruebas protegidas: todos los casos distintos de `TS-SEMA-094`.
- Archivos autorizados: el cuerpo existente de `c094` y este documento.
- Acciones prohibidas: manifiesto, CMake, build, ejecución y cambios de producto.
- Terminación: analizar un fixture que cree símbolos y scopes y destruir toda la
  sesión sin fuga ni doble liberación bajo ASan/LSan.

## Caso

El fixture analiza un struct y una función con parámetro y variable local. Esta
entrada obliga al analyzer a poseer símbolos globales, símbolos locales y scopes
anidados antes de entrar en la frontera de destrucción.

La aserción ordinaria demuestra que el grafo fue construido. El perfil
sanitizer es el oráculo para fugas, dobles liberaciones y accesos posteriores a
la destrucción. El caso conserva `InfrastructurePending`; su perfil y su fila
de manifiesto se integrarán por separado.

## Evidencia

- Implementación: `tests/compiler-suite/host/modules/SemaAnalyzerLifecycleModule.cpp`.
- Contrato: `CS-SEMA-011`.
- Validación ejecutada: ninguna, por prohibición expresa.
- Integración pendiente: perfil sanitizer y fila del manifiesto.
