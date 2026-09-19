# NativeSharp 2.0 self-hosting — tanda 08: HIR, MIR, CFG, dominancia y SSA

## 1. Identidad

| Campo | Valor |
|---|---|
| Estado | `LANDED` |
| Tareas | `NS2-SH-084` a `NS2-SH-098` |
| Gate | `SH07` |
| Rama | `development` |
| Commit de entrada | `e3bfbb3c8` |
| Commit de cierre | este commit (`selfhost(tanda-08): port hir mir cfg dominance and ssa`) |
| Fecha de apertura | 2026-09-19 |
| Fecha de cierre | 2026-09-19 |
| Baseline de cierre | `tanda-08-sh07-final-r2-20260919`, PASS, 667.035 s |

## 2. Objetivo y alcance

### Objetivo

Crear dentro de `nsc-next`, sin cargar ni invocar código C++, las fronteras intermedias que separan
el frontend semántico del backend: HIR, MIR, CFG, dominancia y SSA. Cada frontera debe tener forma
determinista, verificación antes de ser consumida y un dump versionado que permita comparar por
función el compilador NativeSharp con `nsc0`.

### Incluido

- HIR `schema=1` con identidad estable de sintaxis, símbolo y tipo;
- conservación observable de operadores, conversiones semánticas y bounds checks;
- lowering HIR validado a MIR tipado, con operaciones explícitas de memoria y runtime;
- bloques básicos, terminadores, aristas tipadas, código inalcanzable y short-circuit;
- construcción iterativa de conjuntos de dominadores e `idom`;
- cálculo de dominance frontiers sobre joins del CFG;
- clasificación de locales promovibles y exclusión de almacenamiento cuya dirección escapa;
- inserción de `phi` en joins y dump SSA determinista;
- verificación HIR, MIR/CFG, dominancia y ownership/cleanup explícito;
- perfil ordenado de pases de corrección y optimización O0/O1/O2/O3;
- comparación estructural contra `nsc0`, estabilidad entre los cuatro niveles y reducción `ddmin`
  automática del primer conjunto de funciones divergente;
- gate CMake/CTest, contrato JSON, reporte JSON y pruebas de infraestructura.

### Frontera deliberada

SH07 congela representación, flujo, análisis y orden de pases; no emite LLVM ni artefactos. El
backend de SH08 consumirá este contrato. El dump nuevo no intenta ser byte a byte igual al dump
histórico, porque añade `schema=1`, IDs de sintaxis y datos que el formato legado no expresa. La
comparación normativa es por funciones, forma de control e invariantes de cada frontera.

El MIR bootstrap conserva el ID del nodo que produce cada valor (`%sN`) y su tipo, pero todavía no
es el dialecto LLVM. SSA muestra promoción y `phi` por join; la selección mínima y el renombrado
final de operandos serán endurecidos al conectar la emisión LLVM, sin cambiar el contrato de CFG,
dominancia o exclusión de variables direccionadas.

## 3. Arquitectura

```text
SyntaxTree congelado + SemanticModel SH06
             |
             v
        HirBuilder
             |
        HirVerifier
             |
             v
        MirBuilder  <--- HIR validado + sidecar estable de sintaxis/semántica
             |
   FlowVerifier + OwnershipCleanupVerifier
             |
       +-----+------------------+
       |                        |
       v                        v
   CFG dump              DominanceBuilder
                               |
                         idom + frontiers
                               |
                               v
                         promoción / SSA
                               |
                     perfiles O0/O1/O2/O3
```

HIR es un preorder denso. Esta elección permite serializar sin punteros, comprobar que toda
referencia vuelve al AST congelado y mantener el mismo resultado en cuatro binarios optimizados de
forma distinta. MIR materializa funciones y bloques; el sidecar sólo se consulta mediante IDs, no
se muta ni se vuelve a interpretar texto fuente.

CFG comparte los bloques MIR: una arista identifica origen, destino y clase (`jump`, `true`,
`false`, `back`, `break`, `continue`). El algoritmo de dominancia empieza con el conjunto universal
para bloques distintos de la entrada, intersecta dominadores de predecesores hasta punto fijo y
elige como `idom` el dominador estricto de mayor profundidad. Los frontiers se obtienen caminando
desde cada predecesor de un join hasta el `idom` del join.

