# Tanda universal 38: herramientas staged del baseline

## Problema confirmado

`PipelineWorker._base_environment("NSLink")` ignoraba el plan activo y usaba constantes globales
fijadas a `C:\native-sharp`. Durante un baseline esto hacía que las pruebas validaran herramientas
instaladas previamente en C: en vez de `nsc.exe` y `nslink.exe` staged en
`A:\native-sharp-3.0-baseline`.

La prueba 147 detectaba correctamente esa fuga de aislamiento.

## Cambio

Se centraliza la selección efectiva de herramientas:

- operación `baseline`: `prefix/bin/nsc.exe` y `prefix/bin/nslink.exe`;
- operaciones ordinarias: `C:\native-sharp\bin\nsc.exe` y
  `C:\native-sharp\bin\nslink.exe`;
- configuración CMake, precondiciones, caché y casos O0 usan la misma selección.

C: no se modifica. El baseline continúa tomando el binario fuente de NSLink desde A:, lo copia al
prefix staged de A: y valida esa copia.

## Archivos

- Modificado: `nscbuilder.py`.
- Nuevo: este documento.

## Validación requerida

1. Ejecutar `nsc.selfhost.bootstrap-infrastructure`.
2. Confirmar la prueba de staging y la selección ordinaria de herramientas.
3. Incluirlo en la corrida integral final.
