# Tanda correctiva G00 — inventario fuente SH40

Fecha: 2026-10-01
Estado: terminada, pendiente de baseline completo

## Fallo observado

SH40 continuaba congelado en 96 unidades del grafo y 40 unidades de NativeSharp.Core. El árbol
actual contiene 41 fuentes Core; por tanto, el cierre self-host válido contiene 97 unidades y 71
unidades externas analizadas. Los cuatro perfiles producían exactamente el mismo inventario nuevo,
pero el verificador rechazaba O0 antes de comparar O1–O3.

## Cambios

- `selfhost/bootstrap/verify_bootstrap_dependencies.py`: actualiza los conteos canónicos a 97/41/71.
- No se relajó ninguna comprobación: proyectos, propietarios, ausencia de binarios ocultos,
  resoluciones cross-project y equivalencia byte a byte entre O0–O3 siguen siendo obligatorios.

## Mejora obtenida

El gate mide el árbol fuente real de NativeSharp 3.0 sin confundir una ampliación deliberada de
Core con deriva del bootstrap.

## Validación específica

- Verificador SH40 ejecutado directamente sobre las cuatro salidas ya generadas: correcto.
- Reporte dirigido: 5 proyectos, 97 unidades, 71 externas analizadas y 5,563 resoluciones.
- El reporte temporal fue eliminado.
- No se reconstruyó ni ejecutó el baseline.
