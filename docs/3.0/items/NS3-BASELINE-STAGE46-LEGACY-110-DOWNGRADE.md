> **Aviso vigente sobre NSMeta:** El versionado de NSMeta ya no aplica. Las menciones de versiones, compatibilidad por versión y requisitos de versión de este documento son históricas y no constituyen un contrato vigente. No debe reintroducirse el versionado de NSMeta ni rechazarse su lectura o compilación por una versión; la aceptación se determina por el contenido y los contratos aplicables.

# NS3 baseline: degradación completa de la fixture NSMETA 1.10

## Problema

La fixture histórica de Stage 46 copiaba metadatos actuales, cambiaba solamente el minor a 1.10 y
conservaba contratos genéricos introducidos en NSMETA 1.38. El codec rechazaba correctamente esa
combinación imposible y las comprobaciones posteriores fallaban por falta del archivo degradado.

## Cambio

Antes de escribir la copia 1.10 se eliminan de sus plantillas genéricas los contratos de parámetros,
los parámetros de lifetime y las restricciones `outlives`. No se relaja el codec ni se permite que
un archivo antiguo anuncie semántica que su formato no puede representar.

## Archivos

- `tests/unit/Stage46NativeLibrariesTests.cpp`

## Mejora

La prueba construye una fixture históricamente válida y sigue comprobando su objetivo original: que
un consumidor moderno ignore la jerarquía espuria que productores 1.10 asociaban a clases estáticas.
