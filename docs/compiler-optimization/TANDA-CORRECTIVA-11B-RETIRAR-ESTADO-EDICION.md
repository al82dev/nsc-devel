# Tanda correctiva 11B: retirar estado de edicion

## Objetivo

Cerrar la advertencia tratada como error que revelo la primera compilacion de la tanda 11.

## Cambios

- Se retiraron de `Project` los campos privados de edicion que ya no participan en ninguna
  decision.
- Los parametros historicos del constructor se conservan temporalmente como costura de migracion
  y se ignoran de forma explicita.

## Mejora

El objeto de proyecto ya no conserva estado muerto capaz de volver a introducir bifurcaciones por
edicion. El cambio mantiene estables los call sites mientras se retira gradualmente la firma vieja.
