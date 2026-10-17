# NSC 3.0.5 — H02S7: destrucción iterativa de todo nodo HIR

## SCOPE LOCK

| Dimensión | Alcance |
|---|---|
| Objetivo | Impedir destrucción recursiva al liberar cualquier `ProjectOopHirNode`. |
| Algoritmo | Drenado iterativo de hijos antes de destruir cada nodo. |
| Símbolos | Destructor de `ProjectOopHirNode` y contrafactual focal directo. |
| Incluido | Temporales, rutas de error, resultados paralelos y roots del modelo. |
| Excluido | Lowering, forma HIR, SCC OOP, HIR base, MIR, Driver y Parser. |
| Terminación | Rojo/verde profundo, regresiones HIR, build de `nsc`, análisis y tracking. |

## Cadena causal previa al cambio

H02S3 protege los roots que ya pertenecen al modelo, pero un `unique_ptr<ProjectOopHirNode>` local
todavía usa la destrucción implícita de `children`. Una ruta de error puede descartar un temporal
profundo antes de anexarlo al modelo; asimismo, un retorno temprano del coordinador paralelo puede
destruir resultados posteriores todavía contenidos en su vector local. En ambos casos la pila del
proceso vuelve a crecer una llamada de destructor por nivel.

## Presupuesto de cambio

- Repositorio: sólo `A:\nsc-devel`.
- Producción: declaración/definición del destructor de `ProjectOopHirNode`.
- Prueba: `ProjectOopLoweringTests.cpp`.
- Contratos protegidos: forma y ownership HIR, sintaxis, semántica, errores, API, ABI y NSMETA.
- Validación: sólo build y pruebas focales O0; sin baseline, batería completa, deploy o instalación.

## Evidencia

### Contrafactual rojo

El foco `hir-node-destruction-depth` construyó un `unique_ptr<ProjectOopHirNode>` local con 32,768
niveles y ejecutó `reset()` sin incorporarlo al modelo. Con la destrucción implícita original, el
proceso terminó con `-1073741571` (`0xC00000FD`) antes de registrar la comprobación.

### Implementación

- `ProjectOopHirNode` tiene un destructor productivo único que intercambia `children` con una pila
  local y vacía cada descendiente antes de destruirlo.
- Cada hijo no nulo se mueve a la pila; los huecos nulos dejados por payloads ya transferidos se
  omiten explícitamente.
- El helper previo de H02S3 ahora delega en `reset()`: la garantía pertenece al nodo y cubre también
  temporales, descartes por error y resultados paralelos que nunca llegaron al modelo.
- No cambiaron campos, orden, forma HIR, ownership observable ni decisiones de lowering.

Durante la primera regresión general apareció `0xC0000005` al reemplazar un `Throw`. La causa exacta
fue un hijo nulo conservado en el vector después de mover su payload; el destructor lo había contado
como entrada pendiente y lo dereferenciaba. Se añadió el guard nulo, se retiró toda instrumentación
temporal y el foco generator-throw volvió a pasar.

### Resultado verde y paridad

- `hir-node-destruction-depth`: `PASS 1/1`, exit `0`.
- `hir-generator-throw-depth`: `PASS 2/2`, exit `0`.
- `hir-model-destruction-depth`: `PASS 1/1`, exit `0`.
- `hir-aggregate-elements-depth`: `PASS 3/3`, exit `0`.
- La unidad OOP general volvió a su resultado previo: 134/136. Los dos únicos fallos son enlaces
  porque `nslink.exe` no está junto a `nsc.exe` ni en el `PATH`; no se declaran aprobados.
- `nsc_hir_tests`: `PASS 34/34`.
- `nsc_namespace_function_hir_tests`: `PASS 12/12`.
- El target focal y `nsc` compilaron con `/W4 /WX`; cero diagnósticos nuevos.
- El contrato de runtime estático de `nsc.exe` pasó.
- `git diff --check` se ejecuta como gate final antes del commit.

No se ejecutaron baseline, batería completa, deploy, instalación ni consumidores externos.

## Residuo H02 confirmado

La destrucción de nodos HIR ya no es una arista recursiva, independientemente de que el árbol haya
entrado al modelo. H02 continúa abierto por:

- el SCC OOP que conecta expression, statement/block, calls, construction, conversion, aggregate,
  await, destruction, exceptions y coroutines;
- recursión productiva en `hir::HirBuilder`, `hir::HirVerifier` y `hir::HirDumper`.

El siguiente corte debe romper el SCC OOP por familias con contrafactuales profundos y paridad HIR;
MIR, Driver y Parser siguen esperando una frontera HIR estable.
