# Baseline 2026-10-04: cierre de goldens NSMETA 1.35–1.36

## Problema

Stage48.2 ya recorría NSMETA hasta 1.37, pero el archivo de fixtures terminaba en 1.34. Además, dos
expectativas seguían congelando el contrato anterior a Runtime ABI 1.2: la huella del modelo raíz y
la salida de `--verify-library-metadata`.

## Cambio

- Se actualizó la huella revisada del contrato raíz con Object ABI 2 y Runtime ABI 1.2.
- La verificación de consola exige `runtime-abi: 1.2`.
- Se archivaron los bytes NSMETA 1.35 y 1.36 usando el modo de creación que omite archivos ya
  existentes, por lo que ningún golden anterior fue reescrito.
- La prueba se ejecuta después sin modo de actualización para demostrar que los nuevos archivos
  son evidencia ordinaria de sólo lectura.

## Archivos

- `tests/unit/Stage48_2LibraryMetadataTests.cpp`
- `tests/fixtures/nsmeta/historical-1.35.nsmeta`
- `tests/fixtures/nsmeta/historical-1.36.nsmeta`

## Mejora

La ventana histórica queda completa hasta la versión inmediatamente anterior a NSMETA 1.37 y el
reporte público vuelve a reflejar el contrato ABI que el compilador realmente emite.

## Validación dirigida

- `nsc.stage48_2.library-metadata`: correcta con creación segura de los dos archivos ausentes.
- La misma prueba volvió a pasar inmediatamente en modo normal, sin ninguna variable de
  actualización de goldens.
