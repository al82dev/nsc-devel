# Tanda — limpieza del historial de NSCBuilder

Fecha: 2026-10-04

Rama: `development`

Estado: terminada

## Cambios

- La pestaña `Historial` incorpora `Eliminar corrida` para borrar la fila seleccionada.
- `Vaciar historial` elimina todas las corridas visibles y su evidencia.
- Ambas operaciones requieren confirmación explícita y muestran el número o la ruta afectados.
- La limpieza se rechaza mientras existe una ejecución activa.
- Cada destino se valida como hijo directo de `A:\nsc-devel\_nscbuilder\history`; nunca se elimina
  la raíz ni una ruta externa.
- Los baselines y gates certificados quedan fuera del alcance de esta función.
- Si se elimina la corrida que estaba abierta, NSCBuilder deshabilita `Abrir resultados`.
- Los fallos parciales se presentan con su ruta y diagnóstico.

## Archivos modificados

- `nscbuilder.py`

## Archivos nuevos

- `docs/compiler-optimization/TANDA-NSCBUILDER-LIMPIEZA-HISTORIAL.md`: expediente de esta tanda.

## Mejora

El historial ordinario de compilaciones y pruebas puede administrarse desde la misma interfaz sin
borrar manualmente carpetas ni poner en riesgo el staging, los gates o los baselines certificados.

## Validación

No se ejecutaron pruebas, compilaciones ni baseline por instrucción expresa del usuario.
