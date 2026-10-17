# NSC 3.0.5 — H02B2: reescritura iterativa de throw en generators

## SCOPE LOCK

| Dimensión | Alcance |
|---|---|
| Objetivo | Eliminar el lambda recursivo que convierte `Throw` en error terminal de generator. |
| Algoritmo | Worklist de slots `unique_ptr`; recorrido DFS en preorden con hijos invertidos. |
| Símbolo | `rewriteGeneratorThrows`. |
| Protegido | Error field 5, state -2, payload, `return false`, tipos y orden del bloque sintetizado. |
| Fuera de alcance | Async poll returns, await transitivo y máquina general HIR. |
| Terminación | Profundidad 32,768, generator focal, HIR y `nsc`. |

## Resultado

- El lambda autorrecursivo desapareció de `splitGeneratorFunction`.
- Cada slot `Throw` se reemplaza en sitio por el mismo bloque de tres operaciones: almacenar error,
  almacenar estado -2 y devolver false.
- El payload se mueve una sola vez y un throw sin payload conserva su `NullConstant` tipado.
- No se modificaron sintaxis, ABI, metadata, layout del iterator ni contrato observable.

## Evidencia focal

- Foco `NSC_PROJECT_OOP_LOWERING_FOCUS=hir-generator-throw-depth`: `PASS 2/2`; reemplazo a 32,768 niveles y todos los bloques exteriores conservados.
- `nsc_ns3_t09_sync_generator_tests`: `PASS 10/10`.
- `nsc_hir_tests`: `PASS 34/34`.
- `nsc_namespace_function_hir_tests`: `PASS 12/12`.
- `cmake --build A:/nsc-devel/build-clang --target nsc`: exit `0`.
- Contrato de runtime estático de `nsc.exe`: aprobado.
- `git diff --check`: limpio.

H02 permanece abierto. No se ejecutó baseline, deploy ni batería completa.
