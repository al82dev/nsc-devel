# NSC 3.0.5 — H02S12H: raw/flexible/VLA index HIR iterativo

## Clasificación

`IMPLEMENTADO; VALIDACIÓN DE BUILD Y PRUEBAS DIFERIDA A LA TANDA INTEGRAL`.

## SCOPE LOCK

| Dimensión | Alcance |
|---|---|
| Objetivo | Eliminar la reentrada recursiva del lowering de base e índices `RawElement`. |
| Naturaleza | Refactor interno no funcional del lowering AST→HIR. |
| Productor | `ProjectOopHirModel::lowerExpression`. |
| Archivo funcional | `src/Frontend/HIR/HirExpressionMachine.cpp`. |
| Incluido | Raw pointers, fixed arrays, flexible arrays y VLA multidimensionales. |
| Excluido | Arrays nativos, strings, range slices, indexadores de usuario, Sema, MIR y LLVM. |
| Contratos congelados | Orden de evaluación, tipos, diagnósticos, address context, HIR, ABI y NSMETA. |
| Archivos protegidos | Pruebas y cambios concurrentes ajenos, en particular `ProjectMethodBodyModelTests.cpp`. |
| Ejecución | Sin build ni pruebas por instrucción expresa; sólo auditoría estática. |

## Cadena causal

La ruta histórica de `RawElement` bajaba la base mediante `lowerExpression` y después invocaba
`lowerExpression` para cada índice. Una cadena profunda de accesos raw podía reentrar en la misma
función una vez por nivel y consumir el stack nativo.

El cambio añade el frame explícito `RawIndex` a la máquina ya existente. El frame conserva:

- la sintaxis y el contexto address/value del acceso;
- el binding core y, cuando existe, el binding de índice VLA;
- la clasificación fixed/flexible de la base;
- las sintaxis de índices en orden fuente;
- la base y cada HIR de índice ya bajado.

La máquina baja primero la base con el mismo contexto anterior: dirección para fixed/flexible y
valor para raw pointer. Después baja los índices de izquierda a derecha y reconstruye el mismo HIR
cuando todos los operandos están disponibles.

## Preservación por variante

### VLA multidimensional

- Conserva la validación previa de rank; los ranks inválidos permanecen en la ruta histórica para
  emitir `multidimensional VLA index lost its semantic rank` en el mismo punto contractual.
- Convierte cada índice y extent a `builtin:nuint` sólo cuando no eran ya de ese tipo.
- Conserva la linealización row-major `linear * extent[dimension] + index`.
- Conserva `RawElementAddress` y el `PointerLoad` únicamente en contexto de valor.

### Fixed array y raw pointer

- Conserva la derivación gradual del tipo de elemento por dimensión.
- Conserva un `RawElementAddress` por índice y el anidamiento base→índice.
- Conserva la diferencia esencial: fixed array baja la base como dirección; raw pointer como valor.
- Conserva el resultado address o el `PointerLoad` según el contexto del consumidor.

### Flexible array

- Conserva la resolución del atributo `TrailingStorage`.
- Conserva el lookup del miembro de longitud y de ambos layouts.
- Conserva `hasTrailingLengthBounds` y `trailingLengthRelativeOffset` sin alterar su cálculo.

## Casos deliberadamente conservados en la ruta histórica

El classifier no activa el frame cuando falta la base, no existen índices, algún índice es nulo o
el rank VLA no coincide. Esas entradas siguen usando el lowering anterior para conservar exactamente
sus errores, rango diagnóstico y precedencia de validación. En particular, no se reemplazó el error
`fixed-array/raw-pointer index expression has no index`.

## Auditoría estática

- La arista profunda `RawElement → lowerExpression(base/indices)` queda reemplazada por frames.
- La evaluación permanece base→índices y cada sintaxis se baja una sola vez.
- El binding VLA y sus extent ordinals se consumen en el mismo orden.
- Las conversiones, operaciones `Star`/`Plus`, tipos y clases de nodo coinciden con la ruta anterior.
- No se modificaron parser, Sema, MIR, LLVM, CLI, diagnósticos públicos, ABI, NSMETA ni formatos.
- El cambio ajeno en `tests/unit/ProjectMethodBodyModelTests.cpp` no forma parte de esta tanda.

## Validación diferida

No se compiló ni se ejecutaron pruebas. La orden vigente exige completar primero todo el barrido
estático e implementar todas las hojas pendientes. El build de `nsc`, los focos HIR profundos y la
regresión integral quedan reservados para la tanda final que ejecutará el usuario desde la GUI.
