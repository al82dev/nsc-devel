# NSC 3.0.5 — H01B2B: `buildImpl` en el coordinador HIR

## SCOPE LOCK

| Dimensión | Alcance |
|---|---|
| Objetivo | Trasladar `buildImpl` y su coordinación a `HIR/HirBuildCoordinator.cpp` sin copiar ni reescribir el cuerpo. |
| Naturaleza | Movimiento físico puro del translation unit; sólo cambia la ruta del archivo y una inclusión privada relativa. |
| Repositorio/rama | `A:\nsc-devel`, `development`. |
| Archivos autorizados | `ProjectOopHirModel.cpp`, `HIR/HirBuildCoordinator.cpp`, `HIR/HirModelFacade.cpp`, `CMakeLists.txt`, expediente y tracking. |
| Protegido | Cuerpos de `buildImpl` y lowering, orden, paralelismo, ordinals, merge, diagnósticos, HIR/MIR, sintaxis, semántica, ABI y NSMETA. |
| Terminación | La ruta antigua desaparece, `buildImpl` tiene una definición en el coordinador, `nsc` construye y los dos focos HIR pasan. |

## Operación

1. La fachada extraída en H01B1 se renombró a `HirModelFacade.cpp`.
2. El translation unit original se movió íntegro a `HIR/HirBuildCoordinator.cpp`.
3. La única edición dentro del archivo movido adapta la inclusión privada de
   `HIR/HirLoweringSupport.h` a `HirLoweringSupport.h` por el nuevo directorio.
4. CMake consume las dos rutas nuevas y deja de consumir `ProjectOopHirModel.cpp`.

El coordinador conserva temporalmente lowering ajeno a coordinación. Esto no cierra H01: H01C–H01J
retirarán esas responsabilidades, conservando el cuerpo de `buildImpl` en su dueño definitivo.

## Resultado

- `ProjectOopHirModel.cpp`: ya no existe.
- `buildImpl`: una definición en `HirBuildCoordinator.cpp`.
- `nsc` construido; contrato de runtime estático aprobado.
- `nsc_hir_tests`: `PASS 34/34`, exit `0`.
- `nsc_namespace_function_hir_tests`: `PASS 12/12`, exit `0`.
- `git diff --check`: limpio.

H01B queda cerrado; H01 general permanece abierto por H01C–H01J.
