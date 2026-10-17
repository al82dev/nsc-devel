# NSC 3.0.5 — H01A: extracción del índice de bindings HIR

## SCOPE LOCK

| Dimensión | Alcance |
|---|---|
| Objetivo | Mover los accesores privados que resuelven bindings semánticos por nodo AST desde el monolito `ProjectOopHirModel.cpp` hacia una unidad cohesionada. |
| Naturaleza | Refactor interno por movimiento puro; cero cambios de algoritmo, orden, semántica o contrato. |
| Repositorio y rama | `A:\nsc-devel`, `development`. |
| Archivos autorizados | `src/Frontend/ProjectOopHirModel.cpp`, `src/Frontend/HIR/HirBindingIndex.cpp`, `CMakeLists.txt`, este expediente y el tracking externo. |
| Símbolos autorizados | Los 22 accesores desde `nameBinding` hasta `localSymbol`, incluido `currentLambdaThisCapture`. |
| Protegido | Construcción del modelo, lowering de statements/expressions, HIR/MIR, ABI, NSMETA, diagnósticos, tests y todo producto distinto de `nsc`. |
| Perfiles | Movimiento independiente de O0/O1/O2/O3; validación focal con el target HIR existente. |
| Prohibido | Cambiar firmas, cuerpos, contenedores, orden de lookup, fallback `lookupSource_`, caché, sintaxis o fixtures. |
| Terminación | El build incorpora la nueva unidad, no quedan definiciones duplicadas, las pruebas HIR focales pasan y el diff demuestra identidad textual de los cuerpos movidos. |

## Hipótesis causal

`ProjectOopHirModel.cpp` mezcla lowering con 22 accesores de un índice semántico ya construido. Los
accesores no poseen estado propio ni dependen de helpers anónimos del monolito, por lo que forman una
frontera física independiente. Extraerlos reduce acoplamiento editorial sin cambiar ejecución.

## Preservación

- Mismas firmas privadas y mismo acceso a miembros de `ProjectOopHirModel`.
- Mismo selector `lookupSource_ == nullptr`.
- Mismos mapas, búsqueda y resultado `nullptr` en ausencia.
- Mismo recorrido de capturas para `currentLambdaThisCapture`.
- Ningún cambio en las rutas de lowering ni en representación HIR.

## Validación requerida

1. `git diff --check`.
2. Construcción focal del compilador/target HIR con el build existente en `A:`.
3. Pruebas HIR focales existentes sin modificar expectativas.
4. Auditoría del diff y commit local aislado.

## Resultado

- `git diff --check`: limpio.
- `nsc` construido desde `A:\nsc-devel\build-clang`; contrato de runtime estático aprobado.
- `nsc_hir_tests`: `PASS 34/34`, exit `0`.
- `nsc_namespace_function_hir_tests`: `PASS 12/12`, exit `0`.
- Cada símbolo movido conserva una única definición; no quedaron duplicados en el monolito.

H01 permanece activo: esta tanda cierra exclusivamente `HirBindingIndex.cpp`. La extracción completa
se conserva como H01A–H01J en el tracking y no se atribuye rendimiento a un movimiento estructural.
