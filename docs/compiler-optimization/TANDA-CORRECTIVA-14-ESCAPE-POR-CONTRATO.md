# Tanda correctiva 14: escape sólo por contrato demostrado

## SCOPE LOCK

- Objetivo: impedir que un resumen de escape desconocido rechace direcciones, referencias o
  vistas de almacenamiento local.
- Naturaleza: corrección funcional localizada en la validación final de escape.
- Invariante: un parámetro con contrato publicado `Global` continúa rechazando almacenamiento
  del frame actual; `NoEscape` y contratos más débiles permanecen sin cambio.
- Revisión de partida: `f73f9d134`.
- Rango causal: activación global de la validación de escape al retirar las ediciones internas.
- Archivos permitidos: `src/Frontend/ProjectMethodBodyModel.cpp` y este documento.
- Archivos protegidos: parser, proyectos, metadata, HIR, MIR, backend, runtime, bibliotecas,
  fixtures, pruebas históricas, instalaciones y toda ruta en `C:`.
- Prohibido: baseline, minibaseline, matrices O1/O2/O3, deploy y cambios de pruebas.
- Terminación: sólo una retención `Global` demostrada produce el diagnóstico de escape.

## Causa raíz

`finalizeEscapeSummaries()` agrupaba `Unknown` y `Global` como contratos equivalentes. La falta de
información del callee se convertía así en una afirmación de retención y rechazaba programas que
antes eran válidos, incluidos consumidores de vistas y direcciones locales.

La validación debe operar sobre hechos publicados. `Unknown` significa que el compilador no posee
un contrato concluyente; no significa que el parámetro escape globalmente.

## Corrección

- La validación continúa únicamente cuando el resumen calculado es `EscapeClass::Global`.
- Se elimina la excepción parcial por clase de local porque ya no es necesaria: la decisión se
  toma por el contrato del parámetro, no por una lista de tipos del argumento.
- El diagnóstico conserva su texto preciso para retención global demostrada.

## Complejidad y seguridad

El recorrido iterativo y su complejidad O(celdas + aristas + argumentos) permanecen intactos. No se
añade recursión, estado global, allocation ni búsqueda adicional.

## Verificación

No se ejecutó baseline ni batería general local. El commit se produce antes de cualquier ejecución;
la batería O0 con NSLink se valida en la otra máquina del usuario.
