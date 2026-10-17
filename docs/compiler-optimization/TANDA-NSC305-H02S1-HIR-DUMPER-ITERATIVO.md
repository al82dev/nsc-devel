# NSC 3.0.5 — H02S1: dumper HIR iterativo

## SCOPE LOCK

| Dimensión | Alcance |
|---|---|
| Objetivo | Eliminar la recursión de proceso al serializar árboles HIR. |
| Algoritmo | DFS preorden mediante pila explícita, con hijos insertados en orden inverso. |
| Símbolos | `dumpNode`, nuevo emisor local `dumpNodeLine` y contrafactual `hir-dump-depth`. |
| Incluido | Nodos HIR, hijos nulos, indentación, atributos, orden de hijos y saltos de línea existentes. |
| Excluido | Cambiar el formato HIR, el modelo, el lowering, API, ABI, NSMETA o destrucción general de árboles. |
| Terminación | Rojo y verde a 8,192 niveles, regresiones HIR, construcción de `nsc` y contrato estático. |

## Rojo reproducido

El foco `NSC_PROJECT_OOP_LOWERING_FOCUS=hir-dump-depth` construye un árbol HIR de bloques con
8,192 aristas y solicita su volcado. Antes del cambio terminó con `-1073741571`
(`0xC00000FD`, agotamiento de pila) sin producir un resultado.

La profundidad se mantuvo deliberadamente en 8,192 porque la indentación histórica hace que el
tamaño del texto crezca cuadráticamente con la profundidad. Ese tamaño basta para reproducir el
defecto y evita convertir la prueba en una carga ordinaria desproporcionada. El caso sólo se
ejecuta bajo su foco explícito.

## Implementación

- `dumpNodeLine` conserva sin reordenar el emisor de una sola línea y todos sus atributos.
- `dumpNode` usa frames `{node, depth}` en una pila local.
- Cada nodo se emite al retirarlo de la pila, igual que en el preorden anterior.
- Los hijos se insertan de derecha a izquierda para retirarlos de izquierda a derecha.
- Un hijo nulo conserva su línea `<none>` y no genera descendencia.
- No se cambió ningún texto, separador, sangría, condición, orden de campos ni salto de línea.

No se modificaron sintaxis, semántica, diagnósticos, API, ABI, NSMETA ni contratos externos.

## Evidencia focal

- Profundidad 8,192: rojo previo `0xC00000FD`; después `PASS 2/2` y exit `0`.
- La prueba verifica número total de líneas, encabezado, contrato de función y pie del documento.
- `nsc_hir_tests`: `PASS 34/34`.
- `nsc_namespace_function_hir_tests`: `PASS 12/12`.
- Ejecución ordinaria de `nsc_project_oop_lowering_tests`: `134/136`; los dos únicos fallos
  requieren `nslink.exe`, que no estaba junto a `nsc.exe` ni disponible en el `PATH`. No se buscó
  una ruta alterna ni se modificó `PATH`.
- `cmake --build A:/nsc-devel/build-clang --target nsc --parallel 16`: exit `0`.
- `nsc.exe static host-runtime contract`: `passed`.

H02 permanece abierto. No se ejecutó baseline, deploy, instalación, batería completa ni consumidor
externo.
