# Tanda correctiva 17: pruebas del lenguaje acumulativo

## Cambio

Las pruebas que convertían `Language` o `unsafe` en permisos obligatorios ahora validan el
contrato vigente:

- `Language` puede omitirse o contener valores históricos sin ocultar sintaxis ni romper grafos.
- NSMETA de distintas épocas puede convivir en el mismo grafo.
- Expresiones, agregados, patrones, lifetimes, pinning, `defer`, comprehensions y `foreach`
  pertenecen a una superficie acumulativa.
- Punteros, FFI, assembly, traits, function values y `assume` no requieren un wrapper `unsafe`.
- Las reglas reales de tipos, provenance, alineación, escape, target y ownership permanecen
  activas y conservan sus pruebas negativas.

## Implementación afectada

- `src/Language/ExpressionErgonomics.cpp`: disponibilidad acumulativa de expresiones.
- `src/Frontend/ProjectInterfaceModel.cpp`: la marca histórica `unsafe` no condiciona un `impl`.
- Pruebas dirigidas de compilación, frontend y Language 3 que conservaban expectativas de
  rechazo administrativas.

## Validación

El commit precede a la ejecución. La validación de cierre será la batería O0 completa con NSLink;
los resultados parciales sólo sirven para corregir errores de compilación del propio conjunto.
