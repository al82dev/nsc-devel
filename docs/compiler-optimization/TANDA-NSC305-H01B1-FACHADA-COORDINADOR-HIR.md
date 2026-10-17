# NSC 3.0.5 — H01B1: fachada del coordinador HIR

## SCOPE LOCK

| Dimensión | Alcance |
|---|---|
| Objetivo | Mover la fachada `build` y las cinco consultas públicas del modelo a `HirBuildCoordinator.cpp`. |
| Naturaleza | Movimiento puro; cero cambios de lógica, datos, firmas, orden o contratos. |
| Repositorio/rama | `A:\nsc-devel`, `development`. |
| Archivos autorizados | `ProjectOopHirModel.cpp`, `HirBuildCoordinator.cpp`, `CMakeLists.txt`, este expediente y tracking. |
| Símbolos | `build`, `functions`, `fieldInitializers`, `functionForMember`, `functionForNamespaceFunction`, `isBuilt`. |
| Protegido | `buildImpl`, lowering, HIR/MIR, sintaxis, semántica, ABI, NSMETA, diagnósticos, tests y consumidores. |
| Terminación | Una definición por símbolo, build de `nsc`, HIR 34/34 y namespace HIR 12/12. |

## Causa y frontera

La fachada y las consultas sólo delegan o leen almacenamiento ya construido. No consumen helpers anónimos ni
participan en lowering. `buildImpl` permanece en el monolito hasta H01B2 porque comparte `makeNode` con el resto
del lowerer; moverlo aquí sin aislar esa frontera dejaría de ser un movimiento puro.

## Preservación

- `build` conserva argumentos, defaults declarados en el header y delegación exacta a `buildImpl`.
- Las consultas conservan bounds, traducción de ordinals y resultados `nullptr`.
- No cambian vectores, índices ni estado `built_`.

## Resultado

- `git diff --check`: limpio.
- `nsc` construido; contrato de runtime estático aprobado.
- `nsc_hir_tests`: `PASS 34/34`, exit `0`.
- `nsc_namespace_function_hir_tests`: `PASS 12/12`, exit `0`.
- Cada símbolo movido conserva una sola definición.

H01B sigue abierto por H01B2 (`buildImpl` y sus helpers privados).
