# Tanda 16 — limpieza posterior al baseline

## Objetivo

Dejar visible una acción de limpieza al terminar el baseline y retirar todos los artefactos
reconstruibles que producen la compilación del compilador y las pruebas.

## Cambio

El panel `G0.2 — baseline certificado` incorpora el botón `Clean`. Permanece deshabilitado durante
la corrida y se habilita cuando termina un baseline. Después de una limpieza exitosa vuelve a
deshabilitarse.

La limpieza existente se amplía para incluir:

- los builds configurados del compilador y de CTest;
- los árboles `build-*` conocidos por NSC Builder;
- `_nscbuilder/work/tests`;
- `_nscbuilder/temp`;
- los logs automáticos `error.log`, `error1.log`, etc. ubicados en la raíz del repositorio.

La confirmación resume los logs como un solo grupo para evitar diálogos con cientos de líneas. La
validación de rutas continúa rechazando cualquier destino que salga del repositorio o que no sea
un build directo, una de las dos áreas temporales exactas o un `errorN.log` directo.

## Archivos modificados

- `nscbuilder.py`: amplía los destinos seguros y añade el botón y su ciclo de estado.

## Archivo nuevo

- `docs/compiler-optimization/TANDA-16-post-baseline-clean.md`: documenta alcance, seguridad y
  verificación.

## Elementos conservados

- `_nscbuilder/history`, incluidos logs y reportes del baseline;
- `_abi` y sus certificados;
- las fuentes del repositorio;
- la instalación configurada, por defecto `C:\native-sharp`.

## Mejora

El baseline puede liberar inmediatamente el espacio ocupado por objetos, ejecutables de pruebas,
fixtures y diagnósticos automáticos sin perder la evidencia necesaria para auditar el resultado.

## Verificación dirigida

- Comprobación de sintaxis de `nscbuilder.py`.
- Inventario aislado de destinos aceptados y rechazados por `cleanup_targets`.
- Limpieza real solicitada por el usuario, con verificación posterior de destinos.
- No se volvió a ejecutar el baseline ni ninguna prueba del compilador.
