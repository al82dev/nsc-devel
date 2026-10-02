# Tanda baseline 06I — bootstrap lock de NSLink 3.0

**Fecha:** 2026-10-02

**Estado:** terminada; pendiente de repetición del baseline por el usuario

**Alcance:** contrato congelado de recuperación SH53

**Baseline completo:** no ejecutado por Codex

## Fallo observado

La corrida `20261001-234156-b757f1ce` instaló y utilizó correctamente
`A:\native-sharp-3.0-baseline\bin\nslink.exe`, cuya identidad real es
`NSLink 3.0.0`. SH53 falló al validar el entorno congelado porque el lock
canónico todavía exigía el texto histórico `NSLink 0.1.0`.

El lock específico de la corrida ya había capturado el SHA-256 correcto del
ejecutable 3.0, por lo que únicamente la expectativa textual de versión quedó
desalineada. La validación fail-closed funcionó correctamente al rechazar esa
contradicción.

## Causa raíz

La tanda de alineación NSLink 3.0 actualizó el producto, SDK, instalación y
pruebas de NSLink, pero no actualizó el artefacto versionado
`selfhost/bootstrap/bootstrap.lock`. `nscbuilder.py` copia ese contrato para la
corrida y reemplaza los hashes de los artefactos seleccionados; deliberadamente
no inventa ni relaja las versiones esperadas. Por eso la copia congelada
combinó:

- SHA-256 actual de NSLink 3.0;
- expectativa textual heredada de NSLink 0.1.0.

## Cambios

- `selfhost/bootstrap/bootstrap.lock`
  - fija `NSLink 3.0.0` como identidad esperada;
  - fija el SHA-256 del binario canónico de NSLink 3.0:
    `2e9af3a605b1954f900d3c3fc0d0e1c90e711fce081e19f40c6f89aa7f8f73dd`.
- `tests/selfhost/test_bootstrap_infrastructure.py`
  - añade una prueba del lock canónico que exige argumento `--version`, texto
    exacto `NSLink 3.0.0` y un SHA-256 hexadecimal completo.

No se modificó el verificador para aceptar cualquier versión. SH53 continúa
fallando cerrado ante binarios, hashes o banners distintos.

## Archivos nuevos

Sólo se creó este documento de tanda. No se agregaron ejecutables, scripts,
fixtures ni formatos.

## Validación dirigida

- infraestructura bootstrap: 79/79 pruebas;
- lock temporal generado mediante `write_run_bootstrap_lock`, igual que en el
  baseline;
- entorno congelado: 8/8 artefactos aprobados (`nsc0`, clang-cl, lld-link,
  LLVM-C.lib, NSLink, Python, CMake y Ninja);
- CTest aislado `nsc.selfhost.distribution-final`/SH53: 1/1 aprobado en 11.45 s.

No se ejecutó el baseline completo ni se modificó `C:\native-sharp`.
