# NSC 3.0.5 — R04C2: brazos statement `match` iterativos

## SCOPE LOCK

- **Repositorio/rama/revisión base:** `A:\nsc-devel`, `development`, `90d3da262`.
- **Objetivo exacto:** eliminar la reentrada del stack C++ entre un statement `match` y los
  statements estructurados contenidos en el bloque de cada brazo.
- **Naturaleza:** refactor interno no funcional del parser.
- **Último estado focal correcto:** `nsc_project_method_body_model_tests` pasa 92/92; R04A,
  R04B y R04C1 están cerrados.
- **Rango evaluado:** `90d3da262..worktree`.
- **Entrada exacta:** statements `match` anidados mediante bloques de brazo `pattern => { ... }`.
- **Operación exacta que falla estructuralmente:**
  `parseMatchStatement -> parseBlock -> parseStatement -> parseMatchStatement` conserva un frame
  del `match` exterior por nivel.
- **Invariante:** la profundidad de cuerpos statement `match` reside en la máquina heap-backed de
  controles estructurados; patrones, guards, lowering y orden fuente permanecen idénticos.
- **Consumidor afectado:** parser de statements y los modelos posteriores que consumen el bloque
  temprano producido por `match`.
- **Comportamiento esperado:** anidamientos profundos se parsean sin agotar el stack del host y
  producen el mismo lowering observable de 3.0.4.
- **Archivos de producto autorizados:** exclusivamente `src/Parser/Parser.cpp`.
- **Evidencia autorizada:** `tests/unit/ProjectMethodBodyModelTests.cpp`, este expediente y
  `A:\handover-deepseek\restantes-optimizacion-deepseek.md`.
- **Símbolos autorizados:** `Parser::StructuredControlParser` y el wrapper
  `Parser::parseMatchStatement`; helpers privados locales indispensables para conservar el
  contrato existente.
- **Archivos y subsistemas protegidos:** headers públicos, AST público, lexer, pattern grammar,
  expression `match`, semántica, HIR, MIR, LLVM, runtime, Driver, metadata, Builder, fixtures y
  expectativas existentes.
- **Contratos congelados:** sintaxis, aceptación/recovery, patterns, bindings, guards,
  exhaustividad, diagnostics y orden, evaluación única del governing value, temporales, metadata
  de coverage, AST lowering, ranges, ownership, API, ABI, NSMETA, CLI y artefactos 3.0.4.
- **Pruebas inmutables:** las 92 comprobaciones focales previas y las pruebas parser existentes;
  no se cambia ninguna expectativa existente.
- **Prohibido:** modificar recursión interna de patterns o expresiones (R02/R03), cambiar
  `parseMatchExpression`, ampliar a loop/comprehension, cleanup/exceptions, introducir guard de
  profundidad, baseline, suite completa, deploy, instalación, push, pull o fetch.
- **Paralelismo:** no aplica; el cursor y lowering conservan orden fuente determinista.
- **Configuración/perfil:** build existente `build-test-infrastructure`, prueba focal O0/host;
  ninguna reconfiguración.
- **Criterio exacto de terminación:** reproducción profunda específica pasa por continuaciones
  explícitas; las 92 pruebas anteriores permanecen verdes; `nsc` construye; el único rojo ajeno
  del target parser queda identificado sin atribuirlo a esta tanda; diff, límites y ausencia de
  reentrada quedan auditados; documentación, tracking y commit local aislado quedan completos.

## Presupuesto obligatorio de cambio

| Dimensión | Autorizado |
|---|---|
| Repositorio | `A:\nsc-devel` |
| Componente | parser de statement `match` |
| Producto | `src/Parser/Parser.cpp` |
| Prueba | `tests/unit/ProjectMethodBodyModelTests.cpp` |
| Documentación | este expediente y tracking externo |
| Configuración | `build-test-infrastructure` existente |
| Perfil | focal; sin baseline |
| Propiedad variable | uso del stack del host |
| Propiedades inmutables | todos los contratos enumerados arriba |

## Prueba contrafactual

`parseMatchStatement` participa directamente en la reproducción porque retiene `arms`, cobertura,
temporales y el lowering completo mientras `parseBlock` reentra al siguiente `match`. Los helpers
de pattern sólo procesan el header de cada brazo y no son necesarios para cortar esa arista; quedan
protegidos para R02.

## Diseño autorizado

La máquina estructural conservará un estado heap-backed por `match` y tres continuaciones:

1. consumir header y preparar el estado;
2. consumir un arm header y delegar su bloque a la máquina de bloques existente;
3. recibir el bloque, cerrar el arm y continuar o materializar el lowering existente.

