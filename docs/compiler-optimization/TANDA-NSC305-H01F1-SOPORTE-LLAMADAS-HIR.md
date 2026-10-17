# NSC 3.0.5 — H01F1: soporte común de llamadas HIR

## SCOPE LOCK

| Dimensión | Alcance |
|---|---|
| Objetivo | Extraer a `HirCallLowering.cpp` el soporte único que materializa una llamada ya resuelta. |
| Naturaleza | Movimiento estructural; el adaptador local conserva la firma y valores capturados anteriores. |
| Símbolo | `ProjectOopHirModel::lowerBoundCall`. |
| Consumidores | Llamadas, getters, operadores, indexadores y conversiones definidas por el usuario. |
| Protegido | Resolución, receptor, dispatch, orden de operandos, conversiones, borrows, defaults, cleanup, HIR/MIR, ABI y NSMETA. |
| Fuera de alcance | Mover todavía el caso `CallExpression`; algoritmos iterativos; cambios de semántica o rendimiento. |
| Terminación | Definición única, `nsc` construido y focos HIR 34/34 y 12/12. |

## Auditoría causal

`lowerResolvedCall` no pertenecía exclusivamente al caso `CallExpression`: era una lambda local
capturada por todo `lowerExpressionImpl` y también daba servicio a accesores, operadores,
indexadores y conversiones. Mover sólo el bloque `CallExpression` habría obligado a duplicar esa
lógica o a dejar una dependencia oculta entre translation units.

H01F se dividió por ello en dos cierres verificables:

1. H01F1 mueve el cuerpo compartido a un método privado único.
2. H01F2 moverá el caso `CallExpression` y sus helpers exclusivos, usando ya esa frontera estable.

El adaptador local mantiene los mismos cuatro argumentos y pasa exactamente el `expression`,
`memberOrdinal`, `addressContext`, `error` y `typeBinding` que antes capturaba la lambda. El cuerpo movido conserva
las comprobaciones y el orden original: resolución de dispatch, receptor, operandos de izquierda a
derecha, conversiones, borrows, argumentos predeterminados, cleanup y carga de retornos por
referencia.

## Cambios

- Nuevo `src/Frontend/HIR/HirCallLowering.cpp` registrado en `nsc_frontend`.
- Nueva frontera privada `lowerBoundCall` en `ProjectOopHirModel`.
- `HirExpressionMachine.cpp` conserva un adaptador local mínimo para no alterar ninguno de sus
  consumidores durante esta primera mitad de H01F.
- No se cambió sintaxis, diagnóstico, API, ABI, formato, metadata ni contrato observable.

## Evidencia focal

- `git diff --check`: limpio.
- `cmake --build A:/nsc-devel/build-clang --target nsc`: exit `0`.
- Contrato de runtime estático de `nsc.exe`: aprobado.
- `nsc_hir_tests`: `PASS 34/34`, exit `0`.
- `nsc_namespace_function_hir_tests`: `PASS 12/12`, exit `0`.

H01F permanece abierto hasta H01F2. Esta tanda no afirma validación integral ni ejecuta baseline,
deploy o batería completa.
