# NSC 3.0.5 — M02U: cost explorer MIR iterativo

## SCOPE LOCK

| Dimensión | Alcance |
|---|---|
| Objetivo | Eliminar la recursión de proceso al acumular costos sobre árboles `ProjectOopMirNode`. |
| Algoritmo | DFS preorden con worklist LIFO y alta inversa de hijos no nulos. |
| Símbolos | `visit` y `accumulateNodeCost` en `ProjectCostExplorer.cpp`. |
| Incluido | Contadores, causas, orden de visita, omisión de slots nulos y suma por función/proyecto. |
| Excluido | Formatos NSCOST, clasificación de costos, MIR, lowering, ABI, NSMETA y API pública. |

## Contrato preservado

El recorrido anterior acumulaba el nodo actual y descendía de izquierda a derecha sólo a hijos no
nulos. La worklist nueva ejecuta `accumulateNodeCost` antes de insertar descendientes y apila los
hijos en orden inverso; por ello conserva exactamente el mismo preorden. Los slots nulos siguen sin
producir costos ni causas. No cambia ningún caso del `switch`, la precedencia entre clasificación
principal, atomics y locks, ni el orden previo al ordenamiento determinista de `causes`.

La extracción separa clasificación y recorrido sin introducir un tipo productivo nuevo. Ambos
helpers quedan por debajo de 60 líneas ejecutables; `visit` no conserva self-call. La complejidad
continúa siendo `O(n)` en tiempo y pasa de `O(h)` stack de proceso a `O(h)` memoria explícita.

## Contrafactual profundo

`NativeSharp3P08P10ExplainTests.cpp` construye una cadena de 32,768 nodos `MoveValue`. Cada nivel
también contiene un slot nulo para probar la omisión histórica. La aserción exige 32,768 sitios,
32,768 causas y el mismo total de proyecto. El árbol se destruye mediante una utilidad iterativa de
prueba para no adjudicar a M02U la futura corrección M02V de ownership profundo.

No se crea fuente, target ni CTest nuevos. La prueba permanece en el target existente
`nsc_ns3_p08_p10_explain_tests`, registrado como `nsc.ns3.p08-p10.explain`. Los gates reales
aplicables son `TL-LANG-499` y `TL-LANG-518`, además de `validate_source_registration.py`.

## Estado de validación

Implementación, auditoría estática y gates focales cerrados:

- `nsc` y `nsc_ns3_p08_p10_explain_tests` construidos; contrato estático host/runtime PASS
  (`A:\temp\m02u-build-nsc-p08p10.log`).
- CTest `nsc.ns3.p08-p10.explain` PASS 1/1 en 0.12 s
  (`A:\temp\m02u-ctest-p08p10.log`).
- `nsc_component_tests` construido y `TL-LANG-499`/`TL-LANG-518` PASS individualmente, con
  `nslink.exe` resuelto por nombre a `A:\nslink-devel\bin\release\nslink.exe`
  (`A:\temp\m02u-build-component-runner.log`, `A:\temp\m02u-tl-lang-499.log`,
  `A:\temp\m02u-tl-lang-518.log`).
- `validate_source_registration.py` PASS (`A:\temp\m02u-source-registration.log`).

La auditoría confirma cero self-call en `visit`, alta inversa de hijos, filtro explícito de slots
nulos y helpers con estimados de 38 y 7 líneas ejecutables. No se ejecutó baseline, deploy,
instalación ni batería completa.
