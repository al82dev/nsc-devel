# Tanda 19 — Medición final

## Método

Se añadió `tools/measure_test_infrastructure.py`, una herramienta Windows con dos operaciones:

- `run`: ejecuta un comando y muestrea cada 100 ms su árbol de procesos, tiempo de pared, procesos
  únicos/concurrentes y memoria agregada;
- `snapshot`: inventaría CTest, targets Ninja, ejecutables lógicos, `.ninja_log`, espacio en disco y
  reportes del harness.

El árbol de procesos, el directorio y el grafo de descendientes se recorren de forma iterativa. No
se usa recursión. Los casos negativos se contabilizan mediante el veredicto verificado `passed`, no
por su exit code esperado.

La comparación limpia usó la misma revisión, Release, Clang/LLVM, `/MT`, 12 jobs y los targets
`nsc pruebas`. La única diferencia fue:

- histórico: `NSC_TEST_INFRASTRUCTURE_V2=OFF`;
- nuevo: `NSC_TEST_INFRASTRUCTURE_V2=ON`.

Cada lado se midió una sola vez desde un directorio nuevo. Es evidencia de ingeniería para este
corte, no un benchmark estadístico: no se hicieron warmups ni se descartaron outliers.

## Resultado de configuración y construcción limpia

| Métrica | Histórico | v2 | Reducción |
|---|---:|---:|---:|
| Configuración | 6.004 s | 4.597 s | 23.43% |
| Build `nsc pruebas` | 455.687 s | 281.515 s | 38.22% |
| Entradas CTest | 684 | 3 | 99.56% |
| Ejecutables lógicos Ninja | 430 | 2 | 99.53% |
| Objetos compilados | 707 | 522 | 26.17% |
| Tiempo agregado de edges `.obj` | 4,748.261 s | 3,295.785 s | 30.59% |
| Enlaces `.exe` | 430 | 2 | 99.53% |
| Tiempo agregado de edges `.exe` | 703.230 s | 6.583 s | 99.06% |
| Procesos únicos observados | 1,985 | 562 | 71.69% |
| Máximo de procesos concurrentes | 38 | 16 | 57.89% |
| Pico agregado de working set | 4,068,659,200 B | 3,940,519,936 B | 3.15% |
| Archivos del árbol limpio | 1,192 | 580 | 51.34% |
| Espacio del árbol limpio | 15,278,373,651 B | 302,288,488 B | 98.02% |

Los tiempos agregados de Ninja suman duración de edges paralelos y no son tiempo de pared. El wall
time comparable es la fila `Build nsc pruebas`.

## Duración funcional O0 v2

La evidencia de cierre de la Tanda 18 produjo:

| Capa | Selección | Resultado | Tiempo de pared |
|---|---:|---:|---:|
| Unidad consolidada | 289 checks | 289/289 | 103.95 s |
| Manifiestos NativeSharp | 76 casos | 76/76 | 0.87 s |
| Suites especializadas | 28 suites | 28/28 | 1509.29 s |
| Total | — | sin fallos | 1614.11 s (26 min 54.11 s) |

No se ejecutó la batería histórica de 684 entradas para fabricar una cifra “antes”: hacerlo habría
sido el baseline largo expresamente prohibido. Por tanto, este informe demuestra la reducción del
costo de configurar/construir/enlazar y registra el tiempo O0 v2, pero no afirma una aceleración de
ejecución funcional contra una corrida histórica no controlada.

## Selecciones rápidas

| Selección | Resultado | Wall time | Procesos únicos | Pico working set |
|---|---:|---:|---:|---:|
| Categoría `diagnostics` (6 casos) | 6/6 | 0.888 s | 5 | 51,384,320 B |
| Caso `nsc.codegen-target` | 1/1 | 0.232 s | 1 | 20,766,720 B |

La categoría y el caso usaron `A:/nsc-devel/build-test-v2/bin/nsc.exe`, O0 y NSLink explícito de
`A:/nslink-devel/bin/release/nslink.exe`.

## Archivos y reproducción

- Herramienta: `tools/measure_test_infrastructure.py`.
- Pruebas: `tests/harness/tests/test_measure_test_infrastructure.py`.
- Datos completos: `TANDA-TEST-INFRA-19-METRICS.json`.
- Validación unitaria total del harness: 30/30.

El JSON conserva comandos, rutas, muestras, conteos, memoria y fuentes de reportes. Los directorios
limpios de medición son artefactos temporales y se eliminaron después del snapshot; se recrean con
los comandos registrados.

## Exclusiones

No se ejecutó baseline, certificación, promoción, instalación, despliegue, O1, O2 ni O3. No se
modificó el compilador, `nscbuilder.py`, `_nscbuilder` ni `C:`.
