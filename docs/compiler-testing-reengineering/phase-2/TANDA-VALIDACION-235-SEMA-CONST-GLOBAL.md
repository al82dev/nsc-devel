# Tanda de validación 235 — diagnóstico causal de const global

Fecha: 2026-10-10

## Estado

IMPLEMENTADA. VALIDACIÓN DE PRODUCTO PENDIENTE.

## Causa confirmada

Una global `const` con inicializador de runtime atravesaba dos reglas
solapadas: la restricción general de inicializador global y la restricción
específica de `const`. El producto emitía `GlobalRequiresConstant` y
`ConstRequiresConstant` para una sola causa.

## Corrección

Las globales ordinarias conservan `GlobalRequiresConstant`; las globales
marcadas `const` delegan la misma condición a su regla específica y emiten
solamente `ConstRequiresConstant`.

## Alcance

La causa corresponde a `TS-SEMA-140`. No cambia la evaluación constante ni la
creación del símbolo global.

## Validación

No se compiló ni ejecutó por orden del usuario. La validación queda delegada a
su corrida posterior.
