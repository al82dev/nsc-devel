# NSC 3.0.5 — H02S2: walkers iterativos del coordinador HIR

## SCOPE LOCK

| Dimensión | Alcance |
|---|---|
| Objetivo | Eliminar la recursión de proceso de los cinco recorridos post-HIR del coordinador. |
| Algoritmo | DFS preorden con pilas explícitas; frame contextual para la prueba de escape. |
| Símbolos | `collectLocalDeclarations`, `stackClassLocalUsesAreSafe`, `firstUnpromotedObjectConstruction`, `scanParallelOrdinals` y `applyParallelOrdinalOffsets`. |
| Incluido | Orden de declaraciones, primer allocation, reglas padre/abuelo, máximos y rebasing de ordinales. |
| Excluido | Lowering de expresiones, aggregates, deconstruction, destrucción general, MIR y LLVM. |
| Terminación | Cinco rojos y verdes separados a 32,768 niveles, regresiones HIR y construcción de `nsc`. |

## Rojos reproducidos

Se construyó directamente un árbol HIR de 32,768 niveles y cada walker se ejecutó en un proceso
independiente. Antes del cambio los cinco focos terminaron con `-1073741571` (`0xC00000FD`):

- `hir-coordinator-walker-collect`;
- `hir-coordinator-walker-uses`;
- `hir-coordinator-walker-allocation`;
- `hir-coordinator-walker-scan`;
- `hir-coordinator-walker-apply`.

La separación por proceso demuestra que no era un único fallo que ocultaba a los demás: cada
recorrido agotaba por sí mismo la pila nativa.

## Implementación

- Los cinco helpers pasaron a ser estáticos privados de `ProjectOopHirModel`; el friend de pruebas
  los alcanza sin publicar API nueva.
- `collectLocalDeclarations` y `firstUnpromotedObjectConstruction` insertan hijos en orden inverso
  para conservar el preorden izquierda→derecha y, en el segundo caso, el primer resultado exacto.
- `stackClassLocalUsesAreSafe` conserva en cada frame nodo, padre, abuelo, índice del hijo e índice
  del padre. Por tanto siguen siendo válidas únicamente la copia directa de campo y la dirección de
  campo usada como destino directo de `Assignment` o `CompoundStore`.
- `scanParallelOrdinals` conserva los mismos pisos y máximos de local, cleanup y región.
- `applyParallelOrdinalOffsets` conserva los mismos filtros y offsets de local, cleanup, región y
  región de excepción.
- `firstUnpromotedObjectConstruction` dejó de ser `noexcept` porque su implementación iterativa
  reserva la pila explícita; así un fallo de reserva no fuerza `std::terminate`.
- La búsqueda mecánica deja una definición y un consumidor de producción para cada helper; no
  queda autorrecursión en estos cinco símbolos.

No se modificaron sintaxis, semántica, diagnósticos, API pública, ABI, NSMETA ni formatos.

## Evidencia focal

- Cinco focos profundos: todos rojos con `0xC00000FD` antes; todos verdes con exit `0` después.
- Conteos verdes: collect `PASS 1/1`, uses `PASS 3/3`, allocation `PASS 1/1`, scan `PASS 1/1`,
  apply `PASS 1/1`.
- Las pruebas adicionales preservan preorden de declaraciones, primer allocation, almacenamiento
  directo de campo permitido y dirección de campo escapada rechazada.
- `nsc_hir_tests`: `PASS 34/34`.
- `nsc_namespace_function_hir_tests`: `PASS 12/12`.
- Ejecución ordinaria de `nsc_project_oop_lowering_tests`: `134/136`; los dos únicos fallos
  requieren `nslink.exe`, ausente junto a `nsc.exe` y en `PATH`. No se buscó una ruta alterna ni se
  modificó `PATH`.
- `cmake --build A:/nsc-devel/build-clang --target nsc --parallel 16`: exit `0`.
- `nsc.exe static host-runtime contract`: `passed`.

H02 permanece abierto: aggregates, deconstruction y propiedad de destrucción HIR todavía deben
resolverse o clasificarse con evidencia. No se ejecutó baseline, deploy, instalación, batería
completa ni consumidor externo.
