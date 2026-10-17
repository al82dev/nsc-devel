# TANDA NSC305 H02S16C - expression statements HIR iterativos

## Estado de este expediente

Este documento registra el cambio aislado H02S16C. El cierre de implementación
es exclusivamente estático: se inspeccionó el diff y `git diff --check` terminó
sin errores. No se ejecutaron compilaciones ni pruebas por instrucción expresa
de la tanda; la validación integrada permanece aplazada.

## Scope lock

H02S16C migra únicamente la arista AST a HIR de un `ExpressionStatement`: la
continuación que espera el resultado de su expresión deja de depender de la
llamada anidada que vivía en el `switch` de statements y pasa a un frame explícito
de la máquina de expresiones.

Quedan fuera de alcance:

- sintaxis, parser y semántica del lenguaje;
- resolución de nombres, tipos, overloads o genéricos;
- representación pública de AST, HIR, MIR, ABI o NSMETA;
- lowering de otras familias de statements o expresiones;
- driver, publicación de artefactos, CMake y Builder 3;
- cambios en diagnósticos, rangos, cleanup o política `noheap`;
- optimizaciones que cambien la forma observable del HIR.

## Implementación histórica auditada

La ruta histórica construía un nodo HIR `ExpressionStatement`, bajaba
`statement->first()` mediante `lowerExpression`, abortaba si el hijo no podía
producirse y, en caso de éxito, agregaba exactamente ese hijo al nodo `void`.
La llamada desde el `switch` de statements mantenía implícitamente en la pila C++
la continuación necesaria para envolver el resultado.

La forma contractual del resultado era y continúa siendo:

1. clase de nodo `ProjectOopHirNodeKind::ExpressionStatement`;
2. sintaxis asociada al statement completo;
3. tipo `builtin:void`;
4. exactamente un hijo, la expresión ya bajada;
5. orden del hijo idéntico al orden fuente;
6. propagación inmediata de `nullptr` y del diagnóstico de la expresión.

## Diseño H02S16C

`ProjectOopHirModel::lowerExpressionStatement` delega en la misma máquina de
expresiones usada por el lowering normal y proporciona el statement que debe
envolver el resultado. La máquina apila como frame inferior
`IterativeExpressionKind::ExpressionStatement`; después procesa la expresión y,
al desenrollar ese frame, crea el nodo `void` con su único hijo.

El `switch` de statements ya no conserva una continuación AST a HIR propia:
realiza una delegación de cola al helper. Así, la arista statement a expression y
su retorno a statement quedan modelados por estado explícito y no por otro nivel
de pila nativa.

## Invariantes preservadas

- El hijo se baja antes de construir el wrapper observable.
- El wrapper conserva como sintaxis el statement, no la expresión.
- La expresión conserva su propio rango y su tipo inferido.
- Un error del hijo se propaga sin sustituir su texto, código o rango.
- Una expresión ausente mantiene el comportamiento histórico de fallo; el frame
  sintético no inventa un rango para ese caso.
- Los checks de efectos y `noheap` se aplican sobre la expresión bajada, nunca
  sobre el wrapper `ExpressionStatement`.
- El wrapper no agrega cleanup, temporales, conversiones ni control de flujo.
- El orden HIR y la cardinalidad de hijos no cambian.

## Salvaguarda para expresiones no iterativas

La máquina conserva fallbacks históricos para clases de expresión que todavía no
pertenecen a una familia migrada. Cuando la raíz de un expression statement entra
por ese fallback, debe invocarse una sola vez `lowerExpressionImpl` y dejar que la
máquina exterior aplique una sola vez las comprobaciones comunes. Volver a llamar
al wrapper público `lowerExpression` desde esa raíz duplicaría las comprobaciones
de efectos y `noheap`; por ello esa condición forma parte del gate estático antes
del cierre.

Los fallbacks anidados que no son la raíz de este wrapper conservan su llamada
histórica a `lowerExpression`, porque pertenecen a la semántica de la familia que
los contiene y no a H02S16C.

## Complejidad y uso de pila

H02S16C agrega un frame de tamaño constante y una operación de envoltura por
statement. Su costo adicional es O(1) en tiempo y memoria. La profundidad de la
expresión sigue gobernada por las familias ya migradas o pendientes, pero esta
arista concreta ya no consume un frame C++ adicional al retornar de la expresión.

## Archivos autorizados

- `include/nativesharp/Frontend/ProjectOopHirModel.h`
- `src/Frontend/HIR/HirExpressionMachine.cpp`
- `src/Frontend/HIR/HirStatementMachine.cpp`
- este expediente

No se requiere alta de fuente, target ni caso nuevo: todos los cambios de código
residen en unidades ya declaradas por el proyecto.

## Gates estáticos ejecutados antes del commit

- Diff completo de los tres archivos HIR: inspeccionado.
- Frame de statement: una producción y un consumo en la máquina.
- Caso statement: delegación de cola al helper nuevo.
- Fallback raíz: llamada directa única a `lowerExpressionImpl`.
- Checks `noheap`: operan sobre el hijo, no sobre el wrapper.
- Contratos públicos o serializados: sin cambios.
- CMake, Driver, MIR, tests y tracking: fuera del diff H02S16C.
- `git diff --check`: sin errores.
- Comentarios de helpers y expediente: presentes.
- Commit: debe contener exclusivamente los tres archivos HIR y este expediente.

## Validación aplazada

Por decisión de la tanda no se compila ni se ejecutan pruebas durante esta
implementación. La validación dinámica corresponde a la corrida consolidada del
usuario después de barrer los pendientes. Hasta entonces este expediente no debe
interpretarse como evidencia de build, ejecución o paridad dinámica.
