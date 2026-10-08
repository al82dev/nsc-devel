# Tanda 16 — Self-hosting y flujos especializados

## Alcance e inventario

La matriz canónica contiene 69 verificadores `KEEP_SPECIALIZED`. Cada propietario histórico se
conserva; la tanda añade un `SuiteAdapter` que rebasa rutas mutables y ejecuta un propietario por
proceso, en orden determinista.

| Clasificación | Flujos |
|---|---:|
| Ejecutables de forma aislada en O0 | 28 |
| Propietario CMake con ruta O1/O2/O3 | 34 |
| Delegación al árbol CTest histórico | 3 |
| Operación prohibida de distribución/certificación/promoción | 3 |
| Matriz de rendimiento O0/O1/O2/O3 | 1 |
| Total | 69 |

Los 41 flujos bloqueados conservan ID, propietario, comando y razón. No se ejecutaron como cierre
positivo ni se debilitó su contrato.

## Implementación

- `SuiteCatalog` carga los 69 IDs desde la matriz canónica y rechaza duplicados o propietarios
  ausentes.
- `SuiteAdapter` conserva cada comando original y reemplaza únicamente:
  - `NSC_EXECUTABLE` por el `nsc.exe` construido y seleccionado;
  - `NSLINK_EXECUTABLE` por el NSLink explícito de A:;
  - raíz del repositorio, build y directorios de evidencia;
  - Python, CMake y CTest por la herramienta resuelta para la corrida.
- `NS_ROOT` y `NS_LIB_ROOT` apuntan al prefijo A: entregado explícitamente; nunca seleccionan el
  `nsc.exe` de ese prefijo.
- `NSC_SELFHOST_PROFILES=O0` congela la única optimización permitida.
- La secuencia SH41, SH42, SH43, SH44, SH45, SH46, SH48, SH49, SH51 y SH52 se modela de forma
  iterativa y estable. Los consumidores comparten la evidencia del productor SH41.
- `--suite-evidence-root` permite repetir un consumidor sin regenerar su productor.
- `run_nsc_tests.py` puede listar, seleccionar por ID, seleccionar por componente o ejecutar todos
  los flujos especializados autorizados.

## Frontera del corpus self-host

La materialización de `tests/cases` amplió accidentalmente los recorridos recursivos históricos de
frontend, semántica e IR. Ese árbol nuevo contiene fixtures del reemplazo y no pertenecía al corpus
congelado. Los tres selectores omiten exclusivamente `tests/cases/`; todas las demás raíces,
entradas y expectativas históricas permanecen intactas.

La prueba `test_selfhost_corpus_boundary.py` demuestra que un `.ns` histórico continúa seleccionado
y que un `.ns` del nuevo harness no contamina ninguno de los tres oráculos.

## Validación

| Evidencia | Resultado |
|---|---:|
| Unit tests del harness | 21/21 |
| Flujos aislables aprobados en la primera corrida válida | 27/28 |
| SH42 después de restaurar su frontera histórica | 1/1 |
| Total especializado O0 autorizado | 28/28 |

La primera corrida completa consumió 1179.790 s de procesos especializados. SH42 se repitió de
forma aislada, reutilizando SH41, y pasó en 620.937 s. La evidencia confirmó que SH41 produjo sólo
`nsc2-o0-nslink.exe` y `nsc3-o0-nslink.exe` en la corrida válida.

## Incidencia de aseguramiento

Una ejecución preliminar no fijó `NSC_SELFHOST_PROFILES`; el verificador transitivo SH41 alcanzó O1
antes de que el filtro del propietario directo pudiera observarlo. La corrida fue cancelada, los
procesos se comprobaron terminados y su evidencia parcial se eliminó. La corrección congela O0 en el
entorno del adaptador y existe una aserción unitaria sobre esa variable. Esa ejecución preliminar no
se contabiliza como validación.

## Exclusiones explícitas

- No se ejecutó baseline, instalación, despliegue, promoción ni certificación.
- No se modificó código de producción del compilador.
- No se modificó `nscbuilder.py` ni `_nscbuilder`.
- Los verificadores que delegan a CTest quedan bloqueados hasta que la tanda 18 tenga un árbol nuevo
  completo y aislado.
