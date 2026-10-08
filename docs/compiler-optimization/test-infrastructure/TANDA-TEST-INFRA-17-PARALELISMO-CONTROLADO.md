# Tanda 17 — Paralelismo controlado

## Implementación

`CaseScheduler` divide la selección, de forma iterativa, en lotes paralelizables separados por
barreras exclusivas:

1. los casos ordinarios se acumulan en un lote;
2. el lote se ejecuta con un máximo de `--jobs N` procesos;
3. antes de un caso exclusivo se espera el lote completo;
4. el caso exclusivo corre solo;
5. los resultados se reconstruyen en el orden determinista del catálogo.

El algoritmo es O(n) en planificación, O(n) en resultados y usa como máximo `min(N, lote)` workers.
No contiene recursión. Los IDs duplicados se rechazan antes de iniciar procesos.

## Fronteras de aislamiento

- Cada fixture directo conserva su `CaseWorkspace` independiente.
- La etiqueta de manifiesto `exclusive` crea una barrera.
- Todos los `SuiteAdapter` son exclusivos porque pueden compartir generaciones self-host.
- `--jobs 1` conserva la ejecución estrictamente secuencial.
- stdout, stderr, código de salida, timeout y fallos se almacenan por caso; el orden de terminación
  de procesos no altera el orden del reporte.

## Validación unitaria

La batería del harness pasó 24/24. Las nuevas pruebas demuestran:

- paralelismo real mayor que uno dentro de un lote;
- observación de actividad igual a uno dentro de la barrera exclusiva;
- orden estable de resultados;
- secuencia estricta con `jobs=1`;
- rechazo de IDs duplicados.

## Comparación funcional

Se ejecutó la categoría `diagnostics` completa con el mismo NSC construido, el mismo NSLink y O0:

| Modo | Casos | Resultado |
|---|---:|---:|
| `--jobs 1` | 6 | 6/6 |
| `--jobs 4` | 6 | 6/6 |

Después de sustituir únicamente la raíz aleatoria de cada workspace y el nombre temporal interno de
enlace por marcadores, los IDs, componentes, comandos observables, códigos, stdout, stderr, timeout,
estado y fallos fueron idénticos. El directorio de trabajo quedó con cero residuos en ambos modos.

## Exclusiones

No se ejecutó baseline, certificación, promoción, instalación, O1, O2 ni O3. No se modificó el
compilador ni la infraestructura histórica de NSC Builder.