SSA considera promovible un parámetro o local si su dirección no fue tomada. Cada join recibe una
entrada `phi` determinista por local promovible; almacenamiento direccionado permanece explícito.
El dumper hace visible esta decisión, lo que permite al backend rechazar una promoción insegura.

## 4. Contratos e invariantes

- ningún IR contiene direcciones de proceso ni orden de hash;
- cada registro HIR tiene ID de sintaxis válido, kind no vacío y profundidad no negativa;
- el primer registro es `CompilationUnit` a profundidad cero;
- MIR sólo se construye después de aceptar HIR;
- toda función definida tiene bloque de entrada y todos sus bloques terminan;
- toda arista apunta a un bloque de la misma función;
- `&&` y `||` producen bloques `logic.rhs`, `logic.short` y `logic.end`;
- código posterior a un terminador vive en un bloque marcado inalcanzable;
- operaciones `new`, `delete`, `delete[]`, `stackalloc`, index y bounds check no quedan como nodos
  genéricos sin bajar;
- entrada domina todos los bloques alcanzables y su `idom` es `<root>`;
- un `phi` sólo aparece en un bloque con al menos dos predecesores;
- un local cuya dirección fue tomada nunca aparece en la lista `promoted`;
- los dumps de O0/O1/O2/O3 son idénticos para la misma fuente;
- toda unidad que `nsc0` acepta aisladamente hasta SSA debe ser aceptada por los cuatro candidatos;
- unidades que requieren proyecto/referencias pueden fallar aisladas, pero su resultado debe ser
  estable entre los cuatro candidatos;
- el primer desacuerdo de funciones genera `sh07-first-divergence.ns` y metadata JSON bajo el
  directorio generado del gate, nunca dentro del árbol fuente.

## 5. Inventario de archivos

| Archivo/familia | Acción | Responsabilidad |
|---|---|---|
| `selfhost/nsc/IR/IntermediateRepresentation.ns` | crear | modelos, builders, verificadores, análisis y dumps SH07 |
| `selfhost/nsc/Driver/Program.ns` | modificar | opciones dump/verify e identidad SH07 |
| `selfhost/nsc/NativeSharp.Compiler.nsproj` | modificar | incluir el núcleo IR |
| `selfhost/contracts/ir.contract.json` | crear | stages, corpus, perfiles e invariantes normativos |
| `selfhost/contracts/README.md` | modificar | registrar el contrato SH07 |
| `selfhost/bootstrap/verify_ir.py` | crear | diferencial estructural, estabilidad, corpus y reducer |
| `selfhost/bootstrap/README.md` | modificar | documentar verificador/reductor SH07 |
| `selfhost/bootstrap/verify_nsc_next_skeleton.py` | modificar | avanzar identidad contractual de SH06 a SH07 |
| `cmake/VerifyNativeSharpIr.cmake` | crear | construir/verificar candidatos O0/O1/O2/O3 |
| `CMakeLists.txt` | modificar | registrar `nsc.selfhost.ir` serial |
| `tests/selfhost/test_bootstrap_infrastructure.py` | modificar | congelar schema, niveles, corpus, LF y reducer |
| `selfhost/README.md` | modificar | declarar la frontera alcanzada |
| `selfhost/nsc/README.md` | modificar | documentar el subsistema IR |
| `docs/2.0/SELF_HOSTING_ROADMAP.md` | modificar | enlazar expediente, contrato y gate SH07 |
| este expediente | crear | diseño, trazabilidad, baseline y deuda explícita |

## 6. Tipos y ownership

| Tipo | Propiedad | Invariantes de vida |
|---|---|---|
| `HirRecord` | propiedad de `HirModule` | strings copiados; IDs densos, sin punteros al AST |
| `HirModule` | resultado propietario de `HirBuilder` | lista preorder y contador de errores |
| `FlowEdge` | propiedad de `FlowFunction` | origen/destino locales a la función |
| `FlowBlock` | propiedad de `FlowFunction` | operaciones propias; exactamente un terminador lógico |
| `FlowFunction` | propiedad de `FlowModule` | bloques, aristas, locales y locals direccionados |
| `FlowModule` | resultado propietario del lowering MIR | no sobrevive a su semantic sidecar |
| `DominanceInfo` | temporal por función | arrays propios `idom` y matriz de dominadores |
| strings de dump | retorno `owned` | llamador responsable; LF canónico |