La arquitectura compartida no cambia y no se introduce un componente nuevo; por ello este ítem no
crea un diagrama arquitectónico adicional. El flujo anterior es la extensión exacta de la máquina
documentada por R04B3/R04C1.

## Reproducción contrafactual

- La revisión base pasa **92/92** comprobaciones.
- Se agregó antes del cambio de producto una entrada con 1,024 statements `match` anidados,
  todos con patrón wildcard y un único bloque de brazo.
- El ejecutable focal de la revisión base termina en `0xC00000FD`
  (`-1073741571`) sin producir el conteo final.
- La entrada no ejercita patterns profundos, expression `match`, semántica, HIR, MIR ni LLVM; la
  profundidad se encuentra únicamente en la arista
  `match arm -> block -> statement match` autorizada.

## Implementación

1. `StructuredControlParser` reconoce el keyword contextual `match` mediante el mismo
   `startsParenthesizedBlock("match")` del dispatcher histórico.
2. `MatchParseState` conserva header, governing value, arm pendiente, arms terminados, estado de
   catch-all e intervalos ya cubiertos en almacenamiento heap-backed.
3. `ContinueMatch` consume un arm header; su bloque se agenda mediante la máquina iterativa de
   bloques y `FinishMatchArm` lo reincorpora sin retener un frame C++ del padre.
4. El wrapper `parseMatchStatement` sólo delega a la máquina compartida.
5. El lowering temprano se dividió en helpers cohesionados menores de 60 líneas. Conserva:
   - evaluación única del governing value;
   - nombres y orden de temporales;
   - selected flag;
   - orden de arms;
   - guard después de bindings;
   - metadata `match-coverage:v1`;
   - extracción de payload y `lowerPattern` existentes;
   - diagnostics de catch-all, guard falso, intervalos y body ausente;
   - ranges y recovery, incluido sincronizar antes de cerrar el range de un body ausente.
6. Los recorridos locales que la función anterior expresaba mediante lambdas recursivas
   (`collectBindings`, catch-all, validación OR, intervalos y coverage) usan worklists explícitas.
   No se cambió `parsePattern`, `lowerPattern`, la gramática de patterns ni expression `match`;
   esos frentes permanecen bajo R02/R03.

## Complejidad y austeridad

- El recorrido de cada arm y de sus patterns locales es lineal en el número de nodos visitados.
- Los sorts de bindings/cases/longitudes conservan la complejidad histórica `O(n log n)`.
- La profundidad sintáctica de arms y bloques consume heap proporcional a la profundidad, no
  stack del host.
- No se agregaron locks, paralelismo, I/O, logging, excepciones, guards de profundidad ni cambios
  públicos.
- La clase compartida continúa físicamente dentro del monolito `Parser.cpp`; su separación es el
  ítem R01 ya rastreado y no se mezcló con esta hipótesis causal.

## Validación focal

- Construcción `nsc_project_method_body_model_tests`, `nsc_parser_tests` y `nsc`: PASS con
  warnings tratados como error.
- Contrato estático de host runtime de `nsc.exe`: PASS.
- Reproducción profunda después del cambio: PASS.
- Foco acumulado: **PASS 95/95**.
  - conserva las 92 comprobaciones anteriores;
  - agrega 1,024 `match` anidados;
  - agrega forma observable del lowering: `$__nsc_match_0`, coverage `A;*` y orden `First` antes
    de `Second`.
  - agrega orden y texto exactos para arm posterior a catch-all, match vacío y body ausente.
- `nsc.parser`: 22/23; conserva como único rojo la expectativa ajena
  `invalid ternary conditional syntax is rejected by NativeSharp 1.0.1`. Esa entrada no contiene
  `match` ni alcanza las rutas modificadas; no se contó como PASS.
- Cinco targets T03 relacionados construyen, pero sus ejecuciones no son verdes y no se usaron
  como evidencia de cierre:
  - tagged variants, exhaustive match y pattern guards quedan bloqueados al no poder abrir la
    biblioteca runtime instalada indicada por sus propios fixtures;
  - fundamental patterns y conditional patterns alcanzan semántica y fallan en asignaciones a
    bindings `ref`;
  - ninguno reportó un diagnóstico de parsing R04C2, pero esto no equivale a aprobar esos targets.
- `git diff --check`: PASS; las líneas agregadas respetan 120 columnas.
- Análisis estático completo: no verificado porque el build congelado no expone una base de
  compilación compatible para `clang-tidy`; `/W4 /WX` sí pasó en los archivos afectados.

No se ejecutaron baseline, minibaseline, suite completa, deploy, instalación, push, pull ni fetch.

## Cierre

R04C2 queda terminado. R04C permanece abierto para R04C3, que debe clasificar
loop/comprehension frente a R03 sin convertir una frontera de expressions en trabajo de statements.
