# Tanda universal 13: escape entre parámetros del llamador

## Objetivo

Distinguir un puntero retenido globalmente de un puntero almacenado dentro de otro
objeto entregado por el llamador.

## Causa confirmada

El parser normaliza `instance->VTable` como `(*instance).VTable`. El acceso al miembro
sí seguía el struct sintetizado, pero el evaluador de `*` sólo reconocía arreglos de
longitud variable. Para cualquier otro puntero devolvía almacenamiento externo sin
procedencia; así perdía el ordinal de `instance` y clasificaba el destino como global.

La pérdida convertía el resumen de `table` en `Global`. Después, una llamada válida
con dos objetos del mismo frame era rechazada con `NS3020` aunque ninguno abandonara
el llamador.

## Implementación

- Se agregó una continuación iterativa para obtener la procedencia del pointee en `*`.
- La misma ruta cubre `p->campo` y su forma normalizada `(*p).campo`.
- Si el receptor es un parámetro puntero, su pointee se conserva como almacenamiento
  externo asociado a ese parámetro.
- La asignación entre parámetros se clasifica como `CallerOwned`, no `Global`.
- Un puntero local sin procedencia demostrable sigue cerrado de forma conservadora.
- No se introdujo recursión.

## Archivos

- `include/nativesharp/Frontend/ReferenceEscapeContinuationKind.h`
- `include/nativesharp/Frontend/ProjectReferenceEscapeEvaluator.h`
- `src/Frontend/ProjectReferenceEscapeEvaluator.cpp`

## Contratos preservados

- Guardar un puntero en almacenamiento estático continúa siendo `Global`.
- Pasar almacenamiento del frame a un contrato realmente global continúa produciendo
  `NS3020`.
- No se debilita el diagnóstico ni se modifica el fixture COM.
- No se toca NSMETA, NSLink ni la instalación en `C:`.

## Validación dirigida

Pendiente después del commit:

- `nsc.item339.com-vtable-production` en O0 como prueba ejecutable principal.
- La ejecución O2 existente puede observarse, pero no amplía el alcance solicitado.

No se ejecuta baseline, certificación ni promoción.
