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

## Evidencia ejecutada

- Commit bajo prueba: `dd3c6c202`.
- Prueba dirigida O0/NSLink: `nsc.ns3.t10.reference-counts`, 1/1 aprobada en 1.62 s.
- Reconstrucción completa de `build-test-nslink`: correcta, código de salida 0.
- Batería completa: 646/684 aprobadas en 1908.26 s.
- Resultado de esta tanda: la prueba 92 desapareció de la lista de fallos.
- Regresiones nuevas atribuibles a `ReferenceCounting.ns`: ninguna.

La corrida local conserva 38 fallos. Catorce son adicionales frente a la matriz estable de la
otra máquina (`24`, `25` y `151` a `164`) y corresponden a resolución local de `cmd.exe`,
`NS_ROOT`, roots o linkers; se mantienen separados del resultado funcional de esta tanda.
