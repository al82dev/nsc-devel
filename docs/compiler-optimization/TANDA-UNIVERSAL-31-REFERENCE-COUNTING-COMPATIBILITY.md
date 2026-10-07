# Tanda universal 31: compatibilidad de conteo de referencias

## Problema confirmado

La sincronización de la biblioteca estándar eliminó
`lib/NativeSharp.Core.Threading/ReferenceCounting.ns`, aunque el contrato seguía siendo público,
estaba cubierto por `nsc.ns3.t10.reference-counts` y era consumido por código válido.

El fallo observable no era una regresión del parser ni del backend: los nombres
`ReferenceCounts`, `ReferenceCountHook` y `ReferenceRelease` dejaron de existir en las fuentes
empaquetadas.

## Cambio

Se restaura la superficie acumulativa completa:

- estado fuerte y débil embebible;
- retención atómica con protección de desbordamiento;
- liberación fuerte y débil con órdenes de memoria explícitos;
- hooks de destrucción y desalojamiento;
- convención del weak implícito mientras existe una referencia fuerte.

No se modificó el compilador, el verificador ni la expectativa de la prueba.

## Archivos

- Nuevo: `lib/NativeSharp.Core.Threading/ReferenceCounting.ns`.
- Nuevo: este documento.

## Validación requerida

1. Ejecutar `nsc.ns3.t10.reference-counts` en O0 con NSLink.
2. Ejecutar la batería CTest completa existente en O0 con NSLink.
3. Registrar por separado cualquier fallo de infraestructura no relacionado.