La estructura evita ciclos propietarios. Un bloque no posee sus predecesores/sucesores; las
relaciones viven como IDs dentro de aristas propiedad de la función. Dominancia tampoco guarda
punteros a CFG, sólo índices. Esta forma es serializable y evita que una arena de una etapa mantenga
viva accidentalmente la etapa anterior.

## 7. Métodos y funciones

| Firma/grupo | Responsabilidad | Postcondición / error |
|---|---|---|
| `HirBuilder.LowerKind` | normalizar kinds sintácticos a HIR | memoria/control/conversiones reciben kind explícito |
| `HirBuilder.Visit` | bajar preorder con hechos SH06 | conserva sintaxis, tipo, símbolo, op, conversiones y bounds |
| `HirBuilder.Build` | producir módulo HIR propietario | root visitada una vez |
| `HirVerifier.Verify` | validar forma y referencias básicas | rechaza root/kind/ID/profundidad inválidos |
| `FlowFunction.AddBlock` | crear bloque y label estable | ID igual al índice; serial monotónico |
| `FlowFunction.AddEdge` | registrar control tipado | verifier comprueba ambos extremos |
| `FlowFunction.MarkAddressed` | excluir storage de promoción | conjunto sin duplicados |
| `MirBuilder.Describe` | bajar nodo a op MIR tipada | incluye `%sN`, tipo, sintaxis y símbolo |
| `MirBuilder.EmitExpression` | postorder de valores | short-circuit crea CFG y `phi.logic` |
| `MirBuilder.LowerBlock` | bajar statements y terminadores | conserva destinos break/continue y reachability |
| `MirBuilder.Build` | HIR validado → módulo MIR/flow | nunca acepta HIR inválido |
| `FlowVerifier.Verify` | validar bloques/aristas/terminación | ningún bloque abierto ni edge foránea |
| `OwnershipCleanupVerifier.Verify` | exigir memoria/cleanup explícito | rechaza delete/stackalloc sin lowering |
| `DominanceBuilder.Build` | punto fijo, profundidad e `idom` | una entrada root por función |
| `IrDumper.Hir` | serializar HIR schema 1 | IDs, tipos y conversiones visibles |
| `IrDumper.Mir` | serializar MIR schema 1 | funciones/bloques/ops tipadas estables |
| `IrDumper.Cfg` | serializar pred/succ | cardinalidad y clases de arista visibles |
| `IrDumper.Dominance` | serializar idom/dom/frontier | análisis recalculado, no cache textual |
| `IrDumper.Ssa` | serializar promoción y phi | omite locals direccionados |
| `IrDumper.PassPipeline` | congelar orden O0–O3 | corrección siempre precede optimización |
| `IrPipeline.Dump/Verify` | fachada obligatoria de etapas | ninguna etapa salta verificadores previos |
| `verify_ir.function_names` | comparar unidad por función | entiende formato legado y schema 1 |
| `verify_ir.source_corpus` | enumerar corpus reproducible | excluye outputs/evidencia/cachés |
| `verify_ir.reduce_lines` | ddmin determinista | conserva el predicado de divergencia |
| `materialize_first_divergence` | guardar primer reproducer | escribe sólo bajo output generado |
| `verify_ir.verify` | gate integral SH07 | reporte sólo si todos los checks pasan |

## 8. Trazabilidad `NS2-SH-084` a `NS2-SH-098`

| ID | Implementación / evidencia |
|---|---|
| 084 | cabecera `hir.module schema=1`, preorder e IDs deterministas |
| 085 | `HirBuilder` consume `SyntaxTree` + `SemanticModel`, sin frontend C++ |
| 086 | `HirVerifier` obligatorio en dump y verify |
| 087 | cabecera `mir.module schema=1`, `%sN:type`, bloques y operaciones |
| 088 | `MirBuilder.Build` exige HIR aceptado y conserva sidecar por ID |
| 089 | alloc/free/stackalloc/index/bounds/load/store/call/control explícitos |
| 090 | `FlowVerifier`, operaciones tipadas y bloques terminados |
| 091 | bloques/aristas reales; caso canónico reproduce 12 nodos/13 aristas |
| 092 | punto fijo de dominadores, `idom`, conjuntos `dom` y frontiers |
| 093 | promoted set, exclusión address-taken, joins y phi |
| 094 | verificación SSA por joins más `OwnershipCleanupVerifier` |
| 095 | O0 antepone todos los verificadores y análisis requeridos para corrección |
| 096 | perfiles acumulativos O0/O1/O2/O3 y orden congelado en contrato |
| 097 | 20 comparaciones de funciones contra dumps reales de `nsc0` |
| 098 | reducer lineal `ddmin`, reproducer y metadata del primer desacuerdo |

