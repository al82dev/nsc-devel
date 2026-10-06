# SEM-04.04 — Contrato independiente del origen de escape

## Propósito

Preparar la extracción del evaluador de orígenes de escape sin mantener sus contratos dentro
del encabezado monolítico de `ProjectMethodBodyModel`.

## Cambio

Se separaron, uno por archivo, el enum `MethodEscapeOriginKind` y el valor
`MethodEscapeOrigin`. `ProjectMethodBodyModel` consume ahora esos contratos mediante include y
ya no los declara inline.

## Archivos

- `include/nativesharp/Frontend/MethodEscapeOriginKind.h`;
- `include/nativesharp/Frontend/MethodEscapeOrigin.h`;
- `include/nativesharp/Frontend/ProjectMethodBodyModel.h`;
- este documento.

## Mejora

El evaluador iterativo puede vivir en un componente propio sin depender de definiciones
accidentales del encabezado de method bodies. No cambia representación, valores, semántica ni
ABI interna del compilador.

## Validación dirigida

Construcción y ejecución exclusiva de la prueba Stage 50.7 de borrow/escape en O0. No se
ejecuta baseline, minibaseline, CTest general ni configuraciones O1/O2/O3.
