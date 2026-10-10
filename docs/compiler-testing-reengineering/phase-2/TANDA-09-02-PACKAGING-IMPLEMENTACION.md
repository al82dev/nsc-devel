# Tanda 09-02 — Packaging

## Alcance

- Componente: `Packaging`.
- Contratos canónicos: `C12B-PKG-001..003`.
- Casos canónicos: `18`.
- IDs: `T12B-00457..00468` y `T12B-01069..01074`.
- Fuentes de producto: `deploy.bat`, `deploy.py` y `package_native_sharp.py`.
- Producto modificado: ninguno.
- Validación ejecutada: ninguna; el cierre de esta tanda es exclusivamente de implementación.

## Clasificación materializada

| Disposición | Casos | Evidencia |
|---|---:|---|
| Required ejecutable | 8 | `deploy.bat` sin CMake y repetición; seis fronteras reales de `package_native_sharp.py` |
| KnownProductDefect ejecutable | 4 | `deploy.bat` ignora build/prefix elegidos y usa rutas fijas de A:/C: |
| InfrastructurePending | 6 | `deploy.py` sólo expone un flujo GUI modal PySide6 sin seam de automatización seguro |
| **Total** | **18** | denominador canónico completo |

Los casos `InfrastructurePending` no se insertan en el manifiesto ordinario. Los cuatro defectos conocidos sí
permanecen ejecutables y rojos: el controlador no los convierte en éxito.

## Contratos ejecutables

`C12B-PKG-001` invoca el `deploy.bat` real con un `cmake.cmd` espía aislado. El espía evita cualquier escritura
en `C:` y permite observar el comando que el wrapper intenta ejecutar. Los casos nominales, de estructura y de
borde demuestran el defecto de rutas fijas; la dependencia ausente y la repetición conservan oráculos positivos.

`C12B-PKG-003` invoca `package_native_sharp.py` como proceso real sobre una copia temporal de la instalación
seleccionada. El caso representativo usa el ejecutable de archivo elegido explícitamente; los demás cubren dry
run, productor ausente, compresión inválida, timestamp vacío y determinismo con entradas fijas. Ningún caso usa
la instalación oficial de `C:` ni una ruta de herramienta asumida.

## Contradicción preservada

El plan textual declara `15 PR + 3 NIGHTLY`, mientras `suite-case-allocation.csv` asigna los `18` casos a
`NIGHTLY`. El controlador usa la matriz de asignación como autoridad operativa y conserva la contradicción en
`PackagingPlanContradictions.jsonl`.

## Integración

- Controlador: `tests/compiler-suite/controller/integration/run_packaging_gate.py`.
- Catálogo: `tests/compiler-suite/packaging/PackagingContractCatalog.jsonl`.
- Clasificaciones: `tests/compiler-suite/packaging/PackagingCaseClassifications.jsonl`.
- Contradicción: `tests/compiler-suite/packaging/PackagingPlanContradictions.jsonl`.
- Manifiesto ordinario: `12` filas (`8` Required y `4` KnownProductDefect).
- Gate CTest: se registra sólo con NSC y herramienta de archivo elegidos explícitamente.

## Estado

`CIERRE ESTÁTICO COMPLETO; VALIDACIÓN DE BUILD PENDIENTE DEL USUARIO`.
