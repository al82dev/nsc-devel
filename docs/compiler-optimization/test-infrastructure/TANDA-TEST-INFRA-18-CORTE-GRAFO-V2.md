# Tanda 18 — Corte del grafo de pruebas v2

## Propósito

Esta tanda crea el punto de entrada CMake/CTest de la infraestructura nueva sin modificar el
compilador, `nscbuilder.py`, `_nscbuilder`, instalaciones en `C:` ni las expectativas históricas.
El corte es opt-in mediante `NSC_TEST_INFRASTRUCTURE_V2`; el modo histórico permanece inactivo en
ese árbol, pero sigue disponible porque 291 filas del inventario contienen perfiles u operaciones
que el scope O0 prohíbe ejecutar durante esta migración.

## Cambios

### Grafo CMake/CTest

- `NSC_TEST_INFRASTRUCTURE_V2=ON` evita crear los cientos de ejecutables históricos.
- El target `pruebas` construye una sola vez `nsc.exe` y `nsc_unit_tests.exe`.
- CTest publica solamente tres entradas:
  - `nsc.unit.consolidated`;
  - `nsc.harness.cases-o0`;
  - `nsc.harness.specialized-o0`.
- `NSC_TEST_SUITE_PREFIX`, `NSC_NSLINK_EXECUTABLE` y el runtime quedan explícitos.
- El `PATH` de las pruebas se reconstruye con directorios de herramientas reales para no heredar
  historiales `_nscbuilder` ni perder `cmd.exe` por truncamiento del entorno.
- El workspace especializado usa `${CMAKE_BINARY_DIR}/w`; SH44 añade deliberadamente una ruta
  larga y necesita conservar su presupuesto de longitud en Windows.

### Harness

- Los casos de manifiesto aceptan raíces de bibliotecas y runtime explícitas.
- `SuiteAdapter` sustituye tanto definiciones CMake como las opciones posicionales `--compiler` y
  `--work-root` de propietarios Python.
- Ninguna suite puede aprobar usando silenciosamente `A:/native-sharp-3.0-baseline/bin/nsc.exe` o
  escribiendo en `build-test-nslink`.
- La adaptación es iterativa; no se añadió recursión.

## Archivos modificados

- `CMakeLists.txt`: grafo v2 opt-in, entorno determinista y tres entradas CTest.
- `tests/unit/CMakeLists.txt`: registro y entorno del ejecutable consolidado.
- `tests/harness/nsc_test/command.py`: raíces de biblioteca/runtime congeladas por corrida.
- `tests/harness/nsc_test/suites.py`: selección explícita del compilador y workspace de propietarios
  posicionales.
- `tests/harness/run_nsc_tests.py`: opciones CLI correspondientes.
- `tests/harness/tests/test_suite_adapter.py`: prueba contra regresiones al NSC y árbol viejos.
- `test-migration-state.json` y `TRACKING.md`: estado y evidencia de la tanda.

No se creó código fuente productivo ni se modificó la semántica de NativeSharp.

## Trazabilidad

El inventario de 684 IDs históricos permanece completo en
`test-destination-matrix.json`:

| Destino histórico | Filas |
|---|---:|
| `KEEP_CPP_UNIT` | 59 |
| `MIGRATE_NS_FIXTURE` | 556 |
| `KEEP_SPECIALIZED` | 69 |
| Total | 684 |

Los números de ejecución v2 no deben sumarse como si fueran IDs CTest históricos uno a uno:

| Capa v2 | Checks/casos ejecutados | Resultado |
|---|---:|---:|
| Registro interno consolidado | 289 | 289/289 |
| Manifiestos NativeSharp O0 | 76 | 76/76 |
| Suites especializadas O0 | 28 | 28/28 |

Un ejecutable C++ histórico puede contener varios checks internos. La matriz de 684 filas conserva
la identidad del propietario original y `test-migration-state.json` conserva la decisión tomada;
el reporte v2 conserva cada check/caso ejecutado.

## Defectos encontrados y corrección causal

### Entorno `PATH`

La primera corrida consolidada obtuvo 287/289. Los dos fallos (`console-input` y
`package-coherence`) no encontraban `cmd.exe`: el entorno heredado contenía cientos de rutas de
historial y Windows lo truncaba. Con un `PATH` mínimo ambas pruebas pasaron; CMake ahora deriva un
entorno determinista de `System32` y las herramientas configuradas.

### SH44

La primera matriz especializada obtuvo 27/28. SH44 falló en `long-path/O0`; la ruta máxima creada
por el prefijo original medía 266 caracteres. El workspace corto redujo la ruta máxima a 255 sin
debilitar el fixture. La prueba dirigida pasó 1/1 y la matriz completa posterior pasó 28/28.

### Compilador obsoleto en conformidad

La auditoría del primer reporte encontró que `nsc.ns3.t00.conformance-compiler` todavía recibía el
NSC instalado anterior como valor de `--compiler`. El adaptador ahora congela opciones
posicionales. El reporte final contiene `A:/nsc-devel/build-test-v2/bin/nsc.exe`, cero referencias
al NSC anterior y cero referencias a `build-test-nslink`.

## Evidencia

- Harness Python: 25/25.
- CTest v2 enumerado: 3 entradas.
- Registro consolidado: 289/289, 103.95 s.
- Manifiestos O0: 76/76, 0.87 s.
- SH44 dirigida: 1/1.
- Suites especializadas O0 completas: 28/28, 1509.29 s.
- Reporte especializado final:
  `build-test-v2/harness-results/20261008T172129.759763Z/summary.json`.
- Compilador observado: `A:/nsc-devel/build-test-v2/bin/nsc.exe`.
- NSLink observado: `A:/nslink-devel/bin/release/nslink.exe`.

## Exclusiones y resultado

No se ejecutó baseline, certificación, promoción, instalación, despliegue, O1, O2 ni O3. No se
tocó `C:`. El grafo histórico no se borró físicamente porque contiene cobertura fuera del scope
O0; en el modo v2 queda excluido de configuración y ya no existe un ejecutable por prueba.
