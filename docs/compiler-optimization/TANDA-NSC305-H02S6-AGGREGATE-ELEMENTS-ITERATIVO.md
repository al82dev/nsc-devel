# NSC 3.0.5 — H02S6: lowering iterativo de elementos agregados

## SCOPE LOCK

| Dimensión | Alcance |
|---|---|
| Objetivo | Eliminar la autorrecursión directa de `appendAggregateElements`. |
| Algoritmo | Máquina DFS con frames reanudables por initializer, slots y entry index. |
| Símbolos | `ProjectOopHirModel::appendAggregateElements` y acceso focal de prueba. |
| Incluido | Orden fuente, spreads, offsets, conversiones, bitfields, volatile y primer fallo. |
| Excluido | SCC OOP restante, destrucción general, HIR base, MIR, Driver y Parser. |
| Terminación | Rojo/verde profundo, paridad HIR focal, build de `nsc`, análisis y tracking. |

## Cadena causal previa al cambio

Un initializer agregado anidado vuelve a invocar `appendAggregateElements` antes de reanudar la
entrada siguiente del padre. La profundidad del AST, de sus bindings y de sus layouts es finita y
válida, pero cada nivel añade un frame del proceso. La hipótesis causal se probará invocando
directamente el símbolo productivo sobre una cadena semántica en memoria; sólo la hoja final será
bajada como expresión.

## Presupuesto de cambio

- Repositorio: sólo `A:\nsc-devel`.
- Producción: `src/Frontend/HIR/HirAggregateLowering.cpp`.
- Costura focal: `ProjectMethodBodyModel.h` y `ProjectOopLoweringTests.cpp`.
- Contratos protegidos: sintaxis, semántica, orden, errores, HIR observable, API, ABI y NSMETA.
- Validación: sólo build y pruebas focales O0; sin baseline, batería completa, deploy o instalación.

## Evidencia

### Contrafactual rojo

Con la implementación recursiva intacta, el foco `hir-aggregate-elements-depth` construyó en
memoria una cadena válida de 4,096 initializers, bindings semánticos y layouts struct. El proceso
terminó con `-1073741571` (`0xC00000FD`) antes de alcanzar la hoja literal. No intervienen parser,
análisis de cuerpos, MIR, LLVM ni enlace.

### Implementación

- Cada initializer usa un frame explícito con `syntax`, tipo destino, offset base, slots, binding,
  índice de entrada y estado de inicialización.
- El frame padre incrementa su índice antes de apilar un initializer hijo. Al terminar el hijo,
  reanuda exactamente la siguiente entrada del padre.
- Slots tuple, array fijo y struct se materializan al entrar por primera vez al frame, igual que en
  la ruta anterior; no se adelanta la evaluación de hermanos.
- `with`, spreads, conversiones implícitas, bitfields, acceso volatile, offsets y provenance usan
  las mismas condiciones, datos y textos de error.
- El primer fallo retorna inmediatamente y conserva el mismo prefijo de elementos ya emitidos.

La costura focal sólo concede amistad al acceso de prueba existente. No añade API pública,
almacenamiento productivo, símbolos exportados, ABI ni metadata.

### Resultado verde y paridad

- `hir-aggregate-elements-depth`: `PASS 3/3`, exit `0`.
- El foco comprueba 4,096 niveles, hoja literal, tipo, provenance y offset absoluto acumulado.
- La unidad OOP general ejecutó 134/136 comprobaciones correctamente. Sus dos únicos fallos son
  de infraestructura de enlace: `nslink.exe` no está junto a `nsc.exe` ni en el `PATH`. No se
  buscó fuera de A:, no se alteró el entorno y esos dos casos no se declaran aprobados.
- `nsc_hir_tests`: `PASS 34/34`.
- `nsc_namespace_function_hir_tests`: `PASS 12/12`.
- El target focal y `nsc` compilaron con `/W4 /WX`; cero diagnósticos nuevos.
- El contrato de runtime estático de `nsc.exe` pasó.
- `git diff --check` se ejecuta como gate final antes del commit.

No se ejecutaron baseline, batería completa, deploy, instalación ni consumidores externos.

## Residuo H02 confirmado

Este corte no cierra H02. Permanecen:

- destrucción recursiva de temporales y resultados paralelos `ProjectOopHirNode`;
- el SCC OOP que conecta expression, statement/block, calls, construction, conversion, aggregate,
  await, destruction, exceptions y coroutines;
- recursión productiva en `hir::HirBuilder`, `hir::HirVerifier` y `hir::HirDumper`.

La siguiente acción causal es asegurar destrucción iterativa de todo ownership HIR temporal antes
de abrir las familias del SCC OOP. MIR, Driver y Parser siguen esperando a que H02 deje una frontera
estable y auditada.
