# Tanda universal 20: foreach move-only por ownership

## Objetivo

Conservar el rechazo de copias implícitas en `foreach` cuando el elemento realmente
es move-only, sin derivar esa capacidad de una edición del lenguaje.

## Cambios

- Los proyectos dirigidos dejan de declarar una edición.
- El elemento move-only contiene un recurso `owned` explícito.
- El modo por valor sigue rechazando la copia implícita del elemento.
- La iteración por referencia continúa disponible sin una propiedad de versión.

## Archivo

- `tests/unit/NativeSharp3T09ForeachProtocolTests.cpp`

## Validación dirigida

Pendiente después del commit: `nsc.ns3.t09.foreach-protocol` en O0. No se ejecuta
baseline, certificación, promoción ni instalación.
