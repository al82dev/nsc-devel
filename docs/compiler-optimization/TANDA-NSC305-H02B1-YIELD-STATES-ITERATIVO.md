# NSC 3.0.5 — H02B1: numeración iterativa de yield states

## SCOPE LOCK

| Dimensión | Alcance |
|---|---|
| Objetivo | Eliminar los dos lambdas recursivos duplicados que numeraban `YieldValue`. |
| Algoritmo | Worklist LIFO; hijos empujados en orden inverso para preservar preorden izquierda-derecha. |
| Símbolo | `assignYieldStates`. |
| Protegido | Estados 1..N, orden de source/HIR y uso compartido por generadores sync/async. |
| Fuera de alcance | Reescritura de returns/throws, análisis transitivo de await y máquina general HIR. |
| Terminación | Cero lambdas duplicados, profundidad 32,768, pruebas de ambos consumidores y `nsc`. |

## Resultado

- Los generadores síncronos y asíncronos usan un único helper privado.
- El helper visita el nodo antes de sus hijos y empuja los hijos de derecha a izquierda; el orden
  observable es idéntico al `for` recursivo anterior de izquierda a derecha.
- `nextYieldState` sigue siendo propiedad del llamador y conserva el rango 1..N.
- No se cambió sintaxis, API, ABI, metadata, layouts de frame ni contrato de runtime.

## Contrafactual profundo

La prueba construye 32,768 bloques anidados, cada uno con un `YieldValue`, ejecuta el helper y
comprueba:

- que se alcanzaron los 32,768 yields;
- que sus estados son exactamente 1..32,768 en preorden;
- que la prueba desmonta el árbol iterativamente para no introducir una recursión de destructor.

## Evidencia focal

- Foco `NSC_PROJECT_OOP_LOWERING_FOCUS=hir-async-yield-state-depth`: `PASS 2/2`.
- `nsc_ns3_t09_sync_generator_tests`: `PASS 10/10`.
- `nsc_ns3_t10_async_generator_tests`: `PASS 9/9`.
- `nsc_hir_tests`: `PASS 34/34`.
- `nsc_namespace_function_hir_tests`: `PASS 12/12`.
- `cmake --build A:/nsc-devel/build-clang --target nsc`: exit `0`.
- Contrato de runtime estático de `nsc.exe`: aprobado.
- `git diff --check`: limpio.

H02 permanece abierto. No se ejecutó baseline, deploy ni batería completa.
