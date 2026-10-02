# Tanda baseline 06C: expectativas SH40 y T44

## Objetivo

Corregir tres expectativas obsoletas detectadas por el baseline después de
actualizar el contrato del runtime y ampliar el frontend de NativeSharp 3.0.

## Cambios

- La prueba de infraestructura bootstrap ahora valida el contrato vigente:
  runtime ABI `1.1` y conjunto de símbolos `3`.
- SH40 ahora espera los conteos actuales y consistentes producidos en O0-O3:
  98 unidades del grafo y 27 unidades del compilador. El informe estructurado
  publica igualmente 98 unidades.
- T44 pasa el nombre canónico `NSLink` al compilador en sus dos invocaciones;
  se elimina la variante minúscula que el contrato del driver rechaza.

## Archivos modificados

- `tests/selfhost/test_bootstrap_infrastructure.py`
- `selfhost/bootstrap/verify_bootstrap_dependencies.py`
- `cmake/VerifyNativeSharpT44MetadataClosure.cmake`

## Archivos nuevos

- Este documento, para conservar la trazabilidad obligatoria de la tanda.

## Mejora obtenida

Las pruebas dejan de comparar contra datos anteriores a la ABI 1.1 y contra
los conteos previos a la unidad adicional del compilador. T44 vuelve a probar
la ruta real de usuario mediante NSLink sin depender de una grafía rechazada.

## Validación dirigida

- Prueba unitaria exacta del contrato runtime: 1/1 correcta en 0.002 s.
- `nsc.ns3.t44.metadata-closure`: correcta en 1.25 s.
- `nsc.selfhost.bootstrap-dependencies`: correcta en 19.53 s.
- Total CTest dirigido: 2/2 en 20.80 s.
- No se ejecutó el baseline completo ni se modificó `C:`.