## 9. Superficie observable

```text
--dump-hir             --verify-hir
--dump-mir             --verify-mir
--dump-cfg             --verify-cfg
--dump-dominance       --verify-dominance
--dump-ssa             --verify-ssa
--dump-pass-pipeline
```

Todos los dumps terminan en LF. Los verificadores exitosos no escriben stdout/stderr y devuelven
cero. Una etapa inválida devuelve uno. Error de uso devuelve dos. La identidad publicada pasa a
`nsc-next 2.0.0-bootstrap.sh07`.

## 10. Perfiles de pases

| Nivel | Cadena acumulativa |
|---|---|
| O0 | verify-hir → lower-mir → verify-mir → build/verify-cfg → dominance/verify → ssa/verify |
| O1 | O0 + phi simplification + copy propagation + DCE + simplify CFG |
| O2 | O1 + constant propagation + local CSE + SCCP + loop simplify |
| O3 | O2 + inlining + loop rotation + unroll + vectorization + global DCE |

Este orden es una dependencia, no una sugerencia: O1 nunca puede ejecutarse sin SSA verificado;
los pases de loops de O2/O3 requieren CFG/dominancia válidos; cualquier pase que invalide control
obliga a reconstruir sus análisis. SH08 conectará estas decisiones al pipeline LLVM conservando
la misma jerarquía observable.

## 11. Comparación y corpus

El formato legado carece de schema e IDs, por lo que el gate no compara bytes entre generaciones.
Para cada frontera obtiene el dump de `nsc0`, comprueba su cabecera histórica y compara el conjunto
exacto de funciones con cada candidato. Después exige los invariantes propios de la etapa.

La estabilidad sí es byte a byte entre candidatos: O0 es referencia y O1/O2/O3 deben producir
exactamente el mismo exit code, stdout y stderr. La matriz canónica cruza seis fuentes por cinco
fronteras. Adicionalmente se enumeran todas las fuentes `.ns`: las unidades standalone aceptadas
por `nsc0 --verify-ssa` deben llegar a SSA en los cuatro candidatos; las que necesitan contexto de
proyecto se usan para estabilidad y ausencia de crash.

## 12. Pruebas específicas previas al baseline

| Prueba | Propósito | Resultado actual |
|---|---|---|
| build O0/O1/O2/O3 | compilar nsc-next desde NativeSharp | PASS ×4 |
| `--verify-pe` | validar cada candidato | PASS ×4 |
| dumps canónicos | HIR/MIR/CFG/dominance/SSA/passes | PASS ×6 ×4 |
| diferencial por función | mismo inventario que `nsc0` | PASS: 20 |
| verificadores | cada frontera en cada nivel | PASS: 24 |
| invariantes | memoria, control, dominancia, SSA y pases | PASS: 118 |
| matriz cruzada | seis fuentes, cinco etapas, cuatro niveles | PASS: 108 comparaciones |
| corpus fuente | estabilidad SSA O0/O1/O2/O3 | PASS: 484 fuentes, 1,452 comparaciones |
| corpus standalone `nsc0` | toda unidad oracle llega a SSA | PASS: 22/22 ×4 |
| infraestructura Python | schema, LF, corpus y reducer | PASS: 23/23 |

## 13. Baseline de cierre

El baseline canónico se ejecutó después del último cambio funcional y documental:

```text
python -B nscbuilder.py --baseline \
  --config selfhost/bootstrap/bootstrap.config.example.json \
  --run-id tanda-08-sh07-final-r2-20260919 --jobs 16
```

