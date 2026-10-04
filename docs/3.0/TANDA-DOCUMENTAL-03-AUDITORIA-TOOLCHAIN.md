# Tanda documental 03 — auditoría del toolchain por contrato

## Objetivo

Reemplazar el estado genérico de NSLink, NSASM, NSDBG, NSC-LSP y VS Code por una evaluación individual basada en
los commits, expedientes y pruebas dirigidas disponibles en cada repositorio local.

## Resultado

- NSLink: ocho contratos con pruebas dirigidas y entrega NSDBG todavía en curso por la matriz O0–O3 pendiente.
- NSASM: identidad/distribución y handoff probados; cinco áreas históricas requieren auditoría específica 3.0.
- NSDBG: nueve contratos con pruebas dirigidas; quedan features nuevas, closures/optimización, async/generators y
  el candidato acumulado.
- NSC-LSP: nueve contratos con pruebas dirigidas; falta una regresión explícita que preserve físicamente `asm`.
- VS Code: seis contratos con pruebas dirigidas; la aceptación manual completa continúa abierta.
- Ningún componente fue marcado `CERTIFICADA` y no se ejecutó baseline.

## Automatización

El Documento maestro §25 contiene un resumen generado para los 16 componentes. Cada actualización del catálogo
regenera sus conteos; el verificador rechaza diferencias entre el resumen, §24 y el tracking raíz.

## Fuentes inspeccionadas

- NSLink: tandas 35–37, matriz de pruebas y cierres recientes de C++ estático/telemetría.
- NSASM: promoción 3.0 con runtime estático y evidencia de handoff T38.
- NSDBG: auditoría de 78 pruebas registradas, 11 regresiones ejecutadas, DAP y promoción 3.0 autocontenida.
- NSC-LSP: tandas 09–18 y cierres 38/39 contra `LanguageService` ABI 2.
- VS Code: históricos 063–067 y cierre 3.0 con contratos focalizados.

## Archivos

- `tools/native_sharp_3_catalog.py`: genera el resumen por componente.
- `tools/verify_native_sharp_3_catalog.py`: comprueba que el resumen sea derivado.
- `tests/unit/test_native_sharp_3_catalog.py`: regresión del conteo por estado.
- Este expediente.
- Fuera del repositorio: Documento maestro y tracking generado en `A:\`.

## Validación permitida

Sólo se ejecutan las pruebas unitarias documentales y el verificador del catálogo. No se compila ningún componente,
no se ejecuta CTest general y no se ejecuta baseline.
