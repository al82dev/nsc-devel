# Tanda correctiva 13: `move` optativo para valores copiables

## SCOPE LOCK

- Objetivo: restaurar la aceptación histórica de valores copiables cuando el programador escribe
  `move(...)` de forma explícita.
- Naturaleza: corrección funcional localizada en el análisis semántico de cuerpos.
- Invariante: un valor RAII o move-only conserva transferencia, invalidación del origen, pinning,
  préstamos activos y diagnósticos de doble movimiento.
- Revisión de partida: `8bc4d629a`.
- Rango causal: reglas de movimiento de `ProjectMethodBodyModel` activadas durante la retirada de
  modalidades globales por edición.
- Archivos permitidos: `src/Frontend/ProjectMethodBodyModel.cpp` y este documento.
- Archivos protegidos: parser, proyectos, metadata, HIR, MIR, backend, runtime, bibliotecas,
  fixtures, pruebas históricas, instalaciones y toda ruta en `C:`.
- Prohibido: baseline, minibaseline, matrices O1/O2/O3, deploy, cambios de pruebas y cambios de
  semántica no relacionados.
- Terminación: el binding representa un `move` sobre un valor copiable como lectura con semántica
  de copia, sin transferir ownership ni invalidar el origen.

## Causa raíz

La rama `MoveExpression` sólo aceptaba propietarios RAII, parámetros `owned`, valores move-only y
unas excepciones internas del compilador. La sintaxis ya reconocía `move(...)`, pero el analizador
rechazaba un local o parámetro copiable y también exigía que el destino fuera RAII o move-only.

Esta política contradice el contrato acumulativo: escribir `move` no debe convertir un valor
copiable en un propietario lineal ni volver inválido un programa que puede resolverse mediante una
lectura normal.

## Corrección

- Un local o parámetro que realmente requiere movimiento conserva la ruta de transferencia.
- Un local o parámetro copiable produce un `MethodMoveBinding` con `clearSourceStorage=false`.
- El resultado copiable publica `OwnershipKind::None` y `ownershipTransfer=false`.
- El destino ya no necesita estar marcado como RAII o move-only sólo porque el inicializador use
  `move(...)`.

## Complejidad y seguridad

La decisión sigue siendo O(1) por expresión. No se añade recursión, búsqueda, allocation ni estado
global. La ruta de valores propietarios permanece sin cambios.

## Verificación

No se ejecutó baseline ni batería general local. El cambio se registra antes de cualquier ejecución,
conforme al flujo acordado; la batería O0 con NSLink se ejecuta en la otra máquina del usuario.
