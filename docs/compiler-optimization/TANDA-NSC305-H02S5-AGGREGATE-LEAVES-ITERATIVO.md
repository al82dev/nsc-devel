# NSC 3.0.5 — H02S5: recorrido iterativo de hojas agregadas

## SCOPE LOCK

| Dimensión | Alcance |
|---|---|
| Objetivo | Eliminar la autorrecursión de `appendAggregateLeaves`. |
| Algoritmo | DFS explícito con frames `{typeKey, baseOffset}` y push inverso. |
| Símbolos | `ProjectOopHirModel::appendAggregateLeaves` y acceso focal de prueba. |
| Incluido | Arrays fijos, structs, offsets absolutos, orden izquierda→derecha y primer fallo. |
| Excluido | `appendAggregateElements`, bindings, layouts, diagnósticos, sintaxis, MIR o LLVM. |
| Terminación | Rojo/verde profundo, paridad HIR focal, build de `nsc`, análisis y tracking. |

## Cadena causal previa al cambio

La ruta de síntesis de igualdad y hash de agregados aplana recursivamente arrays fijos y structs.
Cada agregado anidado vuelve a entrar en `appendAggregateLeaves`, por lo que una forma válida de
profundidad suficiente consume el stack del proceso antes de emitir la hoja escalar final.

La prueba contrafactual construye exclusivamente en memoria una cadena finita de structs con un
campo por nivel y offsets explícitos. El foco invoca directamente el símbolo productivo; no usa
parser, análisis de cuerpos, MIR, LLVM ni enlace. Antes de modificar el algoritmo se registrará la
menor profundidad estable que termine con `0xC00000FD`.

## Presupuesto de cambio

- Repositorio: sólo `A:\nsc-devel`.
- Producción: `src/Frontend/HIR/HirAggregateLowering.cpp`.
- Costura focal: `ProjectNameLookup.h`, `ProjectObjectLayoutModel.h` y
  `ProjectOopLoweringTests.cpp`.
- Contratos protegidos: sintaxis, semántica, diagnósticos, HIR observable, API, ABI y NSMETA.
- Validación: sólo build y pruebas focales O0; sin baseline, batería completa, deploy o instalación.

## Evidencia

### Contrafactual rojo

Con la implementación recursiva intacta, el foco `hir-aggregate-leaves-depth` recorrió una cadena
válida de 4,096 structs y terminó con `-1073741571` (`0xC00000FD`). La profundidad se congeló ahí:
es la primera probada y ya reproduce establemente el defecto sin pagar búsquedas lineales inútiles
en una cadena mayor.

### Implementación

- La llamada recursiva fue sustituida por una pila local de pares `{typeKey, baseOffset}`.
- Arrays fijos y campos struct se insertan en orden inverso para que el pop LIFO conserve el DFS
  izquierda→derecha de la implementación anterior.
- Las consultas de count, stride, tipo ordinal y layout ocurren cuando el frame llega a su turno;
  no se adelanta trabajo de hermanos.
- Se conservaron literalmente las condiciones y textos de los tres errores existentes.
- La hoja conserva tipo, nodo de sintaxis y offset absoluto; no cambió el formato HIR.

La costura de prueba sólo concede amistad al `ProjectOopHirModelTestAccess` ya existente. No añade
API pública, almacenamiento, símbolos exportados, ABI ni metadata.

### Resultado verde y paridad

- `hir-aggregate-leaves-depth`: `PASS 5/5`, exit `0`.
- El foco comprueba la cadena struct de 4,096 niveles, la hoja escalar final, provenance y offset.
- El mismo foco comprueba tres elementos de array fijo en offsets ascendentes por stride.
- El mismo foco comprueba dos campos struct en orden declarado y con offsets absolutos distintos.
- `nsc_hir_tests`: `PASS 34/34`.
- `nsc_namespace_function_hir_tests`: `PASS 12/12`.
- El target focal y `nsc` compilaron con `/W4 /WX`; cero diagnósticos nuevos.
- El contrato de runtime estático de `nsc.exe` pasó.
- `git diff --check` se ejecuta como gate final antes del commit.

No se ejecutaron baseline, batería completa, deploy, instalación ni consumidores externos.

## Residuo H02 confirmado

Este corte no cierra H02. La auditoría posterior a H02S4 confirmó todavía:

- autorrecursión de `appendAggregateElements`;
- el SCC OOP que conecta expression, statement/block, calls, construction, conversion, aggregate,
  await, destruction, exceptions y coroutines;
- destrucción recursiva de temporales `ProjectOopHirNode` en rutas de error y resultados paralelos;
- recursión productiva en `hir::HirBuilder`, `hir::HirVerifier` y `hir::HirDumper`.

La siguiente acción causal es aislar y convertir `appendAggregateElements`; no se adelantan MIR,
Driver ni Parser mientras la frontera HIR permanezca móvil.
