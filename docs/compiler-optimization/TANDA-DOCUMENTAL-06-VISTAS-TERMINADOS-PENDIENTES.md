# Tanda documental 06: vistas de terminados y pendientes

## Objetivo

Separar el tracking integral de NativeSharp 3.0 en dos consultas operativas reproducibles sin
crear otra autoridad de estado.

## Cambios

- Se centralizó la definición de estado terminal en el catálogo canónico.
- Se agregó un generador que lee `A:/NATIVESHARP-3.0-TRACKING.md`, verifica que coincida byte a
  byte con el documento maestro y produce:
  - `A:/NATIVESHARP-3.0-TERMINADOS.md`;
  - `A:/NATIVESHARP-3.0-PENDIENTES.md`.
- La vista de pendientes distingue dependencia/bloqueo, impedimento actual, condición exacta de
  cierre y evidencia existente para cada ID; extrae referencias cruzadas y bloqueos técnicos
  declarados sin inventar dependencias cuando el catálogo no las registra.
- Las dos vistas forman una partición completa: ningún ID se pierde ni aparece en ambas.
- `PRUEBAS DIRIGIDAS`, `REFERENCIA`, `EN CURSO`, `ENRUTADA` y `REQUIERE AUDITORÍA` no se presentan
  como trabajo terminado.

## Archivos

- Modificado: `tools/native_sharp_3_catalog.py`.
- Modificado: `tools/verify_native_sharp_3_catalog.py`.
- Modificado: `tools/native_sharp_3_source_inventory.py` para inventariar ambas vistas raíz.
- Nuevo: `tools/generate_native_sharp_3_status_views.py`.
- Nuevo: `tests/unit/test_native_sharp_3_status_views.py`.
- Nuevos productos generados en la raíz de `A:`: los dos documentos solicitados.
- El verificador canónico comprueba también que ambas vistas coincidan byte a byte.

## Corrección de estado detectada

`NS3-T06-F02` pasó de `TERMINADA` a `EN CURSO`: sus pruebas existentes cubren movimientos
parciales locales, pero la nota de la corrección c67 documenta que extraer un payload move-only
desde un field genérico todavía no está soportado. El ítem sólo podrá cerrarse cuando ese retorno
con `move` y su drop exacto queden implementados y probados.

## Validación

Sólo corresponde ejecutar la prueba unitaria dirigida del generador y su modo `--check`. No se
ejecuta baseline ni una suite general.
