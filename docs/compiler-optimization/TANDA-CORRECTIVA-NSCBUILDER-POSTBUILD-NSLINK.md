# Tanda correctiva: NSLink en el post-build de NSCBuilder

## Problema

La construcción enlazaba correctamente `nsc.exe` con Clang/LLD, pero el mismo objetivo ejecutaba
después `nsc.exe --write-windows-runtime-library`. NSCBuilder eliminaba `NSC_LINKER` del entorno y
la copia de NSLink del staging se preparaba hasta después de construir e instalar. La generación de
`NativeSharp.Runtime.Win32.lib` fallaba aunque el ejecutable principal ya estuviera enlazado.

El cierre de la validación recibía entonces una fotografía `after-build` inexistente. Al compararla
como si fuera una medición real, reportaba además hashes ausentes y la aparición de `lld-link` y
`nslink`, ocultando que todos eran efectos secundarios de la construcción interrumpida.

## Cambios

- `nscbuilder.py` proporciona al comando de construcción el NSLink fuente configurado en A:.
- El enlace del compilador anfitrión continúa usando el LLD configurado por CMake; `NSC_LINKER`
  sólo atiende el post-build ejecutado por el compilador recién construido.
- Las construcciones ordinarias validan que el NSLink configurado exista antes de comenzar.
- Una fotografía `after-build` no capturada queda marcada explícitamente y ya no genera falsas
  divergencias de binarios o herramientas.
- `tests/unit/test_nscbuilder_pipeline.py` cubre el orden previo al staging, la omisión segura de
  comparaciones imposibles y la conservación de las comprobaciones estrictas para snapshots reales.

## Archivos

- Modificado: `nscbuilder.py`.
- Nuevo: `tests/unit/test_nscbuilder_pipeline.py`, prueba dirigida y aislada del pipeline.
- Nuevo: este documento de trazabilidad.

## Mejora

NSCBuilder puede construir el objetivo `nsc` desde un staging limpio sin depender de una copia
anterior de NSLink y los reportes conservan una sola causa accionable cuando una etapa previa falla.

## Validación permitida

Sólo se ejecutan las pruebas unitarias dirigidas de este archivo y una construcción incremental del
objetivo `nsc`. No se ejecuta baseline, CTest general ni matriz de optimizaciones.

Resultado:

- `test_nscbuilder_pipeline.py`: 3/3 pruebas aprobadas.
- Construcción incremental de `nsc`: aprobada.
- Contrato de runtime estático del host: aprobado.
- `NativeSharp.Runtime.Win32.lib`: generado correctamente mediante el NSLink configurado.
- Baseline, CTest general y matriz de optimizaciones: no ejecutados.
