# SEM-06.06 — Equivalencia O0 a través del Driver

## Propósito

Cerrar la diferencia entre una prueba directa del modelo semántico y la ruta CLI que utiliza el
usuario. El fixture aislado invoca el Driver con `--dump-project-method-bodies`, `-O0` y
`-j 1`, `-j 2`, `-j 12`.

## Contrato comprobado

- Las cuatro ejecuciones terminan correctamente: `j1`, `j2`, `j12` y repetición de `j12`.
- `stderr` permanece vacío.
- Los dumps son idénticos byte por byte.
- El SHA-256 de cada dump coincide y contiene 64 dígitos hexadecimales.
- Una ejecución O0 `j2 --time` expone `analysis`, `wait`, `merge` y
  `peak_working_set_bytes` para la primera oleada.

El fixture contiene 24 callables y una especialización genérica descubierta desde sus cuerpos, por
lo que no reduce la prueba a una única tarea trivial.

## Archivos

- `tests/unit/ProjectCallableWaveDriverTests.cpp`
- `CMakeLists.txt`

El test vive en un ejecutable propio para no convertir la ejecución dirigida en todo el conjunto
monolítico de `nsc_driver_tests`.

## Evidencia esperada

```powershell
cmake --build A:\nsc-devel\build-test-nslink `
  --target nsc_project_callable_wave_driver_tests --parallel 12
A:\nsc-devel\build-test-nslink\tests\nsc_project_callable_wave_driver_tests.exe
```

Resultado: `PASS: 6 checks`.

`clang-tidy` no reportó hallazgos propios después de documentar la supresión local del falso
positivo de `main`: el cuerpo captura, reporta y convierte tanto excepciones estándar como
desconocidas en código de salida 1. Los avisos restantes pertenecen a headers preexistentes.

No se ejecutan linker, baseline, minibaseline, CTest general ni O1/O2/O3.
