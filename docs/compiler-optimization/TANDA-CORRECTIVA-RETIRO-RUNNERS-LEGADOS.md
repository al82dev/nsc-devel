# Tanda correctiva — retiro de runners heredados

Fecha: 2026-10-01
Estado: terminada, pendiente de baseline completo

## Objetivo

Eliminar la segunda infraestructura de construcción y pruebas que coexistía con `nscbuilder.py`.
El baseline, CTest, fixtures y bug triage quedan bajo un solo runner Python. `deploy.bat` se conserva
intacto por instrucción explícita.

## Archivos eliminados

- Orquestadores batch: `all.bat`, `build_nsc.bat`, `build-all.bat`, `check_entorno.bat`,
  `nslink_solo.bat`, `run_bug_triage.bat`, `run_windows_checklist.bat` y `test_nsc.bat`.
- Helpers CMD: `tools/build_project.cmd`, `tools/clean_project.cmd`,
  `tools/prepare_libraries.cmd` y `tools/setup_toolchain.cmd`.
- Runners Python reemplazados: `build_al_dia.py`, `run_cases.py`, `report.py`,
  `nslink_pipeline.py` y `verify_manifest.py`.

Todos permanecen recuperables desde el historial de Git.

## Reemplazos necesarios

- `tools/build_project.py`: compila un `.nsproj` invocando `nsc.exe` directamente; emite objeto y
  ejecutable sin `cmd.exe` ni sincronización de bibliotecas desde C:.
- `tools/clean_project.py`: elimina artefactos de un ejemplo sin depender de batch.
- Los 47 ejemplos/tutoriales conservan sus wrappers de usuario, pero esos wrappers llaman a las
  herramientas Python nuevas. Sus 47 wrappers de limpieza hacen lo mismo.
- `cases_table.py`, CMake y el README de bug triage describen ahora el runner único.

## Dirección de dependencias

`nscbuilder.py` no contiene referencias a `.bat`, `.cmd`, `cmd.exe` ni `ComSpec`. Los wrappers de
ejemplos pueden iniciar Python por comodidad, pero ninguna parte del compilador, del baseline o del
runner Python depende de ellos.

## Verificación específica

- Sintaxis correcta de `nscbuilder.py`, `tools/build_project.py` y `tools/clean_project.py`.
- 3/3 pruebas unitarias de infraestructura correctas.
- Cero wrappers apuntando a `tools/build_project.cmd` o `tools/clean_project.cmd`.
- `deploy.bat` sin modificaciones.
- `git diff --check` sin errores del parche.
- No se ejecutó baseline, CTest ni una compilación del compilador.
