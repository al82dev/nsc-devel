# Tanda correctiva G00 — runner Python sin dependencia de batch

Fecha: 2026-10-01
Estado: terminada

## Resultado

`nscbuilder.py` es el orquestador único del baseline. Configura y construye con procesos directos,
ejecuta CTest directamente y compila, enlaza y ejecuta fixtures y bug triage mediante su runner
Python interno. No invoca archivos `.bat` ni `.cmd`.

La lista canónica de archivos de Gate 0 dejó de declarar como infraestructura activa a `all.bat`,
`run_windows_checklist.bat`, `run_bug_triage.bat`, `run_cases.py` y `report.py`. El contrato de las
cuatro optimizaciones se verifica ahora contra `make_baseline_plan()` y las implementaciones
internas `_run_case_group()` y `_triage()`.

## Archivos modificados

- `nscbuilder.py`.
- `tests/selfhost/test_bootstrap_infrastructure.py`.

## Verificación

- Búsqueda estática: cero referencias `.bat`, `.cmd`, `cmd.exe` o `ComSpec` en `nscbuilder.py`.
- Pruebas unitarias específicas de infraestructura; sin baseline y sin CTest.

## Limpieza heredada pendiente de autorización destructiva exacta

Los orquestadores anteriores todavía existen en Git para evitar borrarlos en bloque sin una
confirmación explícita de la lista. Ya no son dependencias de `nscbuilder.py`. Su eliminación debe
hacerse como una tanda separada porque también retirará scripts Python que sólo sirven a esa pila.