| Dato | Resultado |
|---|---|
| Estado | PASS, 0 fallos; Gate 0 `LANDED` |
| Inicio / fin | 2026-09-19 16:55:54 / 17:07:01, UTC-06:00 |
| Duración total | 667.035 s |
| CTest `lld-link` | 444/444 PASS, 276.899 s |
| Fixtures `lld-link` | 148/148 PASS, 52.051 s |
| Triage `lld-link` | 18/18 PASS, 8.896 s |
| CTest NSLink | 444/444 PASS, 237.862 s |
| Fixtures NSLink | 148/148 PASS, 45.508 s |
| Triage NSLink | 18/18 PASS, 9.226 s |
| Gate 0 | LANDED, 7.566 s |
| Fuente auditada | 1,406 archivos; SHA-256 `e0b81639de915426ae57dca719776eb4de9d21fe3fd2495ad2966f93d127a19c` |
| `nsc0` construido/instalado | SHA-256 `d2eeb7abcadb802a1bf2802a0fef148d62f666bfa6e53ee70ec02b5c288f6561` |
| Reporte Markdown | SHA-256 `9c49fb5242f6603fb1412db52057b48eca23fba81cab27229d4436145f7585c2` |
| Reporte JSON | SHA-256 `28dd0f76bd4f8aecd1f76714d9118a1ad5aa9990b0add22e8daf1371e554c4d9` |
| Gate G00 | SHA-256 `f76e99178d8072071b27c52ed2358923d0168173f7cf5dfa35dc057d1af13bc8` |
| Reporte SH07 promovido | SHA-256 `1c51ed9dde2e5bd0ef1381bd40973544920899b5434e1d45f41261985fe196c4` |
| Candidato O0 | SHA-256 `2bc316d238821dec13362a3dc1f1ea573a52724f73f20c6f25f28bb9861f7442` |
| Candidato O1 | SHA-256 `bdea6a954d65c8c02593d9401784a686f67b8e4da3e9219fa7d46976e2dc82b1` |
| Candidato O2 | SHA-256 `56ffee418ee891a10e7e7f646cdcc1ff69314724bdcd9b66866c00e5c2b61a0a` |
| Candidato O3 | SHA-256 `b13877520d19833bd38405e5895480f0d7b2dbacc2bb565d2004c9791709ca82` |

La primera corrida, `tanda-08-sh07-final-20260919`, falló correctamente porque el gate heredado
SH04 aún esperaba literalmente la identidad `bootstrap.sh06`. La expectativa avanzó a SH07, el
gate aislado quedó verde y la misma corrida detectó además cambio S0→S1 porque esa corrección se
hizo mientras estaba activa. No se promovió. Sólo la segunda corrida íntegra, con árbol inmóvil,
ambos linkers verdes y Gate 0 `LANDED`, constituye evidencia de cierre.

## 14. Riesgos y deuda explícita

- Los dumps schema 1 son comparables estructuralmente, no textualmente, con el formato legado.
- Las fuentes multiarchivo que requieren `.nsproj` no son unidades standalone; SH12/SH13
  ejecutarán la misma frontera con project graph y metadata importada.
- El `phi` bootstrap es conservador: puede colocar más phis que el mínimo. Es correcto para flujo,
  pero SH08 endurecerá def-use/rename al producir LLVM.
- Excepciones y cleanup no local se conservan como kinds/operaciones; unwind físico es SH08.
- Los perfiles O0–O3 congelan orden y dependencias de IR. La traducción a `PassBuilder` LLVM,
  LTO y ThinLTO pertenece explícitamente a `NS2-SH-108`.
- `nsc0` continúa como oráculo temporal hasta SH15.

## 15. Higiene y contenido del commit

- [x] no se escribió ni instaló nada en `C:`;
- [x] candidatos y reportes temporales viven bajo `selfhost/out` ignorado;
- [x] no se versionan ejecutables, objetos, librerías, PDB ni caches;
- [x] no se preparan las seis rutas locales ajenas identificadas al abrir la tanda;
- [x] diseño, tipos, métodos, funciones, decisiones y deuda están documentados;
- [x] baseline completo posterior al último cambio;
- [x] evidencia SH07 promovida con hashes finales;
- [x] commit exacto creado por esta tanda.

Commit previsto:

```text
selfhost(tanda-08): port hir mir cfg dominance and ssa
```

## 16. Criterio de cierre

- [x] HIR, MIR, CFG, dominancia y SSA se construyen dentro de `nsc-next`;
- [x] ninguna frontera puede saltar su verificador;
- [x] cuatro niveles producen dumps estables;
- [x] corpus standalone del oráculo llega a SSA y corpus completo no diverge entre niveles;
- [x] primera divergencia puede reducirse automáticamente;
- [x] baseline completo verde;
- [x] gate SH07 promovido;
- [x] commit exacto creado por esta tanda.
